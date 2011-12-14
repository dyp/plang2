/// \file verification.cpp
///

#include "ir/visitor.h"
#include "generate_name.h"

using namespace ir;

// Pseudo casting.
ir::PredicateTypePtr predicateType(ir::ExpressionPtr _pExpr) {
    return _pExpr->getType().as<PredicateType>();
}

ir::PredicateTypePtr predicateType(const ir::FunctionCall &_call) {
    return predicateType(_call.getPredicate());
}

ir::PredicateTypePtr predicateType(const ir::Call &_call) {
    return predicateType(_call.getPredicate());
}

// FIXME ExprType of predicate.
ir::PredicateReferencePtr predicateReference(const ir::Call &_call) {
    return _call.getPredicate().as<PredicateReference>();
}

// Class for call checker.
class CallChecker : public Visitor {
public:
    CallChecker(const ir::Node &_node) :
        m_pNode(&_node), m_bResult(false)
    {}

    virtual bool visitFunctionCall(ir::FunctionCall &_node) {
        m_bResult = true;
        stop();
        return false;
    }

    bool run() {
        traverseNode(*m_pNode);
        return m_bResult;
    }

private:
    ir::NodePtr m_pNode;
    bool m_bResult;

};

bool callChecker(ir::NodePtr _pNode) {
    if (!_pNode)
        return false;
    CallChecker cc(*_pNode);
    return cc.run();
}

typedef Collection<ir::FormulaDeclaration> Formulas;
typedef Auto<Formulas> FormulasPtr;

// Class to extract needed formulas from module.
class FormulasCollector : public ir::Visitor {
public:
    FormulasCollector(const ir::Node &_node, Formulas &_container) :
        m_pNode(&_node), m_pContainer(&_container), m_nCount(0)
    {}

    bool visitFormulaCall(ir::FormulaCall &_call) {
        if (m_pContainer->findByNameIdx(_call.getName()) == -1)
            if (_call.getTarget()) {
                m_pContainer->prepend(_call.getTarget());
                ++m_nCount;
            }
        return true;
    }

    int run() {
        traverseNode(*m_pNode);
        return m_nCount;
    }

private:
    ir::NodePtr m_pNode;
    FormulasPtr m_pContainer;
    int m_nCount;
};

int collectFormulas(ir::Node &_node, FormulasPtr _pContainer) {
    if (_pContainer)
        return FormulasCollector(_node, *_pContainer).run();
    else
        return 0;
}

void collectAllFormulas(ir::Node &_node, FormulasPtr _pContainer) {
    if (!_pContainer)
        return;
    if (collectFormulas(_node, _pContainer) != 0) {
        FormulasPtr pContainer = new Formulas();
        pContainer->assign(*_pContainer);
        collectAllFormulas(*_pContainer, pContainer);
        _pContainer->assign(*pContainer);
    }
}

FormulasPtr collectAllFormulas(ir::Node &_node) {
    FormulasPtr pContainer = new Formulas();
    collectAllFormulas(_node, pContainer);
    return pContainer;
}

// Class for analize variables.
class VarAnalyzer : public Visitor {
public:
    VarAnalyzer(const ir::Node &_node, const ir::NamedValues &_container, const ir::NamedValuesPtr _pStandart = NULL) :
        m_pNode(&_node), m_pContainer(&_container), m_pStandart(_pStandart)
    {}

    void addValue(const ir::NamedValue &_val) {
        if (!m_pStandart) {
            if (m_pContainer->findByNameIdx(_val.getName()) == (size_t)-1)
                m_pContainer->add(&_val);
        }
        else {
            if (m_pContainer->findByNameIdx(_val.getName()) == (size_t)-1)
                if (m_pStandart->findByNameIdx(_val.getName()) != (size_t)-1)
                    m_pContainer->add(&_val);
        }
    }

    virtual bool visitVariableReference(ir::VariableReference &_node) {
        if (_node.getTarget())
            addValue(*_node.getTarget());
        return false;
    }

    virtual bool visitNamedValue(ir::NamedValue &_node) {
        addValue(_node);
        return false;
    }

    virtual bool visitParam(ir::Param &_node) {
        addValue(_node);
        return false;
    }

    // TODO Type parameters order, formulas.

    void run() {
        traverseNode(*m_pNode);
    }

