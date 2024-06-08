/// \file statement_tree.h
///

#ifndef STATEMENT_TREE_H_
#define STATEMENT_TREE_H_

#include <list>

#include "ir/base.h"
#include "autoptr.h"

namespace st {

using StmtVertexPtr = std::shared_ptr<class StmtVertex>;

class StmtVertex : public std::enable_shared_from_this<StmtVertex> {
public:
    StmtVertex(const ir::StatementPtr& _pStmt) :
        m_pStmt(_pStmt)
    {}

    void setParent(const StmtVertexPtr& _pVertex) {
        m_pParent = _pVertex;
    }

    StmtVertexPtr& getParent() {
        return m_pParent;
    }

    StmtVertexPtr appendChild(const StmtVertexPtr& _pVertex) {
        if (!_pVertex)
            return NULL;
        m_children.push_back(_pVertex);
        _pVertex->setParent(shared_from_this());
        return m_children.back();
    }

    std::list<StmtVertexPtr>& getChildren() {
        return m_children;
    }

    ir::Statement& getStatement() const {
        return *m_pStmt;
    }

    bool operator==(const StmtVertex& _other) const {
        return (m_pStmt->getKind() == ir::Statement::BLOCK && _other.m_pStmt->getKind() == ir::Statement::BLOCK)
            || (m_pStmt->getKind() == ir::Statement::PARALLEL_BLOCK && _other.m_pStmt->getKind() == ir::Statement::PARALLEL_BLOCK);
    }

    bool operator!=(const StmtVertex& _other) const {
        return !this->operator ==(_other);
    }

    void expand();
    void modifyForVerification();
    void simplify();
    ir::StatementPtr mergeForVerification() const;

private:
    ir::StatementPtr m_pStmt;
    StmtVertexPtr m_pParent;
    std::list<StmtVertexPtr> m_children;

    // if(E) A else B :
    // if (E) -> (A, B)
    void expandIf(const ir::If& _if);

    // { A1, ..., An } :
    // {} -> (A1, ..., An)
    void expandBlock(const ir::Block& _block);

    // A1 || ... || An :
    // || -> (A1, ..., An)
    void expandBlockP(const ir::ParallelBlock& _block);

    // switch (E) { case E1: A1; ... case En: An; default: D } :
    // switch (E) -> (A1, ..., An, D)
    void expandSwitch(const ir::Switch& _switch);

    // { T1 a1; ... Tn an } :
    // { T1 a1; ... Tn an } -> (T1 a1, ..., Tn an)
    void expandVariableDeclarationGroup(const ir::VariableDeclarationGroup& _vdg);

    // For verification

    // if(E(f(x))) :
    // {} -> (f(x: y), if(E(y)))
    void modifyIf(const ir::IfPtr& _if);

    // f(..., g(x), ... : y) :
    // {} -> (g(x: z), f(..., z, ...: y))
    void modifyCall(const ir::CallPtr& _call);

    // switch (E) -> (A1, ..., An, D) :
    // if (E == E1) -> (A1, if (E == E2) -> (A2, ... if (E == En) -> (An, D)))
    void modifySwitch(const ir::SwitchPtr& _switch);

    // a = E(F(x)) :
    // {} -> (f(x: z), a = E(z))
    void modifyAssignment(const ir::AssignmentPtr& _assignment);

    // a1, ..., an = E1, ..., En :
    // || -> (a1 = E1, ..., an = En)
    void modifyMultiAssignment(const ir::MultiassignmentPtr& _massignment);

    // T a = E :
    // T a = E -> a = E
    void modifyVariableDeclaration(const ir::VariableDeclarationPtr& _decl);

    // { T1 a1; ... Tn an } :
    // {}
    void modifyVariableDeclarationGroup(const ir::VariableDeclarationGroupPtr& _vdg);

    // A -> B -> C  &&  A = B :
    // A -> C
    void mergeEqualChildren();

    // {} -> (A1, ..., An) :
    // { A1; { A2; ... { An-1; An }}}
    template<typename _Block>
    ir::StatementPtr mergeBlock() const;

    // if(E) -> (A, B) :
    // if(E) A else B
    ir::StatementPtr mergeIf() const;

    // T a -> A :
    // A
    ir::StatementPtr mergeVariableDecl() const;
};

}

#endif // STATEMENT_TREE_H_
