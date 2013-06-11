/// \file statement_tree.cpp
///

#include "statement_tree.h"
#include "node_analysis.h"
#include "term_rewriting.h"
#include "ir/statements.h"

using namespace ir;
using namespace na;
using namespace tr;

namespace st {

void StmtVertex::expandIf(const If& _if) {
    if (_if.getBody())
        appendChild(new StmtVertex(_if.getBody()));
    if (_if.getElse())
        appendChild(new StmtVertex(_if.getElse()));
}

void StmtVertex::expandBlock(const Block& _block) {
    for (size_t i = 0; i < _block.size(); ++i)
        if (const StatementPtr& pStmt = _block.get(i))
            appendChild(new StmtVertex(pStmt));
}

void StmtVertex::expandSwitch(const Switch& _switch) {
    for (size_t i = 0; i < _switch.size(); ++i)
        if (const StatementPtr pStmt = _switch.get(i)->getBody())
            appendChild(new StmtVertex(pStmt));
    if (_switch.getDefault())
        appendChild(new StmtVertex(_switch.getDefault()));
}

void StmtVertex::expandVariableDeclarationGroup(const VariableDeclarationGroup& _vdg) {
    for (size_t i = 0; i < _vdg.size(); ++i)
        if (const VariableDeclarationPtr pDecl = _vdg.get(i))
            appendChild(new StmtVertex(pDecl));
}

void StmtVertex::expand() {
    if (!m_pStmt || !m_children.empty())
        return;

    switch (m_pStmt->getKind()) {
        case Statement::BLOCK:
        case Statement::PARALLEL_BLOCK:
            expandBlock(*m_pStmt.as<Block>());
            break;
        case Statement::IF:
            expandIf(*m_pStmt.as<If>());
            break;
        case Statement::SWITCH:
            expandSwitch(*m_pStmt.as<Switch>());
            break;
        case Statement::VARIABLE_DECLARATION_GROUP:
            expandVariableDeclarationGroup(*m_pStmt.as<VariableDeclarationGroup>());
            break;
    }

    for (std::list<Auto<StmtVertex> >::iterator i = m_children.begin();
            i != m_children.end(); ++i)
        (*i)->expand();
}

void StmtVertex::modifyIf(const If& _if) {
    if (!_if.getArg() || !containsCall(_if.getArg()))
        return;

    std::pair<NodePtr, NodePtr> newArg = extractFirstCall(*_if.getArg());
    if (!newArg.first)
        return;

    StmtVertexPtr newVertex = new StmtVertex(new If(newArg.second.as<Expression>()));
    newVertex->m_children.swap(this->m_children);

    this->appendChild(new StmtVertex(newArg.first.as<Call>()));
    this->appendChild(newVertex);

    m_pStmt = new Block();
}

void StmtVertex::modifyCall(const Call& _call) {
    assert(m_children.empty());

    std::pair<NodePtr, NodePtr> newCall = extractFirstCall(_call);
    if (!newCall.first)
        return;

    appendChild(new StmtVertex(newCall.first.as<Call>()));
    appendChild(new StmtVertex(newCall.second.as<Call>()));

    m_pStmt = new Block();
}

static ExpressionPtr _getEquality(const ExpressionPtr& _pArg, const ExpressionPtr& _pAlternative) {
    if (_pAlternative->getKind() != Expression::TYPE)
        return new Binary(Binary::EQUALS, _pArg, _pAlternative);

    const TypeExpr& type = (const TypeExpr&)*_pAlternative;
    if (type.getContents()->getKind() != Type::RANGE && type.getContents()->getKind() != Type::SUBTYPE)
        return new Literal(true);

    SubtypePtr pSubtype = type.getContents()->getKind() == Type::RANGE
        ? type.getContents().as<Range>()->asSubtype()
        : type.getContents().as<Subtype>();

    if (!pSubtype)
        return new Literal(true);

    ExpressionPtr pExpr = clone(pSubtype->getExpression());
    pExpr = Expression::substitute(pExpr, new VariableReference(pSubtype->getParam()), _pArg).as<Expression>();
    return pExpr;
}

static ExpressionPtr _getCondition(const ExpressionPtr _pArg, const Collection<Expression>& _alternative) {
    if (_alternative.empty())
        return new Literal(false);
    if (_alternative.size() == 1)
        return _getEquality(_pArg, _alternative.get(0));

    BinaryPtr
        pCondition = new Binary(Binary::BOOL_OR, _getEquality(_pArg, _alternative.get(0))),
        pCurrent = pCondition;
    for (size_t i = 1; i < _alternative.size(); ++i) {
        if (i + 1 == _alternative.size()) {
            pCurrent->setRightSide(_getEquality(_pArg, _alternative.get(i)));
            break;
        }

        pCurrent->setRightSide(new Binary(Binary::BOOL_OR, _getEquality(_pArg, _alternative.get(i))));
        pCurrent = pCurrent->getRightSide().as<Binary>();
    }

    return pCondition;
}

void StmtVertex::modifySwitch(const Switch& _switch) {
    if (_switch.empty() && !_switch.getDefault())
        return;
    if (_switch.empty()) {
        m_pStmt = _switch.getDefault();
        return;
    }

    std::list<StmtVertexPtr> children;
    children.swap(this->m_children);

    StmtVertexPtr
        pParentVertex = this,
        pNewVertex = this;

    this->appendChild(children.front());

    std::list<StmtVertexPtr>::iterator j = ++(children.begin());
    for (size_t i = 1; i < _switch.size(); ++i, ++j) {
        pNewVertex = new StmtVertex(new If(_getCondition(_switch.getArg(), _switch.get(i)->getExpressions())));
        pNewVertex->appendChild(*j);
        pParentVertex->appendChild(pNewVertex);
        pParentVertex = pNewVertex;
    }

    if (_switch.getDefault())
        pNewVertex->appendChild(new StmtVertex(_switch.getDefault()));

    this->m_pStmt = new If(_getCondition(_switch.getArg(), _switch.get(0)->getExpressions()));
}

void StmtVertex::modifyAssignment(const Assignment& _assignment) {
    assert(m_children.empty());

    std::pair<NodePtr, NodePtr> newAssignment = extractFirstCall(_assignment);
    if (!newAssignment.first)
        return;

    appendChild(new StmtVertex(newAssignment.first.as<Call>()));
    appendChild(new StmtVertex(newAssignment.second.as<Assignment>()));
    m_pStmt = new Block();
}

void StmtVertex::modifyMultiAssignment(const Multiassignment& _massignment) {
    assert(m_children.empty());

    if (_massignment.getLValues().empty())
        return;
    if (_massignment.getLValues().size() == 1) {
        AssignmentPtr pAssignment = new Assignment(_massignment.getLValues().get(0), _massignment.getExpressions().get(0));
        appendChild(new StmtVertex(pAssignment));
        m_pStmt = new ParallelBlock();
        return;
    }

    for(size_t i = 0; i < _massignment.getLValues().size(); ++i) {
        AssignmentPtr pAssignment = new Assignment(_massignment.getLValues().get(i), _massignment.getExpressions().get(i));
        appendChild(new StmtVertex(pAssignment));
    }
    m_pStmt = new ParallelBlock();
}

void StmtVertex::modifyVariableDeclaration(const VariableDeclaration& _decl) {
    assert(m_children.empty());
    if (!_decl.getValue())
        return;
    AssignmentPtr pAssignment = new Assignment(new VariableReference(_decl.getVariable()), _decl.getValue());
    appendChild(new StmtVertex(pAssignment));
}

void StmtVertex::modifyVariableDeclarationGroup(const VariableDeclarationGroup& _vdg) {
    m_pStmt = new Block();
}

void StmtVertex::modifyForVerification() {
    if (!m_pStmt)
        return;

    switch(m_pStmt->getKind()) {
        case Statement::IF:
            modifyIf(*m_pStmt.as<If>());
            break;
        case Statement::CALL:
            modifyCall(*m_pStmt.as<Call>());
            break;
        case Statement::SWITCH:
            modifySwitch(*m_pStmt.as<Switch>());
            break;
        case Statement::ASSIGNMENT:
            modifyAssignment(*m_pStmt.as<Assignment>());
            break;
        case Statement::MULTIASSIGNMENT:
            modifyMultiAssignment(*m_pStmt.as<Multiassignment>());
            break;
        case Statement::VARIABLE_DECLARATION:
            modifyVariableDeclaration(*m_pStmt.as<VariableDeclaration>());
            break;
        case Statement::VARIABLE_DECLARATION_GROUP:
            modifyVariableDeclarationGroup(*m_pStmt.as<VariableDeclarationGroup>());
            break;
    }

    for (std::list<Auto<StmtVertex> >::iterator i = m_children.begin();
            i != m_children.end(); ++i)
        (*i)->modifyForVerification();
}

void StmtVertex::mergeEqualChildren() {
    bool bIsChanged;
    do {
        bIsChanged = false;
        for (std::list<StmtVertexPtr>::iterator i = m_children.begin(), j = i;
            i != m_children.end(); j = i, ++i) {
            if (*this != **i)
                continue;

            m_children.splice(i, (*i)->m_children, (*i)->m_children.begin(), (*i)->m_children.end());
            m_children.erase(i);

            i = (i == j ? m_children.begin() : j);
            bIsChanged = true;
        }
    } while (bIsChanged);
}

void StmtVertex::simplify() {
    mergeEqualChildren();

    for (std::list<StmtVertexPtr>::iterator i = m_children.begin();
            i != m_children.end(); ++i)
        (*i)->simplify();
}

template<typename _Block>
StatementPtr StmtVertex::mergeBlock() const {
    if (m_children.empty())
        return NULL;

    std::vector<StatementPtr> merged;
    for (std::list<StmtVertexPtr>::const_iterator i = m_children.begin();
        i != m_children.end(); ++i)
        if (const StatementPtr pStmt = (*i)->mergeForVerification())
            merged.push_back(pStmt);

    if (merged.size() == 1)
        return merged[0];

    Auto<_Block>
        pBlock = new _Block(),
        pCurrent = pBlock;

    for (size_t i = 0; i < merged.size(); ++i) {
        pCurrent->add(merged[i]);
        if (i + 2 >= merged.size())
            continue;
        pCurrent->add(new _Block());
        pCurrent = pCurrent->get(1).template as<_Block>();
    }

    return pBlock;
}

StatementPtr StmtVertex::mergeIf() const {
    if (m_children.empty())
        return NULL;

    StatementPtr
        pFirst = m_children.front()->mergeForVerification(),
        pSecond = m_children.front() != m_children.back()
            ? m_children.back()->mergeForVerification() : StatementPtr(NULL);

    return new If(this->m_pStmt.as<If>()->getArg(), pFirst, pSecond);
}

StatementPtr StmtVertex::mergeVariableDecl() const {
    if (m_children.empty())
        return NULL;
    return m_children.front()->mergeForVerification();
}

StatementPtr StmtVertex::mergeForVerification() const {
    if (!m_pStmt)
        return NULL;

    switch (m_pStmt->getKind()) {
        case Statement::IF:
            return mergeIf();
        case Statement::BLOCK:
            return mergeBlock<Block>();
        case Statement::PARALLEL_BLOCK:
            return mergeBlock<ParallelBlock>();
        case Statement::VARIABLE_DECLARATION:
            return mergeVariableDecl();
    }

    return m_pStmt;
}

}