    void runNamedValues(ir::NamedValues &_orig) {
        traverseNode(_orig);
    }

private:
    ir::NodePtr m_pNode;
    ir::NamedValuesPtr m_pContainer, m_pStandart;

public:
    static ir::NamedValuesPtr _varCollector(const ir::NodePtr _pNode, const ir::NamedValuesPtr pStandart = NULL) {
        ir::NamedValuesPtr pContainer = new ir::NamedValues();
        if (_pNode)
            VarAnalyzer(*_pNode, *pContainer, pStandart).run();
        return pContainer;
    }

    static void _varCollector(const ir::NodePtr _pNode, const ir::NamedValues &_pContainer, const ir::NamedValuesPtr _pStandart = NULL) {
        if (!_pNode)
            return;
        VarAnalyzer(*_pNode, _pContainer, _pStandart).run();
    }

};

bool varChecker(const ir::NodePtr _pNode, const ir::NamedValue &_var) {

    if (!_pNode)
        return false;

    ir::NamedValuesPtr pStandart = new ir::NamedValues();
    pStandart->add(&_var);

    ir::NamedValuesPtr pContainer = VarAnalyzer::_varCollector(_pNode, pStandart);

    return (pContainer->findByNameIdx(_var.getName()) != (size_t)-1);

}

ir::NamedValuesPtr varCollector(const ir::NodePtr _pNode) {
    return VarAnalyzer::_varCollector(_pNode);
}

void varCollector(const ir::NodePtr _pNode, const ir::NamedValues &_container) {
    VarAnalyzer::_varCollector(_pNode, _container);
}

ir::NamedValuesPtr compareValues(const ir::NodePtr _pNode, const ir::NamedValues &_pStandart) {
    return VarAnalyzer::_varCollector(_pNode, &_pStandart);
}

ir::NamedValuesPtr compareValuesWithOrder(const ir::NodePtr _pNode, const ir::NamedValues &_standart) {
    ir::NamedValuesPtr pContainer = compareValues(_pNode, _standart);
    return compareValues(&_standart, *pContainer);
}

static std::wstring fmtInt(const int _n) {
    int i = _n;
    std::wstring result = L"";
    while (i!=0) {
        result += ('a' + i % 26);
        i = i / 26;
    }
    return result;
}

ir::NamedValuePtr generateNewVariable(const ir::NodePtr _pNode, const ir::TypePtr _pType) {
    ir::NamedValuesPtr pVars = varCollector(_pNode);
    for (int i=1; ; ++i)
        if (pVars->findByNameIdx(fmtInt(i)) == -1)
            return new ir::NamedValue(fmtInt(i), _pType);
}

// Class of factual and formal params.
class Parameters : public ir::Node {
public:
    Parameters() {}

    void addParam(ir::NamedValue &_params, ir::Expression &_args) {
        if (m_params.findByNameIdx(_params.getName()) == -1) {
            m_params.add(new ir::NamedValue(_params.getName(), _params.getType()));
            m_args.add(&_args);
        }
    }

    ir::ExpressionPtr getArg(ir::NamedValue &_param) {
        const int ind = m_params.findByNameIdx(_param.getName());
        if (ind != -1)
            return m_args.get(ind);
        else
            return NULL;
    }

private:
    ir::Collection<ir::Expression> m_args;
    ir::NamedValues m_params;
};

typedef Auto<Parameters> ParametersPtr;

// Class for extracting params.
class GenerateParameters : public Visitor {
public:
    GenerateParameters(const ir::FormulaCall &_call, const Parameters &_params) :
        m_pNode(&_call), m_pParameters(&_params), m_pStandart(new NamedValues()), m_bRoot(false), m_nInd(0)
    {
        m_pStandart->assign(_call.getTarget()->getParams());
    }

    GenerateParameters(const ir::FunctionCall &_call, const Parameters &_params) :
        m_pNode(&_call), m_pParameters(&_params), m_pStandart(new NamedValues()), m_bRoot(false), m_nInd(0)
    {
        m_pStandart->assign(predicateType(&_call)->getInParams());
        m_pStandart->append(*varCollector(&predicateType(_call)->getOutParams()));
    }

    GenerateParameters(const ir::Call &_call, const Parameters &_params) :
        m_pNode(&_call), m_pParameters(&_params), m_pStandart(new NamedValues()), m_bRoot(false), m_nInd(0)
    {
        m_pStandart->assign(predicateType(_call)->getInParams());
        m_pStandart->append(*varCollector(&predicateType(_call)->getOutParams()));
    }

    virtual bool visitFormulaCall(ir::FormulaCall &_expr) {
        m_bRoot = true;
        return true;
    }

    virtual bool traverseFunctionCall(ir::FunctionCall &_expr) {
        VISITOR_ENTER(FunctionCall, _expr);
        m_bRoot = true;
        VISITOR_TRAVERSE_COL(Expression, FunctionCallArgs, _expr.getArgs());
        VISITOR_EXIT();
    }

    virtual bool traverseCall(ir::Call &_stmt) {
        VISITOR_ENTER(Call, _stmt);
        m_bRoot = true;
        VISITOR_TRAVERSE_COL(Expression, PredicateCallArgs, _stmt.getArgs());
        for (size_t i = 0; i < _stmt.getBranches().size(); ++i) {
            CallBranch &br = *_stmt.getBranches().get(i);
            VISITOR_TRAVERSE_COL(Expression, PredicateCallBranchResults, br);
        }
        VISITOR_EXIT();
    }

    virtual bool visitExpression(ir::Expression &_expr) {
        if (!m_bRoot)
            return true;
        if (m_pStandart->get(m_nInd)) {
            m_pParameters->addParam(*m_pStandart->get(m_nInd), _expr);
            ++m_nInd;
            return false;
        }
        return false;
    }

    ParametersPtr run() {
        traverseNode(*m_pNode);
        return m_pParameters;
    }

private:
    ir::NodePtr m_pNode;
    ir::NamedValuesPtr m_pStandart;
    ParametersPtr m_pParameters;
    bool m_bRoot;
    int m_nInd;

};

ParametersPtr generateParams(const ir::FormulaCall &_call) {
    ParametersPtr pParameters = new Parameters();
    return GenerateParameters(_call,  *pParameters).run();
}

ParametersPtr generateParams(const ir::FunctionCall &_call) {
    ParametersPtr pParameters = new Parameters();
    return GenerateParameters(_call,  *pParameters).run();
}

ParametersPtr generateParams(const ir::Call &_call) {
    ParametersPtr pParameters = new Parameters();
    return GenerateParameters(_call,  *pParameters).run();
}

// Class for addition arguments to formula call.
class ArgsAddition : public Visitor {
public:
    ArgsAddition(const ir::Node &_node, const ParametersPtr _pMask = NULL) :
        m_pNode(&_node), m_pParameters(_pMask)
    {}

    virtual bool visitNamedValue(ir::NamedValue &_node) {
        ir::FormulaCallPtr pFormulaCall= (ir::FormulaCallPtr &)m_pNode;
        if (!m_pParameters)
            pFormulaCall->getArgs().add(new ir::VariableReference(&_node));
        else
            pFormulaCall->getArgs().add(clone(m_pParameters->getArg(_node)));
        return false;
    }

    void run() {
        traverseNode(m_pNode.as<FormulaCall>()->getTarget()->getParams());
    }

private:
    ir::NodePtr m_pNode;
    ParametersPtr m_pParameters;

};

ir::ExpressionPtr addCallArgs(const ir::ExpressionPtr _pExpr, const ParametersPtr _pParameters = NULL) {
    if (!_pExpr)
        return NULL;
    ArgsAddition(*_pExpr, _pParameters).run();
    return _pExpr;
}

ir::ExpressionPtr addSimpleArgs(const ir::ExpressionPtr _pExpr) {
    return addCallArgs(_pExpr);
}

ir::ExpressionPtr copyCallArgs(const ir::ExpressionPtr _pExpr, const ir::FormulaCall &_call) {
    return addCallArgs(_pExpr, generateParams(_call));
}

// Class for extracting call from statements, and its modifing.
class ExtractCall : Visitor {
public:
    ExtractCall(const ir::Node &_node, const ir::NodePtr _pNewNode = NULL) :
        Visitor(CHILDREN_FIRST), m_pNode(&_node), m_pNewNode(_pNewNode), m_pCall(NULL)
    {}

    virtual bool visitFunctionCall(ir::FunctionCall &_node) {
        m_pCall = clone(_node);
        if (m_pNewNode)
            callSetter(m_pNewNode);
        stop();
        return false;
    }

    ir::FunctionCallPtr run() {
        traverseNode(*m_pNode);
        return m_pCall;
    }

private:
    ir::NodePtr m_pNode, m_pNewNode;
    ir::FunctionCallPtr m_pCall;
};

ir::FunctionCallPtr extractFirstCall(const ir::NodePtr _pNode, const ir::NodePtr _pNewNode = NULL) {
    if (!_pNode)
        return NULL;
    return ExtractCall(*_pNode, _pNewNode).run();
}

ir::CallPtr generatePredicateCallFromFunctionCall(ir::FunctionCall &_fcall, ir::VariableReference &_var) {
    ir::CallPtr pCall = new ir::Call(_fcall.getPredicate());
    ir::CallBranchPtr pBranch = new ir::CallBranch();
    pBranch->add(&_var);

    pCall->getArgs().assign(_fcall.getArgs());
    pCall->getBranches().add(pBranch);

    return pCall;
}

// Conversion recursive call to superposition operator.
// _pCall, _pTail and _pVar never NULL.
void generateSuperpositionFromAssignment(ir::Assignment &_assignment, ir::CallPtr &_pCall, ir::AssignmentPtr &_pTail, ir::NamedValuePtr &_pVar) {

    ir::FunctionCallPtr pCall = extractFirstCall(&_assignment);
    if(!pCall)
        return;

    // FIXME Variable type.
    _pVar = generateNewVariable(&_assignment, new ir::Type(ir::Type::NAT, -1));
    _pCall = generatePredicateCallFromFunctionCall(*pCall, *new ir::VariableReference(_pVar));
    _pTail = clone(_assignment);

    extractFirstCall(_pTail, new ir::VariableReference(_pVar));

}

// Some utils.
ir::NamedValuesPtr getPredicateParams(const ir::Predicate &_predicate) {
    ir::NamedValuesPtr pContainer = varCollector(&_predicate.getInParams());
    varCollector(&_predicate.getOutParams(), *pContainer);
    return pContainer;
}

ir::NamedValuesPtr getPredicateParams(const ir::PredicateType &_predicateType) {
    ir::NamedValuesPtr pContainer = varCollector(&_predicateType.getInParams());
    varCollector(&_predicateType.getOutParams(), *pContainer);
    return pContainer;
}

ir::FormulaDeclarationPtr declareFormula(const std::wstring &_sName, const ir::Expression &_expr,
                                         const ir::NamedValuesPtr _pParams = NULL, const ir::TypePtr _pType = NULL)
{
    ir::FormulaDeclarationPtr pFormulaDecl = new ir::FormulaDeclaration(_sName, _pType , clone(_expr));
    if (_expr.getKind() == ir::Expression::FORMULA)
        if (((ir::Formula&)_expr).getQuantifier() == ir::Formula::NONE)
            pFormulaDecl->setFormula(clone(((ir::Formula&)_expr).getSubformula()));
    if (_pParams)
        pFormulaDecl->getParams().assign(*clone(compareValuesWithOrder(&_expr, *_pParams)));
    else
        pFormulaDecl->getParams().assign(*clone(varCollector(&_expr)));
    return pFormulaDecl;
}

ir::FormulaDeclarationPtr extractFormulaFromPredicate(const std::wstring &_sName, const ir::Predicate &_predicate,
                                                      const ir::Expression &_expr, const ir::TypePtr _pType = NULL)
{
    return declareFormula(_sName, _expr, getPredicateParams(_predicate), _pType);
}

// Get left variable of assignment
ir::NamedValuePtr getLeftVariable(const ir::Assignment &_assignment) {
    if (_assignment.getLValue()->getKind() == ir::Expression::VAR)
        return varCollector(_assignment.getLValue())->get(0);
    else
        return NULL;
}
// and equolity.
ir::NamedValuePtr getLeftVariable(ir::Binary &_binary) {
    if (_binary.getLeftSide()->getKind() == ir::Expression::VAR)
        return varCollector(_binary.getLeftSide())->get(0);
    else
        return NULL;
}

// Predicate collection class.
class PredicateInfoCollection : public Visitor {
public:

    // Predicate pre-, post- condition, measure and module name.
    class PredicateInfo : public Node {
    public:
        std::wstring m_sPredicateName, m_sModuleName;
        ir::FormulaDeclarationPtr m_pPreCond, m_pPostCond, m_pMeasure;
    };

    typedef Auto<PredicateInfo> PredicateInfoPtr;

    virtual bool visitPredicate(ir::Predicate &_predicate) {
        PredicateInfoPtr pInfo = new PredicateInfo();

        pInfo->m_sPredicateName = _predicate.getName();
        pInfo->m_sModuleName = _predicate.getName();
        pInfo->m_pPreCond = getFormulaDeclaration(_predicate,
                                                  m_nameGenerator.makeNamePredicatePrecondition(_predicate),
                                                  _predicate.getPreCondition());
        pInfo->m_pPostCond = getFormulaDeclaration(_predicate,
                                                   m_nameGenerator.makeNamePredicatePostcondition(_predicate),
                                                   _predicate.getPostCondition());
        pInfo->m_pMeasure = getFormulaDeclaration(_predicate,
                                                  m_nameGenerator.makeNamePredicateMeasure(_predicate),
                                                  _predicate.getMeasure(),
                                                  ir::Type::NAT);

        m_info.add(pInfo);
        return false;
    }

    std::wstring getModuleName(const std::wstring &_predicateName) {
        PredicateInfoPtr pInfo = getInfo(_predicateName);
        if (!pInfo)
            return L"";
        else
            return pInfo->m_sModuleName;
    }

    ir::FormulaDeclarationPtr getPreCondition(const std::wstring &_predicateName) {
        PredicateInfoPtr pInfo = getInfo(_predicateName);
        if (pInfo)
            return pInfo->m_pPreCond;
        else
            return NULL;
    }

    ir::FormulaDeclarationPtr getPostCondition(const std::wstring &_predicateName) {
        PredicateInfoPtr pInfo = getInfo(_predicateName);
        if (pInfo)
            return pInfo->m_pPostCond;
        else
            return NULL;
    }

    ir::FormulaDeclarationPtr getMeasure(const std::wstring &_predicateName) {
        PredicateInfoPtr pInfo = getInfo(_predicateName);
        if (pInfo)
            return pInfo->m_pMeasure;
        else
            return NULL;
    }

private:
    Collection<PredicateInfo> m_info;
    NameGenerator m_nameGenerator;

    ir::FormulaDeclarationPtr getFormulaDeclaration(const ir::Predicate &_predicate, const std::wstring &_sName, const ir::ExpressionPtr _pExpr,
                                                    const int _nType = -1)
    {
        if (!_pExpr)
            return NULL;
        return extractFormulaFromPredicate(_sName, _predicate, *_pExpr, _nType != -1 ? new ir::Type(_nType, -1) : NULL);
    }

    PredicateInfoPtr getInfo(const std::wstring &_predicateName) {
        for(size_t i = 0; i<m_info.size(); ++i)
            if (m_info.get(i)->m_sPredicateName == _predicateName)
                return m_info.get(i);
        return NULL;
    }
};

// Correctnes proove class.
class Correction : public Visitor {
public:
    Correction(const ir::Module &_module, const ir::Module &_theoriesContainer) :
        m_pModule(&_module), m_pTheories(&_theoriesContainer), m_pPreCond(NULL), m_pPostCond(NULL), m_pPredicate(NULL), m_nLemma(1)
    {
        if (m_pModule)
            m_info.traverseNode(*m_pModule);
    }

    // Corr(A, t, K, P, Q).
    void generalCorr(const ir::Predicate &_predicate, ir::Node &_node, const ir::ExpressionPtr _pPreCond = NULL,
                     const ir::ExpressionPtr _pPostCond = NULL)
    {

        ir::ExpressionPtr pLastPreCond(m_pPreCond), pLastPostCond(m_pPostCond);
        m_pPredicate = &_predicate;

        _modifyConditions(_pPreCond, _pPostCond);
        traverseNode(_node);
        _modifyConditions(pLastPreCond, pLastPostCond);

    }

    // Corr(K, P, Q).
    void specialCorr(ir::Statement &_statement, const ir::ExpressionPtr _pPreCond = NULL,
                     const ir::ExpressionPtr _pPostCond = NULL)
    {

        if (!m_pPredicate || !_pPreCond || !_pPostCond)
            return;

        addLemma(_generateTotality(*m_pPredicate, _statement, *_pPreCond, *_pPostCond));
        addLemma(_generateCorr(*m_pPredicate, _statement, *_pPreCond, *_pPostCond));

    }

    // QC.
    void ruleQC(ir::If &_if, const ir::ExpressionPtr _pPreCond = NULL, const ir::ExpressionPtr _pPostCond = NULL) {

        if (!m_pPredicate || !_pPreCond || !_pPostCond)
            return;

        // Corr(A, t, B, P(x) & E, P, Q).
        if (_if.getBody())
            generalCorr(*m_pPredicate,
                        *_if.getBody(),
                        new ir::Binary(ir::Binary::BOOL_AND,
                                        _pPreCond,
                                        _if.getArg()),
                        _pPostCond);

        // Corr(A, t, C, P(x) & not E, P, Q).
        if (_if.getElse())
            generalCorr(*m_pPredicate,
                        *_if.getElse(),
                        new ir::Binary(ir::Binary::BOOL_AND,
                                        _pPreCond,
                                        new ir::Unary(ir::Unary::BOOL_NEGATE,
                                                    _if.getArg())),
                        _pPostCond);

    }

    // QS.
    void ruleQS(ir::Statement &_first, ir::Statement &_second, ir::NamedValue &_linkedValue,
                const ir::ExpressionPtr _pPreCond = NULL, const ir::ExpressionPtr _pPostCond = NULL)
    {}

    // QSB.
    void ruleQSB(ir::Call &_first, ir::Statement &_second, ir::NamedValue &_linkedValue,
                const ir::ExpressionPtr _pPreCond = NULL, const ir::ExpressionPtr _pPostCond = NULL)
    {

        if (!m_pPredicate || !_pPreCond || !_pPostCond)
            return;

        // FIXME Rules collision.
        /*generalCorr(*m_pPredicate,
         *             _first,
         *             generatePreConditionCall(_first),
         *             generatePostConditionCall(_first));*/

        addLemma(_generateQSB2(*m_pPredicate, _first, *_pPreCond));

        generalCorr(*m_pPredicate,
                    _second,
                    new ir::Binary(ir::Binary::BOOL_AND,
                                   _pPreCond,
                                   _generatetPostConditionCall(_first)),
                    _pPostCond);

    }

    // Corr(A, t, if (E) B(x: y) else C(x: y), P, Q).
    virtual bool visitIf(ir::If &_if) {
        ruleQC(_if, m_pPreCond, m_pPostCond);
        stop();
        return false;
    }

    // TODO Add 3 statements.

    // TODO Normal call.
    virtual bool visitCall(ir::Call &_call) {
        if (!m_pPreCond || !m_pPredicate)
            return false;

        // FIXME _call.getLabel()->getName().
        if (predicateReference(_call)->getName() != m_pPredicate->getName()) {
            // Simple call.
            // RB.
        }
        else
            // Recursive call.
            // QSB2 lemma.
            addLemma(_generateQSB2(*m_pPredicate, _call, *m_pPreCond));

        return false;
    }

    // Corr(A, t, a := E, P, Q).
    virtual bool visitAssignment(ir::Assignment &_assignment) {

        if (!callChecker(&_assignment))
            specialCorr(_assignment, m_pPreCond, m_pPostCond);
        else {
            ir::NamedValuePtr pVar;
            ir::AssignmentPtr pTail;
            ir::CallPtr pCall;
            generateSuperpositionFromAssignment(_assignment, pCall, pTail, pVar);

            // Never NULL :)
            if (!pVar || !pCall || !pTail)
                return false;

            if (isCorrect(*pCall))
                ruleQSB(*pCall, *pTail, *pVar, m_pPreCond, m_pPostCond);
            else
                ruleQS(*pCall, *pTail, *pVar, m_pPreCond, m_pPostCond);
        }

        // FIXME Stop traversing.
        //stop();
        return false;
    }

    virtual bool visitPredicate(ir::Predicate &_predicate) {
        // Create Theory.
        m_pTheory = new Module(_predicate.getName());
        m_pTheories->getModules().add(m_pTheory);

        // Implement pre-, post- condition and measure.
        ir::FormulaDeclarationPtr
            pPreCond   = m_info.getPreCondition(_predicate.getName()),
            pPostCond  = m_info.getPostCondition(_predicate.getName()),
            pMeasure   = m_info.getMeasure(_predicate.getName());

         if (pPreCond)
             m_pTheory->getFormulas().append(*collectAllFormulas(*pPreCond));
         if (pPostCond)
             m_pTheory->getFormulas().append(*collectAllFormulas(*pPostCond));

         addFormula(pPreCond);
         addFormula(pPostCond);
         addFormula(pMeasure);

        // Start lemmas generation.
        if (_predicate.getBlock() && pPreCond && pPostCond)
            generalCorr(_predicate,
                        *_predicate.getBlock(),
                        addSimpleArgs(new ir::FormulaCall(pPreCond)),
                        addSimpleArgs(new ir::FormulaCall(pPostCond)));

        return false;
    }

    void run() {
        traverseNode(*m_pModule);
    }

private:
    ir::ModulePtr m_pModule, m_pTheory, m_pTheories;
    ir::ExpressionPtr m_pPreCond, m_pPostCond;
    ir::PredicatePtr m_pPredicate;
    PredicateInfoCollection m_info;
    int m_nLemma;

    void addLemma(ir::ExpressionPtr _pExpr, const std::wstring &_name = L"") {
        if (!m_pTheory || !_pExpr)
            return;
        ir::LemmaDeclarationPtr pLemma = new ir::LemmaDeclaration(clone(_pExpr));
        if (_name.empty())
            pLemma->setLabel(new ir::Label(fmtInt(m_nLemma++, L"L%u")));
        else
            pLemma->setLabel(new ir::Label(_name));
        m_pTheory->getLemmas().add(pLemma);
    }

    void addFormula(ir::FormulaDeclarationPtr _pFormula) {
        if (m_pTheory && _pFormula)
            m_pTheory->getFormulas().add(clone(_pFormula));
    }

    inline void _modifyConditions(const ir::ExpressionPtr _pPreCond, const ir::ExpressionPtr _pPostCond) {
        m_pPreCond = _pPreCond;
        m_pPostCond = _pPostCond;
    }

    // Create quantifier formula.
    static ir::FormulaPtr _generateFormula(bool _bForall, const ir::ExpressionPtr _pExpr, ir::NamedValuesPtr _pLinked,
                                           bool _bLinkedPrevent = false, bool _bCheckAssignment = true)
    {

        ir::FormulaPtr pFormula = new ir::Formula();
        if (!_pExpr)
            return pFormula;

        pFormula->setSubformula(_pExpr);
        pFormula->setQuantifier(_bForall ? ir::Formula::UNIVERSAL : ir::Formula::EXISTENTIAL);

        if (!_pLinked) {
            pFormula->getBoundVariables().assign(*varCollector(_pExpr));
            return pFormula;
        }

        ir::NamedValuesPtr pArgs = new ir::NamedValues(*_pLinked);

        if (_bLinkedPrevent) {
            const int nCount = pArgs->size();
            varCollector(_pExpr, *pArgs);

            ir::NamedValuesPtr pArgsAllowed = new ir::NamedValues();
            for(unsigned int i=nCount; i<pArgs->size(); ++i) {
                pArgsAllowed->add(pArgs->get(i));
            }
            pFormula->getBoundVariables().assign(*compareValuesWithOrder(_pExpr, *pArgsAllowed));
            return pFormula;
        }

        if (_bCheckAssignment && _pExpr->getKind() == ir::Expression::BINARY) {
            ir::NamedValuePtr pVar = getLeftVariable((ir::Binary &)*_pExpr);
            if (pVar)
                if (pArgs->findByNameIdx(pVar->getName()) == -1)
                    pArgs->add(pVar);
        }

        pFormula->getBoundVariables().assign(*compareValuesWithOrder(_pExpr, *pArgs));

        return pFormula;

    }

    // generate L(a := E).
    static ir::ExpressionPtr _generateLogicAssignment(ir::Assignment &_assignment) {
        ir::NamedValuePtr pLeftVariable = getLeftVariable(_assignment);
        if (pLeftVariable) {
            if (!varChecker(_assignment.getExpression(), *pLeftVariable))
                return new ir::Binary(ir::Binary::EQUALS,
                                      new ir::VariableReference(pLeftVariable),
                                      _assignment.getExpression());
            else
                return new ir::Binary(ir::Binary::EQUALS,
                                      new ir::VariableReference(generateNewVariable(_assignment.getExpression(),
                                                                                    pLeftVariable->getType())),
                                      _assignment.getExpression());
        }
        // TODO Correct return.
        return new ir::Binary();
    }

    // generate L(K(x: y)).
    static ir::ExpressionPtr _generateLogic(ir::Statement &_statement) {
        switch (_statement.getKind()) {
            case ir::Statement::ASSIGNMENT:
                return _generateLogicAssignment((ir::Assignment &)_statement);
            default:
                return new ir::Binary();
        }
    }

    // forall x,y. P(x) => (L(S(x: y)) => Q(x, y)).
    static ir::ExpressionPtr _generateCorr(ir::Predicate &_predicate, ir::Statement &_statement, ir::Expression &_precond,
                                           ir::Expression &_postcond)
    {
        return _generateFormula(true,
                                new ir::Binary(ir::Binary::IMPLIES,
                                               &_precond,
                                               new ir::Binary(ir::Binary::IMPLIES,
                                                              _generateLogic(_statement),
                                                              &_postcond)),
                                NULL);
    }

    // forall x. P(x) => exists y. L(S(x: y)).
    static ir::ExpressionPtr _generateTotality(ir::Predicate &_predicate, ir::Statement &_statement, ir::Expression &_precond,
                                               ir::Expression &_postcond)
    {
        return _generateFormula(true,
                                new ir::Binary(ir::Binary::IMPLIES,
                                                &_precond,
                                                _generateFormula(false,
                                                                 _generateLogic(_statement),
                                                                 varCollector(&_predicate.getOutParams()))),
                                varCollector(&_predicate.getOutParams()), true);
    }

    // P*(u) = m(u)<m(x) & P(u).
    ir::ExpressionPtr _generateRecPreCond(ir::Call &_call) {
        const ir::FormulaDeclarationPtr pMeasure = m_info.getMeasure(predicateReference(_call)->getName());
        if (pMeasure)
            return new ir::Binary(ir::Binary::BOOL_AND,
                                new ir::Binary(ir::Binary::LESS,
                                                _generatetMeasureCall(_call),
                                                addSimpleArgs(new ir::FormulaCall(pMeasure))),
                                _generatePreConditionCall(_call));
        else
            return NULL;
    }

    // QSB2: forall x. P(x) => P*(u).
    ir::ExpressionPtr _generateQSB2(const ir::Predicate &_predicate, ir::Call &_call, ir::Expression &_precond) {
        const ir::ExpressionPtr pRecPreCond = _generateRecPreCond(_call);
        if (pRecPreCond)
            return _generateFormula(true,
                                    new ir::Binary(ir::Binary::IMPLIES,
                                                &_precond,
                                                pRecPreCond),
                                    &_predicate.getInParams());
        else
            return NULL;
    }

    // Is there formulas of correctness?
    bool isCorrect(const ir::Call &_call) {
        return predicateType(_call.getPredicate())->getPreCondition() &&
               predicateType(_call.getPredicate())->getPostCondition();
    }

    // Generate conditions call.
    ir::ExpressionPtr _generatePreConditionCall(const ir::Call &_call) {
        const ir::FormulaDeclarationPtr pPreCond = m_info.getPreCondition(predicateReference(_call)->getName());
        if (pPreCond)
            return addCallArgs(new ir::FormulaCall(pPreCond), generateParams(_call));
        else
            return NULL;
    }

    ir::ExpressionPtr _generatetPostConditionCall(const ir::Call &_call) {
        const ir::FormulaDeclarationPtr pPostCond = m_info.getPostCondition(predicateReference(_call)->getName());
        if (pPostCond)
            return addCallArgs(new ir::FormulaCall(pPostCond), generateParams(_call));
        else
            return NULL;
    }

    ir::ExpressionPtr _generatetMeasureCall(const ir::Call &_call) {
        const ir::FormulaDeclarationPtr pMeasure = m_info.getMeasure(predicateReference(_call)->getName());
        if (pMeasure)
            return addCallArgs(new ir::FormulaCall(pMeasure), generateParams(_call));
        else
            return NULL;
    }

};

ir::ModulePtr verify(const Module &_module) {
    ir::ModulePtr pTheories = new Module();
    Correction(_module, *pTheories).run();
    return pTheories;
}