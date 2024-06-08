/// \file generate_semantics.cpp
///


#include <iostream>
#include <fstream>
#include <sstream>
#include <list>

#include "ir/statements.h"
#include "generate_semantics.h"
#include "generate_name.h"
#include "lexer.h"
#include "utils.h"
#include "llir.h"
#include "node_analysis.h"
#include "term_rewriting.h"

using namespace ir;
using namespace vf;

class CollectPreConditions::NameGenerator {
private:
    size_t m_typeNumber;   //for naming modules
    size_t m_lambdaNumber;
    size_t m_callNumber;   //for naming lemmas
    size_t m_switchDefaultNumber;
    size_t m_switchCaseNumber;
    size_t m_arrayPartIndexNumber;
    size_t m_subtypeParamNumber;
    size_t m_unionConsFieldNumber;
    size_t m_assignmentNumber;
    size_t m_divideNumber;
    size_t m_arrayConsNumber;
    size_t m_arrayModNumber;
    size_t m_ifNumber;
    size_t m_arrayUnionNumber;

public:
    NameGenerator() : m_typeNumber(0), m_lambdaNumber(0), m_callNumber(0), m_switchDefaultNumber(0),
                      m_switchCaseNumber(0), m_arrayPartIndexNumber(0), m_subtypeParamNumber(0), m_unionConsFieldNumber(0),
                      m_assignmentNumber(0), m_divideNumber(0), m_arrayConsNumber(0), m_arrayModNumber(0), m_ifNumber(0),
                      m_arrayUnionNumber(0) {}

    std::wstring makeNameSubmoduleForType();     //for modules
    std::wstring makeNameSubmoduleForLambda();
    std::wstring makeNameSubmoduleForProcess(ir::Process &_process);
    std::wstring makeNameSubmoduleForPredicate(ir::Predicate &_predicate);

    std::wstring makeNamePredicatePrecondition(ir::Predicate &_predicate);    //for preconditions
    std::wstring makeNamePredicateBranchPrecondition(ir::Predicate &_predicate, size_t _branchNumber);
    std::wstring makeNameTypePreCondition();
    std::wstring makeNameTypeBranchPreCondition(size_t _branchNumber);
    std::wstring makeNameProcessBranchPreCondition(ir::Process &_process, size_t _branchNumber);
    std::wstring makeNameLambdaToPredicate();   //for lambdas

    std::wstring makeNamePredicatePostcondition(ir::Predicate &_predicate);     //for verification
    std::wstring makeNamePredicateMeasure(ir::Predicate &_predicate);

    std::wstring makeNameLemmaCall();     //for lemmas
    std::wstring makeNameLemmaSwitchDefault();
    std::wstring makeNameLemmaSwitchCase();
    std::wstring makeNameLemmaArrayPartIndex();
    std::wstring makeNameLemmaSubtypeParam();
    std::wstring makeNameLemmaUnionConsField();
    std::wstring makeNameLemmaAssignment();
    std::wstring makeNameLemmaDivide();
    std::wstring makeNameLemmaArrayCons();
    std::wstring makeNameLemmaArrayMod();
    std::wstring makeNameLemmaIf();
    std::wstring makeNameLemmaArrayUnion();
};

///preconditions

std::wstring CollectPreConditions::NameGenerator::makeNamePredicatePrecondition(ir::Predicate &_predicate){
    return _predicate.getName() + L"Precondition";
}

std::wstring CollectPreConditions::NameGenerator::makeNamePredicateBranchPrecondition(ir::Predicate &_predicate, size_t _branchNumber){
    return _predicate.getName() + fmtInt(_branchNumber, L"Precondition%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameTypePreCondition(){
    return fmtInt(m_typeNumber, L"Type%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameTypeBranchPreCondition(size_t _branchNumber){
    return fmtInt(m_typeNumber, L"Type%u") + fmtInt(_branchNumber, L"Precondition%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameProcessBranchPreCondition(ir::Process &_process, size_t _branchNumber){
    return _process.getName() + fmtInt(_branchNumber, L"Precondition%u");
}


///verification

std::wstring CollectPreConditions::NameGenerator::makeNamePredicatePostcondition(ir::Predicate &_predicate){
    return _predicate.getName() + L"Postcondition";
}

std::wstring CollectPreConditions::NameGenerator::makeNamePredicateMeasure(ir::Predicate &_predicate){
    return _predicate.getName() + L"Measure";
}


///submodules

std::wstring CollectPreConditions::NameGenerator::makeNameSubmoduleForType(){
    m_typeNumber++;
    return fmtInt(m_typeNumber, L"Type%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameSubmoduleForLambda(){
    m_lambdaNumber++;
    return fmtInt(m_lambdaNumber, L"Lambda%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLambdaToPredicate(){
    return fmtInt(m_lambdaNumber, L"Lambda%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameSubmoduleForProcess(ir::Process &_process){
    return _process.getName();
}

std::wstring CollectPreConditions::NameGenerator::makeNameSubmoduleForPredicate(ir::Predicate &_predicate){
    return _predicate.getName();
}



///semantics' lemmas

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaCall(){
    m_callNumber++;
    return fmtInt(m_callNumber, L"Call%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaSwitchDefault(){
    m_switchDefaultNumber++;
    return fmtInt(m_switchDefaultNumber, L"SwitchDefault%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaSwitchCase(){
    m_switchCaseNumber++;
    return fmtInt(m_switchCaseNumber, L"SwitchCase%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaArrayPartIndex(){
    m_arrayPartIndexNumber++;
    return fmtInt(m_arrayPartIndexNumber, L"ArrayPartIndex%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaSubtypeParam(){
    m_subtypeParamNumber++;
    return fmtInt(m_subtypeParamNumber, L"SubtypeParam%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaUnionConsField(){
    m_unionConsFieldNumber++;
    return fmtInt(m_unionConsFieldNumber, L"UnionConsField%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaAssignment(){
    m_assignmentNumber++;
    return fmtInt(m_assignmentNumber, L"Assignment%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaDivide(){
    m_divideNumber++;
    return fmtInt(m_divideNumber, L"Divide%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaArrayCons(){
    m_arrayConsNumber++;
    return fmtInt(m_arrayConsNumber, L"ArrayConstructor%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaArrayMod(){
    m_arrayModNumber++;
    return fmtInt(m_arrayModNumber, L"ArrayModification%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaIf(){
    m_ifNumber++;
    return fmtInt(m_ifNumber, L"If%u");
}

std::wstring CollectPreConditions::NameGenerator::makeNameLemmaArrayUnion(){
    m_arrayUnionNumber++;
    return fmtInt(m_arrayUnionNumber, L"ArrayUnion%u");
}

CollectPreConditions::CollectPreConditions(const ModulePtr &_module) :
    m_module(_module), m_pNameGen(std::make_shared<NameGenerator>())
{}

ExpressionPtr CollectPreConditions::collectConditions() {
    ExpressionPtr pExpr;
    for (std::list<Loc>::reverse_iterator i = m_path.rbegin(); i != m_path.rend(); ++i) {
        switch (i->role) {

            case R_PredicateDecl: {
                const auto pFormula = i->pNode->as<Predicate>()->getPreCondition();

                if(pFormula) {
                    if(!pExpr)
                        pExpr = pFormula->getSubformula();
                    else
                        pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pFormula->getSubformula(), pExpr);
                }
                break;
            }

            case R_IfBody: {
                assert(::next(i)->type == N_If);
                const auto pExpr1 = ::next(i)->pNode->as<If>()->getArg();

                if(!pExpr)
                    pExpr = pExpr1;
                else
                    pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1, pExpr);

                break;
            }

            case R_IfElse: {
                assert(::next(i)->type == N_If);
                auto pExpr1 = ::next(i)->pNode->as<If>()->getArg();

                if ((pExpr1->getKind() == Expression::UNARY) && (pExpr1->as<Unary>()->getOperator() == Unary::BOOL_NEGATE))
                    pExpr1 = pExpr1->as<Unary>()->getExpression();
                else
                    pExpr1 = std::make_shared<Unary>(Unary::BOOL_NEGATE, pExpr1);


                if(!pExpr)
                    pExpr = pExpr1;
                else
                    pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1, pExpr);

                break;
            }

            case R_SwitchDefault: {
                const auto pSwitch = ::next(i)->pNode->as<Switch>();
                ExpressionPtr pExpr1;

                for(size_t j = 0; j < pSwitch->size(); ++j) {

                    for(size_t k = 0; k < pSwitch->get(j)->getExpressions().size(); ++k) {

                        const auto pExprCurrent = pSwitch->get(j)->getExpressions().get(k);

                        if (pExprCurrent->getKind() != Expression::TYPE) {
                            pExpr1 = std::make_shared<Binary>(Binary::NOT_EQUALS, pSwitch->getArg(), pExprCurrent);
                        } else {
                            if (pExprCurrent->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                                const auto range = pExprCurrent->as<TypeExpr>()->getContents()->as<Range>();

                                pExpr1 = std::make_shared<Binary>(Binary::BOOL_OR,
                                    std::make_shared<Binary>(Binary::LESS, pSwitch->getArg(), range->getMin()),
                                    std::make_shared<Binary>(Binary::GREATER,  pSwitch->getArg(), range->getMax()));
                            }
                        }

                        if(!pExpr)
                            pExpr = ExpressionPtr(pExpr1);
                        else
                            pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1, pExpr);
                    }
                }

                break;
            }

            case R_SwitchCase: {
                ExpressionPtr pExpr1;
                const auto pCase = i->pNode->as<SwitchCase>();
                const auto pSwitch = ::next(i)->pNode->as<Switch>();

                //case getExpressions more than 1, combining them in lemma with BOOL_OR
                for(size_t j = 0; j < pCase->getExpressions().size(); ++j) {
                    ExpressionPtr pExpr2;
                    const auto pCurrent = pCase->getExpressions().get(j);

                    if(pCurrent->getKind() != Expression::TYPE) {
                        pExpr2 = std::make_shared<Binary>(Binary::EQUALS, pSwitch->getArg(), pCurrent);
                    }
                    else {
                        if(pCurrent->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {
                            const auto range = pCurrent->as<TypeExpr>()->getContents()->as<Range>();

                            pExpr2 = std::make_shared<Binary>(Binary::BOOL_AND,
                                std::make_shared<Binary>(Binary::GREATER_OR_EQUALS, pSwitch->getArg(), range->getMin()),
                                std::make_shared<Binary>(Binary::LESS_OR_EQUALS,  pSwitch->getArg(), range->getMax()));
                        }
                    }

                    if(j == 0)
                        pExpr1 = pExpr2;
                    else
                        pExpr1 = std::make_shared<Binary>(Binary::BOOL_OR, pExpr1, pExpr2);
                }

                if(!pExpr)
                    pExpr = pExpr1;
                else
                    pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1, pExpr);

                break;
            }

            case R_ArrayIterationPart: {
                const auto conditions = i->pNode->as<ArrayPartDefinition>()->getConditions();
                const auto iterators = ::next(i)->pNode->as<ArrayIteration>()->getIterators();
                Collection<VariableReference> vars;
                ExpressionPtr pExpr1;

                for (size_t j = 0; j < iterators.size(); j++)
                    vars.add(std::make_shared<VariableReference>(iterators.get(j)));

                for (size_t i = 0; i < conditions.size(); i++) {
                    ExpressionPtr pExpr2;
                    ExpressionPtr pCurrent = conditions.get(i);

                    if(iterators.size() == 1)
                        pExpr2 = varBelongsSetOneDimension(vars.get(0), pCurrent);
                    else
                        pExpr2 = varsBelongSetSeveralDimensions(vars, pCurrent);

                    if(i == 0)
                        pExpr1 = pExpr2;
                    else
                        pExpr1 = std::make_shared<Binary>(Binary::BOOL_OR, pExpr1, pExpr2);
                }

                if(!pExpr)
                    pExpr = pExpr1;
                else
                    pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1, pExpr);

                break;
            }
            case R_ArrayIterationDefault: {
                const auto pArrayIt = ::next(i)->pNode->as<ArrayIteration>();
                const auto iterator = pArrayIt->getIterators().get(0);
                const auto pVar = std::make_shared<VariableReference>(iterator);
                const auto iterators = ::next(i)->pNode->as<ArrayIteration>()->getIterators();
                Collection<VariableReference> vars;
                ExpressionPtr pExpr1;

                for (size_t j = 0; j < iterators.size(); j++)
                    vars.add(std::make_shared<VariableReference>(iterators.get(j)));

                for (size_t i = 0; i < pArrayIt->size(); i++) {
                    for (size_t j = 0; j < pArrayIt->get(i)->getConditions().size(); j++) {

                        ExpressionPtr pCurrent = pArrayIt->get(i)->getConditions().get(j);

                        if(iterators.size() == 1)
                            pExpr1 = std::make_shared<Unary>(Unary::BOOL_NEGATE, varBelongsSetOneDimension(vars.get(0), pCurrent));
                        else
                            pExpr1 = std::make_shared<Unary>(Unary::BOOL_NEGATE, varsBelongSetSeveralDimensions(vars, pCurrent));

                        if(!pExpr)
                            pExpr = ExpressionPtr(pExpr1);
                        else
                            pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1, pExpr);
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    return pExpr;
}

///Creating modules for theory

int CollectPreConditions::handlePredicateDecl(NodePtr &_node) {
    m_pPredicate = _node->as<Predicate>();
    m_pNewModule = std::make_shared<Module>(m_pNameGen->makeNameSubmoduleForPredicate(*m_pPredicate));
    m_module->getModules().add(m_pNewModule);
    return 0;
}

int CollectPreConditions::handleProcessDecl(NodePtr &_node) {
    m_pProcess = _node->as<Process>();
    m_pNewModule = std::make_shared<Module>(m_pNameGen->makeNameSubmoduleForProcess(*m_pProcess));
    m_module->getModules().add(m_pNewModule);
    return 0;
}

bool CollectPreConditions::visitPredicateType(const PredicateTypePtr &_node) {
    m_pNewModule = std::make_shared<Module>(m_pNameGen->makeNameSubmoduleForType());
    m_module->getModules().add(m_pNewModule);
    return true;
}

bool CollectPreConditions::visitLambda(const LambdaPtr &_node) {
 /*   Lambda &_lambda = (Lambda &)_node;
    m_pPredicate = std::make_shared<Predicate((AnonymousPredicate &)_lambda.getPredicate());
    m_pPredicate->setName(m_pNameGen->makeNameLambdaToPredicate());

    m_pNewModule = std::make_shared<Module(m_pNameGen->makeNameSubmoduleForLambda());
    m_module.getModules().add(m_pNewModule);*/
    return true;
}


///Adding preconditions to module

int CollectPreConditions::handlePredicatePreCondition(NodePtr &_node) {

    m_pNewModule->getFormulas().add(na::declareFormula(
        m_pNameGen->makeNamePredicatePrecondition(*m_pPredicate),
        m_pPredicate, _node->as<Expression>()));

    return 0;
}

int CollectPreConditions::handlePredicateBranchPreCondition(NodePtr &_node) {

    m_pNewModule->getFormulas().add(na::declareFormula(
        m_pNameGen->makeNamePredicateBranchPrecondition(*m_pPredicate, getLoc().cPosInCollection),
        m_pPredicate, _node->as<Expression>()));

    return 0;
}

int CollectPreConditions::handlePredicateTypePreCondition(NodePtr &_node) {

    m_pNewModule->getFormulas().add(na::declareFormula(
        m_pNameGen->makeNameTypePreCondition(),
        m_pPredicate, _node->as<Expression>()));

    return 0;
}

int CollectPreConditions::handlePredicateTypeBranchPreCondition(NodePtr &_node) {

    m_pNewModule->getFormulas().add(na::declareFormula(
        m_pNameGen->makeNameTypeBranchPreCondition(getLoc().cPosInCollection),
        m_pPredicate, _node->as<Expression>()));

    return 0;
}

int CollectPreConditions::handleProcessBranchPreCondition(NodePtr &_node) {

    m_pNewModule->getFormulas().add(na::declareFormula(
        m_pNameGen->makeNameProcessBranchPreCondition(*m_pProcess, getLoc().cPosInCollection),
        m_pPredicate, _node->as<Expression>()));

    return 0;
}


///Generating lemmas

int CollectPreConditions::handleFunctionCallee(NodePtr &_node) {
    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(collectConditions(),
                                    std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
    return 0;
}

int CollectPreConditions::handlePredicateCallBranchResults(NodePtr &_node) {
//    ((PredicateType &)((Call &)::prev(m_path.end())).getPredicate()->getType()).getOutParams().get(m_path.end()->cPosInCollection)->get(0);
//    ((Expression &)_node).getType();
    return 0;
}

int CollectPreConditions::handlePredicateCallArgs(NodePtr &_node) {
    return 0;
}

bool CollectPreConditions::visitCall(const CallPtr &_node) {

    ExpressionPtr pCond = collectConditions();

    if(_node->getPredicate()->getKind() == Expression::PREDICATE) {

//compatibility of arguments
        for (size_t i = 0; i < _node->getArgs().size(); i++) {

            ExpressionPtr pCallArg = _node->getArgs().get(i);
            ParamPtr pPredParam = _node->getPredicate()->as<PredicateReference>()->getTarget()->as<Predicate>()->getInParams().get(i);

            ExpressionPtr pExpr;
            TypePtr pTypeCall = pCallArg->getType();
            TypePtr pTypePred = pPredParam->getType();

            pTypeCall = getNotNamedReferenceType(pTypeCall);
            pTypePred = getNotNamedReferenceType(pTypePred);

//for arrays
            if(pCallArg->getKind() == Expression::VAR) {

                if(pTypeCall && pTypeCall->getKind() == Type::PARAMETERIZED &&
                   pTypeCall->as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY &&
                   pTypePred && pTypePred->getKind() == Type::PARAMETERIZED &&
                   pTypePred->as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY) {

                    Collection<Range> rangesCall = arrayRangesWithCurrentParams(pCallArg);
                    ExpressionPtr pExpr;

                    Collection<Range> rangesPred, ranges;

//в разных случаях доступ к аргументам может быть и не таким
                    const auto params = pTypePred->as<ParameterizedType>()->getParams();
                    const auto pArray = pTypePred->as<ParameterizedType>()->getActualType()
                        ->as<ArrayType>();
                    if (pArray)
                        getRanges(*pArray, ranges);

                    Collection<Expression> args = pPredParam->getType()->as<NamedReferenceType>()->getArgs();

                    for (size_t j = 0; j < ranges.size(); j++) {
                        RangePtr pNewRange = std::make_shared<Range>(ranges.get(j)->getMin(), ranges.get(j)->getMax());

                        for (size_t l = 0; l < params.size(); l++) {
                            pNewRange = Expression::substitute(pNewRange, std::make_shared<VariableReference>(params.get(l)), args.get(l))->as<Range>();
                        }

                        rangesPred.add(pNewRange);
                    }

                    if(rangesCall.size() == rangesPred.size()) {
                        for (size_t l = 0; l < rangesCall.size(); l++) {
                            if(l == 0)
                                pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
                                    std::make_shared<Binary>(Binary::EQUALS, rangesCall.get(0)->getMin(), rangesPred.get(0)->getMin()),
                                    std::make_shared<Binary>(Binary::EQUALS, rangesCall.get(0)->getMax(), rangesPred.get(0)->getMax()));
                            else
                                pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr, std::make_shared<Binary>(Binary::BOOL_AND,
                                    std::make_shared<Binary>(Binary::EQUALS, rangesCall.get(l)->getMin(), rangesPred.get(l)->getMin()),
                                    std::make_shared<Binary>(Binary::EQUALS, rangesCall.get(l)->getMax(), rangesPred.get(l)->getMax())));
                        }

                        if(pCond)
                            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                                std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr),
                                std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                        else
                            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                                std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                    }
                    ///error lemma
                    else {
                        LiteralPtr pLiteral = std::make_shared<Literal>(Number("0" , Number::INTEGER));

                        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                            std::make_shared<Binary>(Binary::NOT_EQUALS, pLiteral, pLiteral),
                            std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                    }
                }
            }

//for subtypes
            if(pTypeCall && pTypeCall->getKind() == Type::SUBTYPE && pTypePred && pTypePred->getKind() == Type::SUBTYPE) {

                const auto pExprCall = pTypeCall->as<Subtype>()->getExpression();
                VarSubstitute substitute(pTypeCall->as<Subtype>()->getParam(), pCallArg);
                substitute.traverseNode(pExprCall);

                const auto pExprPred = pTypePred->as<Subtype>()->getExpression();
                substitute = VarSubstitute(pTypePred->as<Subtype>()->getParam(), pCallArg);
                substitute.traverseNode(pExprPred);

                if(pCond)
                    pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pCond, pExprPred);
                else
                    pExpr = pExprPred;

                pExpr = std::make_shared<Binary>(Binary::IFF, pExpr, pExprCall);

                m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                    std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
            }

//for predicate
//в леммы подставляются Formula а не Expression
//посмотреть, что еще тут может быть
            if(((pCallArg->getKind() == Expression::TYPE && 
                pCallArg->as<TypeExpr>()->getContents()->getKind() == Type::PREDICATE) ||
                (pCallArg->getKind() == Expression::LAMBDA)) &&
                pPredParam->getType()->getKind() == Type::PREDICATE) {

                FormulaPtr pPre, pPost;

                if(pCallArg->getKind() == Expression::LAMBDA) {
                    pPre = pCallArg->as<Lambda>()->getPredicate().getPreCondition();
                    pPost = pCallArg->as<Lambda>()->getPredicate().getPostCondition();
                }

                if(pCallArg->getKind() == Expression::TYPE && 
                    pCallArg->as<TypeExpr>()->getContents()->getKind() == Type::PREDICATE) {
                    pPre = pCallArg->as<TypeExpr>()->getContents()->as<PredicateType>()->getPreCondition();
                    pPost = pCallArg->as<TypeExpr>()->getContents()->as<PredicateType>()->getPostCondition();
                }

                const auto pArgPred = pPredParam->getType()->as<PredicateType>();
                ExpressionPtr pExpr;

                if(pArgPred->getPreCondition()) {

                    pExpr = pArgPred->getPreCondition();

                    if(pPre)
                        pExpr = std::make_shared<Binary>(Binary::IMPLIES, pExpr, pPre);
                }

                if(pExpr) {
                    if(pCond)
                        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                            std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr),
                            std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                    else
                        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                            std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                }

                if(pPost) {

                    pExpr = pPost->getSubformula();

                    if(pArgPred->getPostCondition())
                        pExpr = std::make_shared<Binary>(Binary::IMPLIES, pExpr, pArgPred->getPostCondition());
                }

                if(pExpr) {
                    if(pCond)
                        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                            std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr),
                            std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                    else
                        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                            std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                }
            }
        }

//compatibility of results
        for (size_t i = 0; i < _node->getBranches().size(); i++) {
            for (size_t j = 0; j < _node->getBranches().get(i)->size(); j++) {

                const auto pCallArg = _node->getBranches().get(i)->get(j);   //kind = VAR
                const auto pPredParam = _node->getPredicate()->as<PredicateReference>()->getTarget()
                    ->as<Predicate>()->getOutParams().get(i)->get(j);   //kind = Param

                ExpressionPtr pExpr;
                auto pTypeCall = pCallArg->getType();
                auto pTypePred = pPredParam->getType();

                pTypeCall = getNotNamedReferenceType(pTypeCall);
                pTypePred = getNotNamedReferenceType(pTypePred);

//for arrays
                if(pCallArg->getKind() == Expression::VAR) {

                    if(pTypeCall && pTypeCall->getKind() == Type::PARAMETERIZED &&
                       pTypeCall->as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY &&
                       pTypePred && pTypePred->getKind() == Type::PARAMETERIZED &&
                       pTypePred->as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY) {

                        Collection<Range> rangesCall = arrayRangesWithCurrentParams(pCallArg);
                        ExpressionPtr pExpr;

                        Collection<Range> rangesPred, ranges;

//в разных случаях доступ к аргументам может быть и не таким
                        const auto params = pTypePred->as<ParameterizedType>()->getParams();

                        const auto pArray = pTypePred->as<ParameterizedType>()->getActualType()->as<ArrayType>();
                        if (pArray)
                            getRanges(*pArray, ranges);

                        const auto args = pPredParam->getType()->as<NamedReferenceType>()->getArgs();

                        for (size_t k = 0; k < ranges.size(); k++) {
                            auto pNewRange = std::make_shared<Range>(ranges.get(k)->getMin(), ranges.get(k)->getMax());

                            for (size_t l = 0; l < params.size(); l++) {
                                pNewRange = Expression::substitute(pNewRange, std::make_shared<VariableReference>(params.get(l)), args.get(l))->as<Range>();
                            }

                            rangesPred.add(pNewRange);
                        }

                        if(rangesCall.size() == rangesPred.size()) {
                            for (size_t l = 0; l < rangesCall.size(); l++) {
                                if(l == 0)
                                    pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
                                        std::make_shared<Binary>(Binary::EQUALS, rangesCall.get(0)->getMin(), rangesPred.get(0)->getMin()),
                                        std::make_shared<Binary>(Binary::EQUALS, rangesCall.get(0)->getMax(), rangesPred.get(0)->getMax()));
                                else
                                    pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr, std::make_shared<Binary>(Binary::BOOL_AND,
                                        std::make_shared<Binary>(Binary::EQUALS, rangesCall.get(l)->getMin(), rangesPred.get(l)->getMin()),
                                        std::make_shared<Binary>(Binary::EQUALS, rangesCall.get(l)->getMax(), rangesPred.get(l)->getMax())));
                            }

                            if(pCond)
                                m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                                    std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr),
                                    std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                            else
                                m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                                    std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                        }
                        //error lemma
                        else {
                            const auto pLiteral = std::make_shared<Literal>(Number("0" , Number::INTEGER));

                            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                                std::make_shared<Binary>(Binary::NOT_EQUALS, pLiteral, pLiteral),
                                std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                        }
                    }
                }

//for subtypes
                if(pTypeCall && pTypeCall->getKind() == Type::SUBTYPE && pTypePred && pTypePred->getKind() == Type::SUBTYPE) {

                    const auto pExprCall = pTypeCall->as<Subtype>()->getExpression();
                    VarSubstitute substitute(pTypeCall->as<Subtype>()->getParam(), pCallArg);
                    substitute.traverseNode(pExprCall);

                    const auto pExprPred = pTypePred->as<Subtype>()->getExpression();
                    substitute = VarSubstitute(pTypePred->as<Subtype>()->getParam(), pCallArg);
                    substitute.traverseNode(pExprPred);

                    if(pCond)
                        pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pCond, pExprPred);
                    else
                        pExpr = pExprPred;

                    pExpr = std::make_shared<Binary>(Binary::IMPLIES, pExpr, pExprCall);

                    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                        std::make_shared<Label>(m_pNameGen->makeNameLemmaCall())));
                }
            }
        }
    }

//предикатов ведь не может быть в результатах?********************************************************
    return true;
}

bool CollectPreConditions::visitSwitch(const SwitchPtr &_node) {

    for(size_t i = 0; i < _node->size(); ++i) {
        for(size_t k = 0; k < _node->get(i)->getExpressions().size(); ++k) {
            for(size_t j = i; j < _node->size(); ++j) {

                size_t l = (i == j) ? k+1: 0;
                for(; l < _node->get(j)->getExpressions().size(); ++l) {

                    const auto pExpr1 = _node->get(i)->getExpressions().get(k);
                    const auto pExpr2 = _node->get(j)->getExpressions().get(l);

                    caseNonintersection(pExpr1, pExpr2);
                }
            }
        }
    }
    return true;
}

bool CollectPreConditions::visitIf(const IfPtr &_node) {
    ExpressionPtr pExprIf, pExprElse;

    if((_node->getArg()->getKind() == Expression::UNARY) && (_node->getArg()->as<Unary>()->getOperator() == Unary::BOOL_NEGATE))
        pExprElse = _node->getArg()->as<Unary>()->getExpression();
    else
        pExprElse = std::make_shared<Unary>(Unary::BOOL_NEGATE, _node->getArg());
//использовать Мишин оптимизатор выражений, когда смержиться

    if(ExpressionPtr pCond = collectConditions()) {
        pExprIf = std::make_shared<Binary>(Binary::BOOL_AND, pCond, _node->getArg());
        pExprElse = std::make_shared<Binary>(Binary::BOOL_AND, pCond, pExprElse);
    }
    else
        pExprIf = _node->getArg();
        //pExprElse already initialized

    NamedValues params;
    na::collectValues(pExprIf, params);
    const auto pFormulaIf = std::make_shared<Formula>(Formula::EXISTENTIAL, pExprIf);
    const auto pFormulaElse = std::make_shared<Formula>(Formula::EXISTENTIAL, pExprElse);
    pFormulaIf->getBoundVariables().append(params);
    pFormulaElse->getBoundVariables().append(params);

    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pFormulaIf,
                                                       std::make_shared<Label>(m_pNameGen->makeNameLemmaIf())));

    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pFormulaElse,
                                                       std::make_shared<Label>(m_pNameGen->makeNameLemmaIf())));
    return true;
}

int CollectPreConditions::handleSwitchDefault(NodePtr &_node) {
    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(collectConditions(),
                                    std::make_shared<Label>(m_pNameGen->makeNameLemmaSwitchDefault())));
    return 0;
}

int CollectPreConditions::handleSwitchCase(NodePtr &_node) {
 //   m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration(collectConditions(),
   //                                 std::make_shared<Label(m_pNameGen->makeNameLemmaSwitchCase())));
    return 0;
}

int CollectPreConditions::handleUnionConsFields(NodePtr &_node) {
    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(collectConditions(),
                                    std::make_shared<Label>(m_pNameGen->makeNameLemmaUnionConsField())));
    return 0;
}

//x[j] => lemma 1 <= j <= n
bool CollectPreConditions::visitArrayPartExpr(const ArrayPartExprPtr &_node) {

    ExpressionPtr pExpr;
    const auto pArrayRanges = arrayRangesWithCurrentParams(_node->getObject());

    for (size_t i = 0; i < _node->getIndices().size(); i++) {

        if (_node->getIndices().get(i)->getKind() != Expression::TYPE) {
            pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
                std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pArrayRanges.get(i)->getMin(), _node->getIndices().get(i)),
                std::make_shared<Binary>(Binary::LESS_OR_EQUALS, _node->getIndices().get(i), pArrayRanges.get(i)->getMax()));
        } else if (_node->getIndices().get(i)->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {
            const auto pRange = _node->getIndices().get(i)->as<TypeExpr>()->getContents()->as<Range>();

            pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
                std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pArrayRanges.get(i)->getMin(), pRange->getMin()),
                std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRange->getMax(), pArrayRanges.get(i)->getMax()));
        }

        if(const auto pCond = collectConditions())
            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr),
                std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayPartIndex())));
        else
            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayPartIndex())));
    }

    return true;
}

///ar1 = ar [k: ar[m], m: ar[k] ]  =>  lemmas 1 <= k <= n & 1 <= m <= n  &  k != m
bool CollectPreConditions::visitReplacement(const ReplacementPtr &_node) {

    if(_node->getNewValues()->getConstructorKind() != Constructor::ARRAY_ELEMENTS)
        return true;

    const auto pRangeArray = arrayRangeWithCurrentParams(_node->getObject());
    const auto pRangeArrays = arrayRangesWithCurrentParams(_node->getObject());

    //case 1 dimension
    if(pRangeArrays.size() == 1) {
        for (size_t i = 0; i < _node->getNewValues()->as<ArrayConstructor>()->size(); i++) {

            const auto pExpr1 = _node->getNewValues()->as<ArrayConstructor>()->get(i)->as<ElementDefinition>()->getIndex();

            const auto pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
                std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRangeArray->getMin(), pExpr1),
                std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pExpr1, pRangeArray->getMax()));

            if(ExpressionPtr pCond = collectConditions())
                m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                    std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr),
                    std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayMod())));
            else
                m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                    std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayMod())));

            for (size_t j = i + 1; j < _node->getNewValues()->as<ArrayConstructor>()->size(); j++) {

                const auto pExpr2 = _node->getNewValues()->as<ArrayConstructor>()->get(j)->as<ElementDefinition>()->getIndex();

                m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                    std::make_shared<Binary>(Binary::NOT_EQUALS, pExpr1, pExpr2),
                    std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayMod())));
            }
        }
    }
    //case several dimensions
    else {
        for (size_t i = 0; i < _node->getNewValues()->as<ArrayConstructor>()->size(); i++) {
            ExpressionPtr pExpr;
            const auto pExpr1 = _node->getNewValues()->as<ArrayConstructor>()->get(i)->as<ElementDefinition>()->getIndex();

            if (pExpr1->getKind() == Expression::CONSTRUCTOR &&
                pExpr1->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

                for (size_t j = 0; j < pExpr1->as<StructConstructor>()->size(); j++) {
                    const auto pExpr2 = pExpr1->as<StructConstructor>()->get(j)->getValue();

                    pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRangeArrays.get(j)->getMin(), pExpr2),
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pExpr2, pRangeArrays.get(j)->getMax()));

                    if (const auto pCond = collectConditions())
                        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                            std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr),
                            std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayMod())));
                    else
                        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                            std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayMod())));
                }

                for (size_t j = i + 1; j < _node->getNewValues()->as<ArrayConstructor>()->size(); j++) {
                    const auto pExpr2 = _node->getNewValues()->as<ArrayConstructor>()->get(j)->as<ElementDefinition>()->getIndex();

                    if (pExpr2->getKind() == Expression::CONSTRUCTOR &&
                        pExpr2->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS &&
                        pExpr1->as<StructConstructor>()->size() ==
                        pExpr2->as<StructConstructor>()->size()) {

                        for (size_t j = 0; j < pExpr1->as<StructConstructor>()->size(); j++) {
                            const auto pExpr3 = pExpr1->as<StructConstructor>()->get(j)->getValue();
                            const auto pExpr4 = pExpr2->as<StructConstructor>()->get(j)->getValue();

                            if(j == 0)
                                pExpr = std::make_shared<Binary>(Binary::NOT_EQUALS, pExpr3, pExpr4);
                            else
                                pExpr = std::make_shared<Binary>(Binary::BOOL_OR, pExpr,
                                    std::make_shared<Binary>(Binary::NOT_EQUALS, pExpr3, pExpr4));
                        }
                        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                            std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayMod())));
                    }
                }
            }
        }
    }
    return true;
}

bool CollectPreConditions::visitAssignment(const AssignmentPtr &_node) {
//для ArrayConstrucor надо в visitArrayIteration
    ExpressionPtr pCond = collectConditions();
//lemmas for ArrayConstrucor
//x = for(... var j ...) { case A1 : ... case An : ...}
    if (_node->getExpression()->getKind() == Expression::CONSTRUCTOR &&
       _node->getExpression()->as<Constructor>()->getConstructorKind() == Constructor::ARRAY_ITERATION) {

        const auto pArrayIt = _node->getExpression()->as<ArrayIteration>();

        const auto pRangeArray = arrayRangeWithCurrentParams(_node->getLValue());
        const auto pRangeArrays = arrayRangesWithCurrentParams(_node->getLValue());

//Ai doesn't intersect with Aj
        for (size_t i = 0; i < pArrayIt->size(); ++i) {
            for (size_t k = 0; k < pArrayIt->get(i)->getConditions().size(); ++k) {
                for (size_t j = i; j < pArrayIt->size(); ++j) {
                    size_t l = (i == j) ? k+1: 0;
                    for (; l < pArrayIt->get(j)->getConditions().size(); ++l) {
                        const auto pExpr1 = pArrayIt->get(i)->getConditions().get(k);
                        const auto pExpr2 = pArrayIt->get(j)->getConditions().get(l);
                        ExpressionPtr pExpr3;

                        //size() == 1  correct?
                        if (pArrayIt->getIterators().size() == 1) {
                            pExpr3 = caseNonintersection(pExpr1, pExpr2);
                        } else {
                            if (pExpr1->getKind() == Expression::CONSTRUCTOR &&
                               pExpr2->getKind() == Expression::CONSTRUCTOR &&
                               pExpr1->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS &&
                               pExpr2->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

                                for (size_t m = 0; m < pArrayIt->getIterators().size(); m++) {
                                     const auto pExpr4 = pExpr1->as<StructConstructor>()->get(m)->getValue();
                                     const auto pExpr5 = pExpr2->as<StructConstructor>()->get(m)->getValue();
                                     if (m == 0)
                                         pExpr3 = caseNonintersection(pExpr4, pExpr5);
                                     else
                                         pExpr3 = std::make_shared<Binary>(Binary::BOOL_OR, pExpr3, caseNonintersection(pExpr4, pExpr5));
                                }
                            }
                        }

                        if (pCond)
                            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                                std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr3),
                                std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayCons())));
                        else
                            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr3,
                                std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayCons())));
                    }
                }
            }
        }

//Ai contains in array's range
        for (size_t i = 0; i < pArrayIt->size(); ++i) {
            for (size_t k = 0; k < pArrayIt->get(i)->getConditions().size(); ++k) {

                const auto pExpr1 = pArrayIt->get(i)->getConditions().get(k);
                ExpressionPtr pExpr2;

                if (pArrayIt->getIterators().size() == 1) {
                    if (pExpr1->getKind() == Expression::TYPE &&
                        pExpr1->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                        const auto pRange = pExpr1->as<TypeExpr>()->getContents()->as<Range>();

                        pExpr2 = std::make_shared<Binary>(Binary::BOOL_AND,
                            std::make_shared<Binary>(Binary::GREATER_OR_EQUALS, pRange->getMin(), pRangeArray->getMin()),
                            std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRange->getMax(), pRangeArray->getMax()));
                    }
                    else {
                        pExpr2 = std::make_shared<Binary>(Binary::BOOL_AND,
                            std::make_shared<Binary>(Binary::GREATER_OR_EQUALS, pExpr1, pRangeArray->getMin()),
                            std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pExpr1, pRangeArray->getMax()));
                    }
                }
                else {
                    if (pExpr1->getKind() == Expression::CONSTRUCTOR &&
                       pExpr1->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

                        for (size_t m = 0; m < pArrayIt->getIterators().size(); m++) {
                            const auto pExpr3 = pExpr1->as<StructConstructor>()->get(m)->getValue();
                            ExpressionPtr pExpr4;

                            if(pExpr3->getKind() == Expression::TYPE &&
                               pExpr3->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                                const auto pRange = pExpr3->as<TypeExpr>()->getContents()->as<Range>();

                                pExpr4 = std::make_shared<Binary>(Binary::BOOL_AND,
                                    std::make_shared<Binary>(Binary::GREATER_OR_EQUALS, pRange->getMin(), pRangeArrays.get(m)->getMin()),
                                    std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRange->getMax(), pRangeArrays.get(m)->getMax()));
                            } else {
                                pExpr4 = std::make_shared<Binary>(Binary::BOOL_AND,
                                    std::make_shared<Binary>(Binary::GREATER_OR_EQUALS, pExpr3, pRangeArrays.get(m)->getMin()),
                                    std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pExpr3, pRangeArrays.get(m)->getMax()));
                            }

                            if (m == 0)
                                pExpr2 = pExpr4;
                            else
                                pExpr2 = std::make_shared<Binary>(Binary::BOOL_AND, pExpr2, pExpr4);
                        }
                    }
                }

                if (pCond)
                    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                        std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr2),
                        std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayCons())));
                else
                    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr2,
                        std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayCons())));
            }
        }

//case ArrayConstrucor doesn't have default
        if (!pArrayIt->getDefault()) {
            ExpressionPtr pExpr, pExpr1;

            for (size_t i = 0; i < pArrayIt->size(); ++i) {
                for (size_t k = 0; k < pArrayIt->get(i)->getConditions().size(); ++k) {

                const auto pExpr2 = pArrayIt->get(i)->getConditions().get(k);

                if (pArrayIt->getIterators().size() == 1) {
                    const auto pVar = std::make_shared<VariableReference>(pArrayIt->getIterators().get(0));

                    if (pExpr2->getKind() == Expression::TYPE &&
                        pExpr2->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                        const auto pRange = pExpr2->as<TypeExpr>()->getContents()->as<Range>();

                        pExpr1 = std::make_shared<Binary>(Binary::BOOL_AND,
                            std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRange->getMin(), pVar),
                            std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pVar, pRange->getMax()));
                    } else {
                        pExpr1 = std::make_shared<Binary>(Binary::EQUALS, pVar, pExpr2);
                    }
                } else {
                    if (pExpr2->getKind() == Expression::CONSTRUCTOR &&
                       pExpr2->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

                        for (size_t m = 0; m < pArrayIt->getIterators().size(); m++) {
                            const auto pVar = std::make_shared<VariableReference>(pArrayIt->getIterators().get(m));
                            const auto pExpr3 = pExpr2->as<StructConstructor>()->get(m)->getValue();
                            ExpressionPtr pExpr4;

                            if (pExpr3->getKind() == Expression::TYPE &&
                               pExpr3->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                                const auto pRange = pExpr3->as<TypeExpr>()->getContents()->as<Range>();

                                pExpr4 = std::make_shared<Binary>(Binary::BOOL_AND,
                                    std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRange->getMin(), pVar),
                                    std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pVar, pRange->getMax()));
                            } else {
                                pExpr4 = std::make_shared<Binary>(Binary::EQUALS, pVar, pExpr3);
                            }

                            if (m == 0)
                                pExpr1 = pExpr4;
                            else
                                pExpr1 = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1, pExpr4);
                        }
                    }
                }

                if (i == 0 && k == 0)
                    pExpr = pExpr1;
                else
                    pExpr = std::make_shared<Binary>(Binary::BOOL_OR, pExpr, pExpr1);
                }
            }

            for (size_t m = 0; m < pArrayIt->getIterators().size(); m++) {
                const auto pVar = std::make_shared<VariableReference>(pArrayIt->getIterators().get(m));
                if(m == 0)
                    pExpr1 = std::make_shared<Binary>(Binary::BOOL_AND,
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRangeArrays.get(0)->getMin(), pVar),
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pVar, pRangeArrays.get(0)->getMax()));
                else
                    pExpr1 = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1,
                        std::make_shared<Binary>(Binary::BOOL_AND,
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRangeArrays.get(m)->getMin(), pVar),
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pVar, pRangeArrays.get(m)->getMax())));
            }

            pExpr = std::make_shared<Binary>(Binary::IFF, pExpr1, pExpr);

            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayCons())));
        }
    }

//Compatibility
    if (_node->getExpression()->getKind() == Expression::VAR) {
        ExpressionPtr pExpr;
        auto pTypeL = _node->getLValue()->getType();
        auto pTypeR = _node->getExpression()->getType();

        if (pTypeL->getKind() == Type::NAMED_REFERENCE)
            pTypeL = pTypeL->as<NamedReferenceType>()->getDeclaration()->getType();

        if (pTypeR->getKind() == Type::NAMED_REFERENCE)
            pTypeR = pTypeR->as<NamedReferenceType>()->getDeclaration()->getType();

///for subtypes
        if (pTypeL->getKind() == Type::SUBTYPE && pTypeR->getKind() == Type::SUBTYPE) {
            const auto pExprL = pTypeL->as<Subtype>()->getExpression();
            VarSubstitute substitute(pTypeL->as<Subtype>()->getParam(), _node->getExpression());
            substitute.traverseNode(pExprL);

            auto pExprR = pTypeR->as<Subtype>()->getExpression();
            substitute = VarSubstitute(pTypeR->as<Subtype>()->getParam(), _node->getExpression());
            substitute.traverseNode(pExprR);

            if(pCond)
                pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pCond, pExprR);
            else
                pExpr = pExprR;

            pExpr = std::make_shared<Binary>(Binary::IMPLIES, pExpr, pExprL);

            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                std::make_shared<Label>(m_pNameGen->makeNameLemmaAssignment())));
        }

///for arrays
        if (pTypeL->getKind() == Type::PARAMETERIZED &&
           pTypeL->as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY &&
           pTypeR->getKind() == Type::PARAMETERIZED &&
           pTypeR->as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY) {

            const auto rangesL = arrayRangesWithCurrentParams(_node->getLValue());
            const auto rangesR = arrayRangesWithCurrentParams(_node->getExpression());
            ExpressionPtr pExpr;

            if(rangesL.size() == rangesR.size()) {
                for (size_t i = 0; i < rangesL.size(); i++) {
                    if(i == 0)
                        pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
                            std::make_shared<Binary>(Binary::EQUALS, rangesL.get(0)->getMin(), rangesR.get(0)->getMin()),
                            std::make_shared<Binary>(Binary::EQUALS, rangesL.get(0)->getMax(), rangesR.get(0)->getMax()));
                    else
                        pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr, std::make_shared<Binary>(Binary::BOOL_AND,
                            std::make_shared<Binary>(Binary::EQUALS, rangesL.get(i)->getMin(), rangesR.get(i)->getMin()),
                            std::make_shared<Binary>(Binary::EQUALS, rangesL.get(i)->getMax(), rangesR.get(i)->getMax())));
                }

                if(pCond)
                    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                        std::make_shared<Binary>(Binary::IMPLIES, pCond, pExpr),
                        std::make_shared<Label>(m_pNameGen->makeNameLemmaAssignment())));
                else
                    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExpr,
                        std::make_shared<Label>(m_pNameGen->makeNameLemmaAssignment())));
            }
            ///error lemma
            else {
                const auto pLiteral = std::make_shared<Literal>(Number("0" , Number::INTEGER));

                m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
                    std::make_shared<Binary>(Binary::NOT_EQUALS, pLiteral, pLiteral),
                    std::make_shared<Label>(m_pNameGen->makeNameLemmaAssignment())));
            }
        }
    }

///lemmas for union of arrays
    if (_node->getExpression()->getKind() == Expression::BINARY) {

        Collection<Expression> exprs;
        auto pExpr = _node->getExpression();
        const ExpressionPtr pExpr2;
        ExpressionPtr pExpr1, pExprEqual, pExprNonintersect;

        while (pExpr->getKind() == Expression::BINARY) {
            exprs.add(pExpr->as<Binary>()->getRightSide());
            pExpr = pExpr->as<Binary>()->getLeftSide();
        }
        exprs.add(pExpr);

//checking that expressions are arrays and have same dimensions
        bool bArrays = true;
        size_t dim;

        for (size_t i = 0; i < exprs.size(); i++) {

            auto pType = exprs.get(i)->getType();

            if (pType->getKind() == Type::NAMED_REFERENCE)
                pType = pType->as<NamedReferenceType>()->getDeclaration()->getType();

            if (!(pType && pType->getKind() == Type::PARAMETERIZED &&
                pType->as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY))

                bArrays = false;

            const auto ranges = arrayRangesWithCurrentParams(exprs.get(i));
            if (i == 0)
                dim = ranges.size();
            else if(dim != ranges.size())
                bArrays = false;
        }

        if (bArrays) {
            for (size_t i = exprs.size(); i > 0 ; i--) {
                const auto ranges = arrayRangesWithCurrentParams(exprs.get(i - 1));

                for (size_t j = 0; j < ranges.size(); j++) {

                    std::wstring strName = fmtInt(j+1, L"i%u");
                    const auto pVar = std::make_shared<VariableReference>(strName);

                    if(j == 0)
                        pExpr1 = std::make_shared<Binary>(Binary::BOOL_AND,
                            std::make_shared<Binary>(Binary::LESS_OR_EQUALS, ranges.get(0)->getMin(), pVar),
                            std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pVar, ranges.get(0)->getMax()));
                    else
                        pExpr1 = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1,
                            std::make_shared<Binary>(Binary::BOOL_AND,
                            std::make_shared<Binary>(Binary::LESS_OR_EQUALS, ranges.get(j)->getMin(), pVar),
                            std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pVar, ranges.get(j)->getMax())));
                }

                if (i == exprs.size())
                    pExprEqual = pExpr1;
                else
                    pExprEqual = std::make_shared<Binary>(Binary::BOOL_OR, pExprEqual, pExpr1);

                //nonintersection
                for (size_t j = i - 1; j > 0 ; j--) {

                    Collection<Range> ranges1 = arrayRangesWithCurrentParams(exprs.get(j - 1));

                    for (size_t k = 0; k < ranges.size(); k++) {

                        if(k == 0)
                            pExprNonintersect = std::make_shared<Binary>(Binary::BOOL_OR,
                                std::make_shared<Binary>(Binary::LESS, ranges.get(k)->getMax(), ranges1.get(k)->getMin()),
                                std::make_shared<Binary>(Binary::GREATER, ranges.get(k)->getMin(), ranges1.get(k)->getMax()));
                        else
                            pExprNonintersect = std::make_shared<Binary>(Binary::BOOL_OR, pExprNonintersect,
                                std::make_shared<Binary>(Binary::BOOL_OR,
                                std::make_shared<Binary>(Binary::LESS, ranges.get(k)->getMax(), ranges1.get(k)->getMin()),
                                std::make_shared<Binary>(Binary::GREATER, ranges.get(k)->getMin(), ranges1.get(k)->getMax())));
                    }

                    m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExprNonintersect,
                        std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayUnion())));
                }
            }

            const auto ranges = arrayRangesWithCurrentParams(_node->getLValue());

            for (size_t j = 0; j < ranges.size(); j++) {

                std::wstring strName = fmtInt(j+1, L"i%u");
                const auto pVar = std::make_shared<VariableReference>(strName);

                if(j == 0)
                    pExpr1 = std::make_shared<Binary>(Binary::BOOL_AND,
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, ranges.get(0)->getMin(), pVar),
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pVar, ranges.get(0)->getMax()));
                else
                    pExpr1 = std::make_shared<Binary>(Binary::BOOL_AND, pExpr1,
                        std::make_shared<Binary>(Binary::BOOL_AND,
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, ranges.get(j)->getMin(), pVar),
                        std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pVar, ranges.get(j)->getMax())));
            }
            pExprEqual = std::make_shared<Binary>(Binary::IFF, pExpr1, pExprEqual);

            m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(pExprEqual,
                std::make_shared<Label>(m_pNameGen->makeNameLemmaArrayUnion())));
        }
    }
    return true;
}

// x/y => lemma y != 0
bool CollectPreConditions::visitBinary(const BinaryPtr &_node) {
    if (_node->getOperator() == Binary::DIVIDE) {
        const auto pLiteral = std::make_shared<Literal>(Number("0" , Number::INTEGER));

        m_pNewModule->getLemmas().add(std::make_shared<LemmaDeclaration>(
            std::make_shared<Binary>(Binary::IMPLIES, collectConditions(),
            std::make_shared<Binary>(Binary::NOT_EQUALS, _node->getRightSide(), pLiteral)),
            std::make_shared<Label>(m_pNameGen->makeNameLemmaDivide())));
    }

    return true;
}

bool CollectPreConditions::visitVariableDeclaration(const VariableDeclarationPtr &_node) {
    return true;
}

bool CollectPreConditions::visitNamedValue(const NamedValuePtr &_node) {
    auto pType = _node->getType();
    if (pType && pType->getKind() == Type::NAMED_REFERENCE) {
        pType = pType->as<NamedReferenceType>()->getDeclaration()->getType();

        if (pType && pType->getKind() == Type::PARAMETERIZED) {
            _node->setType(clone(_node->getType()));
            const auto namedRefArgs = _node->getType()->as<NamedReferenceType>()->getArgs();
            const auto paramTypeParams = pType->as<ParameterizedType>()->getParams();
            const auto paramTypeActualType = pType->as<ParameterizedType>()->getActualType();

            for (size_t i = 0; i < namedRefArgs.size() && i < paramTypeParams.size(); i++) {
                if (namedRefArgs.get(i)->getKind() == Expression::TYPE)
                    pType = namedRefArgs.get(i)->as<TypeExpr>()->getContents();

                VarSubstitute substitute(paramTypeParams.get(i), pType);
                substitute.traverseNode(paramTypeActualType);
            }
        }
    }
    return true;
}

//for ArrayConstructor and Switch
ExpressionPtr CollectPreConditions::caseNonintersection(const ExpressionPtr& _pExpr1, const ExpressionPtr& _pExpr2) {
    ExpressionPtr pExpr;

    if ((_pExpr1->getKind() != Expression::TYPE) && (_pExpr2->getKind() != Expression::TYPE))
        pExpr = std::make_shared<Binary>(Binary::NOT_EQUALS, _pExpr1, _pExpr2);

    if ((_pExpr1->getKind() != Expression::TYPE) && (_pExpr2->getKind() == Expression::TYPE)
        && (_pExpr2->as<TypeExpr>()->getContents()->getKind() == Type::RANGE)) {

        const auto pRange = _pExpr2->as<TypeExpr>()->getContents()->as<Range>();

        pExpr = std::make_shared<Binary>(Binary::BOOL_OR,
            std::make_shared<Binary>(Binary::LESS, _pExpr1, pRange->getMin()),
            std::make_shared<Binary>(Binary::GREATER, _pExpr1, pRange->getMax()));
    }

    if ((_pExpr2->getKind() != Expression::TYPE) && (_pExpr1->getKind() == Expression::TYPE)
        && (_pExpr1->as<TypeExpr>()->getContents()->getKind() == Type::RANGE)) {

        const auto pRange = _pExpr1->as<TypeExpr>()->getContents()->as<Range>();

        pExpr = std::make_shared<Binary>(Binary::BOOL_OR,
            std::make_shared<Binary>(Binary::LESS, _pExpr2, pRange->getMin()),
            std::make_shared<Binary>(Binary::GREATER, _pExpr2, pRange->getMax()));
    }

    if ((_pExpr1->getKind() == Expression::TYPE) && (_pExpr1->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) &&
        (_pExpr2->getKind() == Expression::TYPE) && (_pExpr2->as<TypeExpr>()->getContents()->getKind() == Type::RANGE)) {

        const auto pRange1 = _pExpr1->as<TypeExpr>()->getContents()->as<Range>();
        const auto pRange2 = _pExpr2->as<TypeExpr>()->getContents()->as<Range>();

        pExpr = std::make_shared<Binary>(Binary::BOOL_OR,
            std::make_shared<Binary>(Binary::LESS, pRange1->getMax(), pRange2->getMin()),
            std::make_shared<Binary>(Binary::GREATER, pRange1->getMin(), pRange2->getMax()));
    }

    return pExpr;
}

//into definition of array substitute params from current array
RangePtr CollectPreConditions::arrayRangeWithCurrentParams(const ExpressionPtr& _pArray) {
//TODO: Никита:    Попробуй разнести по разным функам подстановку аргументов в произвольный параметризованный тип и выдирание диапазонов из массива
    RangePtr pNewRange;

    if (_pArray->getKind() == Expression::VAR) {
        const auto params = _pArray->as<VariableReference>()->getTarget()->as<Param>()->
            getType()->as<NamedReferenceType>()->getDeclaration()->as<TypeDeclaration>()->
            getType()->as<ParameterizedType>()->getParams();

        Collection<Type> dims;
        _pArray->as<VariableReference>()->getTarget()->as<Param>()->
            getType()->as<NamedReferenceType>()->getDeclaration()->as<TypeDeclaration>()->
            getType()->as<ParameterizedType>()->getActualType()->as<ArrayType>()->getDimensions(dims);
        const auto pType = getNotNamedReferenceType(dims.get(0));
        RangePtr pRange;
        if (pType && pType->getKind() == Type::SUBTYPE)
            pRange = pType->as<Subtype>()->asRange();

        const auto args = _pArray->as<VariableReference>()->getTarget()->as<Param>()->
            getType()->as<NamedReferenceType>()->getArgs();

        pNewRange = std::make_shared<Range>(pRange->getMin(), pRange->getMax());

        for (size_t i = 0; i < params.size(); i++) {
            pNewRange = Expression::substitute(pNewRange, std::make_shared<VariableReference>(params.get(i)), args.get(i))->as<Range>();
        }
    }

    return pNewRange;
}

Collection<Range> CollectPreConditions::arrayRangesWithCurrentParams(const ExpressionPtr& _pArray) {
    Collection<Range> newRanges;

    if (_pArray->getKind() == Expression::VAR) {
        const auto params = _pArray->as<VariableReference>()->getTarget()->as<Param>()->
            getType()->as<NamedReferenceType>()->getDeclaration()->as<TypeDeclaration>()->
            getType()->as<ParameterizedType>()->getParams();

        const auto pArray = _pArray->as<VariableReference>()->getTarget()->as<Param>()->
            getType()->as<NamedReferenceType>()->getDeclaration()->as<TypeDeclaration>()->
            getType()->as<ParameterizedType>()->getActualType()->as<ArrayType>();

        Collection<Range> ranges;

        if (pArray)
            getRanges(*pArray, ranges);

        const auto args = _pArray->as<VariableReference>()->getTarget()->as<Param>()->
            getType()->as<NamedReferenceType>()->getArgs();

        //if borders of range is complicated, then /Collection<Range> ranges/ changes
        //попробовать ranges.clone()
        for (size_t j = 0; j < ranges.size(); j++) {
            RangePtr pNewRange = std::make_shared<Range>(ranges.get(j)->getMin(), ranges.get(j)->getMax());

            for (size_t i = 0; i < params.size(); i++) {
                pNewRange = Expression::substitute(pNewRange, std::make_shared<VariableReference>(params.get(i)), args.get(i))->as<Range>();
            }

            newRanges.add(pNewRange);
        }
    }

    return newRanges;
}

ExpressionPtr CollectPreConditions::varBelongsSetOneDimension(const VariableReferencePtr& _pVar, const ExpressionPtr& _pExpr) {

    ExpressionPtr pExpr;

    if (_pExpr->getKind() != Expression::TYPE) {
        pExpr = std::make_shared<Binary>(Binary::EQUALS, _pVar, _pExpr);
    } else {
        if(_pExpr->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {
            const auto range = _pExpr->as<TypeExpr>()->getContents()->as<Range>();

            pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
                std::make_shared<Binary>(Binary::LESS_OR_EQUALS, range->getMin(), _pVar),
                std::make_shared<Binary>(Binary::LESS_OR_EQUALS, _pVar, range->getMax()));
        }
    }
    return pExpr;
}

ExpressionPtr CollectPreConditions::varsBelongSetSeveralDimensions(const Collection<VariableReference>& _vars, const ExpressionPtr& _pExpr) {
    ExpressionPtr pExpr;

    if(_pExpr->getKind() == Expression::CONSTRUCTOR &&
        _pExpr->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

        for (size_t m = 0; m < _vars.size(); m++) {
            const auto pExpr1 = _pExpr->as<StructConstructor>()->get(m)->getValue();
            ExpressionPtr pExpr2;

            if(pExpr1->getKind() == Expression::TYPE &&
                pExpr1->as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                const auto pRange = pExpr1->as<TypeExpr>()->getContents()->as<Range>();

                pExpr2 = std::make_shared<Binary>(Binary::BOOL_AND,
                    std::make_shared<Binary>(Binary::LESS_OR_EQUALS, pRange->getMin(), _vars.get(m)),
                    std::make_shared<Binary>(Binary::LESS_OR_EQUALS, _vars.get(m), pRange->getMax()));
            } else {
                pExpr2 = std::make_shared<Binary>(Binary::EQUALS, _vars.get(m), pExpr1);
            }

            if (m == 0)
                pExpr = pExpr2;
            else
                pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExpr, pExpr2);
        }
    }
    return pExpr;
}

TypePtr CollectPreConditions::getNotNamedReferenceType(const TypePtr& _pType) {
    auto result = _pType;
    while (result && result->getKind() == Type::NAMED_REFERENCE) {
        result = result->as<NamedReferenceType>()->getDeclaration()->getType();
    }
    return result;
}

///Executing function
ModulePtr ir::processPreConditions(const ModulePtr &_module) {
    const auto pNewModule = std::make_shared<Module>();
    CollectPreConditions collector(pNewModule);
    collector.traverseNode(_module);
    return pNewModule;
}

void ir::getRanges(const ArrayType &_array, Collection<Range> &_ranges) {
    Collection<Type> dims;
    _array.getDimensions(dims);

    for(TypePtr pType : dims) {
        if (!pType) {
            _ranges.add(NULL);
            continue;
        }

        switch (pType->getKind()) {
            case Type::SUBTYPE:
                pType = pType->as<Subtype>()->asRange();
                // no break;
            case Type::RANGE:
                _ranges.add(pType->as<Range>());
                break;
            default:
                _ranges.add(NULL);
        }
    }
}

class Semantics: public Visitor {
public:
    Semantics() : m_pPrecondition(std::make_shared<Conjunction>()) {}

    void nonZero(const ExpressionPtr& _pExpr);
    ConjunctionPtr isElement(const SubtypePtr& _pSubtype, const ExpressionPtr& _pExpr);
    ConjunctionPtr isElement(const Collection<Type>& _dims, const ExpressionPtr& _pIndex);
    ConjunctionPtr notIntersect(const SubtypePtr& _pSub1, const SubtypePtr& _pSub2);
    ConjunctionPtr checkIntersect(const ExpressionPtr& _pExpr1, const ExpressionPtr& _pExpr2);
    void arrayUnion(const ArrayTypePtr& _pArr1, const ArrayTypePtr& _pArr2);
    void arrayConstructor(const ArrayTypePtr& _type, const ArrayConstructorPtr& _constructor);
    void subtract(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight);
    void add(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight);

    bool visitBinary(const BinaryPtr &_bin) override;
    bool visitReplacement(const ReplacementPtr &_rep) override;
    bool visitArrayIteration(const ArrayIterationPtr &_ai) override;
    bool visitArrayPartExpr(const ArrayPartExprPtr& _ap) override;

    ConjunctionPtr getPrecondition(const ExpressionPtr& _pExpr) {
        traverseExpression(_pExpr);
        return m_pPrecondition;
    }

    template <class T>
    std::shared_ptr<T> clone(const std::shared_ptr<T>& _ptr) { return m_cloner.get(_ptr); }

private:
    ConjunctionPtr m_pPrecondition;
    Cloner m_cloner;
};

void Semantics::nonZero(const ExpressionPtr& _pExpr) {
    if (!_pExpr)
        return;
    m_pPrecondition->addExpression(std::make_shared<Binary>(Binary::NOT_EQUALS, _pExpr, std::make_shared<Literal>()));
}

ConjunctionPtr Semantics::isElement(const SubtypePtr& _pSubtype, const ExpressionPtr& _pExpr) {
    if (!_pSubtype || !_pExpr)
        return nullptr;

    return Conjunction::getConjunction(Expression::substitute(clone(_pSubtype->getExpression()),
        std::make_shared<VariableReference>(_pSubtype->getParam()), _pExpr)->as<Expression>());
}

ConjunctionPtr Semantics::isElement(const Collection<Type>& _dims, const ExpressionPtr& _pIndex) {
    if (_dims.size() == 1) {
        return _dims.get(0)->getKind() == Type::SUBTYPE
            ? isElement(_dims.get(0)->as<Subtype>(), _pIndex)
            : nullptr;
    }

    if (_pIndex->getKind() != Expression::CONSTRUCTOR
        || _pIndex->as<Constructor>()->getKind() != Constructor::STRUCT_FIELDS)
        return nullptr;

    const auto cons = _pIndex->as<StructConstructor>();
    const auto pConj = std::make_shared<Conjunction>();

    for (size_t i = 0; i < _dims.size(); ++i)
        if (_dims.get(i)->getKind() == Type::SUBTYPE)
            pConj->append(isElement(_dims.get(i)->as<Subtype>(), cons->get(i)->getValue()));

    return pConj;
}

ConjunctionPtr Semantics::notIntersect(const SubtypePtr& _pSub1, const SubtypePtr& _pSub2) {
    const auto pVar = std::make_shared<VariableReference>(_pSub1->getParam());

    const auto pConj = isElement(_pSub1, pVar);

    pConj->append(isElement(_pSub2, pVar));
    pConj->negate();

    return pConj;
}

ConjunctionPtr Semantics::checkIntersect(const ExpressionPtr& _pExpr1, const ExpressionPtr& _pExpr2) {
    if (!_pExpr1 || !_pExpr2)
        return nullptr;

    if (_pExpr1->getKind() != Expression::TYPE && _pExpr2->getKind() != Expression::TYPE) {
        return Conjunction::getConjunction(std::make_shared<Binary>(Binary::NOT_EQUALS, _pExpr1, _pExpr2));
    }

    if (_pExpr1->getKind() == Expression::TYPE && _pExpr2->getKind() == Expression::TYPE) {
        assert(_pExpr1->as<TypeExpr>()->getContents()->getKind() == Type::SUBTYPE);
        assert(_pExpr2->as<TypeExpr>()->getContents()->getKind() == Type::SUBTYPE);

        return notIntersect(_pExpr1->as<TypeExpr>()->getContents()->as<Subtype>(),
            _pExpr2->as<TypeExpr>()->getContents()->as<Subtype>());
    }

    const auto& pExpr = _pExpr1->getKind() != Expression::TYPE ? _pExpr1 : _pExpr2;
    const auto& pType = _pExpr1->getKind() == Expression::TYPE
        ? _pExpr1->as<TypeExpr>()->getContents() : _pExpr2->as<TypeExpr>()->getContents();

    assert(pType->getKind() == Type::SUBTYPE);

    ConjunctionPtr pResult = isElement(pType->as<Subtype>(), pExpr);
    if (!pResult)
        return nullptr;

    pResult->negate();
    return pResult;
}

void Semantics::arrayUnion(const ArrayTypePtr& _pArr1, const ArrayTypePtr& _pArr2) {
    Collection<Type> dim1, dim2;
    _pArr1->getDimensions(dim1);
    _pArr2->getDimensions(dim2);

    if (dim1.size() != dim2.size())
        return;

    TypePtr dimLeft = NULL, dimRight = NULL;
    for (auto i = dim1.begin(), j = dim2.begin();
        i != dim1.end(); ++i, ++j) {
        if (**i != **j) {
            if (dimLeft || dimRight)
                return;
            dimLeft = *i;
            dimRight = *j;
        }
    }

    if (dimLeft->getKind() != Type::SUBTYPE || dimRight->getKind() != Type::SUBTYPE)
        return;

    ConjunctionPtr pConj = notIntersect(dimLeft->as<Subtype>(), dimRight->as<Subtype>());
    if (!pConj)
        return;

    m_pPrecondition->addExpression(na::generalize(pConj->mergeToExpression()));
}

void Semantics::arrayConstructor(const ArrayTypePtr& _type, const ArrayConstructorPtr& _constructor) {
    if (_type->getDimensionType()->getKind() != Type::SUBTYPE)
        return;

    //const Subtype& dim = *_type.getDimensionType().as<Subtype>();
    Collection<Type> dims;
    _type->getDimensions(dims);


    for (size_t i = 0; i < _constructor->size(); ++i) {
        const auto pDef = _constructor->get(i);

        m_pPrecondition->append(isElement(dims, pDef->getIndex()));

        for (size_t j = i + 1; j < _constructor->size(); ++j)
            m_pPrecondition->addExpression(std::make_shared<Binary>(Binary::NOT_EQUALS,
                pDef->getIndex(), _constructor->get(j)->getIndex()));
    }
}

void Semantics::subtract(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight) {
    if (!_pLeft || !_pLeft->getType() || !_pRight)
        return;
    switch (_pLeft->getType()->getKind()) {
        case Type::NAT:
            m_pPrecondition->addExpression(std::make_shared<Binary>(Binary::GREATER_OR_EQUALS, _pLeft, _pRight));
            break;
        case Type::SUBTYPE: {
            m_pPrecondition->append(isElement(_pLeft->getType()->as<Subtype>(),
                std::make_shared<Binary>(Binary::SUBTRACT, _pLeft, _pRight)));
            break;
        }
    }
}

void Semantics::add(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight) {
    if (!_pLeft || !_pRight || !_pLeft->getType() || !_pRight->getType())
        return;
    switch (_pLeft->getType()->getKind()) {
        case Type::ARRAY: {
            assert(_pRight->getType()->getKind() == Type::ARRAY);
            arrayUnion(_pLeft->getType()->as<ArrayType>(), _pRight->getType()->as<ArrayType>());
            break;
        }
    }
}

bool Semantics::visitBinary(const BinaryPtr &_bin) {
    switch (_bin->getOperator()) {
        case Binary::DIVIDE:
            nonZero(_bin->getRightSide());
            break;
        case Binary::SUBTRACT:
            subtract(_bin->getLeftSide(), _bin->getRightSide());
            break;
        case Binary::ADD:
            add(_bin->getLeftSide(), _bin->getRightSide());
            break;
    }
    return true;
}

bool Semantics::visitReplacement(const ReplacementPtr &_rep) {
    if (!_rep->getObject() || !_rep->getObject()->getType())
        return true;

    switch (_rep->getObject()->getType()->getKind()) {
        case Type::ARRAY:
            arrayConstructor(_rep->getObject()->getType()->as<ArrayType>(), _rep->getNewValues()->as<ArrayConstructor>());
            break;
    }

    return true;
}

bool Semantics::visitArrayIteration(const ArrayIterationPtr &_ai) {
    for (size_t i = 0; i < _ai->size(); ++i)
        for (size_t j = i + 1; j < _ai->size(); ++j) {
            const auto
                pDef1 = _ai->get(i),
                pDef2 = _ai->get(j);

            for (size_t n = 0; n < pDef1->getConditions().size(); ++n)
                for (size_t m = 0; m < pDef2->getConditions().size(); ++m) {
                    const ExpressionPtr
                        &pExpr1 = pDef1->getConditions().get(n),
                        &pExpr2 = pDef2->getConditions().get(m);

                    if (pExpr1->getKind() == Expression::CONSTRUCTOR
                        && pExpr1->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {
                        const StructConstructorPtr
                            tuple1 = pExpr1->as<StructConstructor>(),
                            tuple2 = pExpr2->as<StructConstructor>();

                        assert(tuple1->size() == tuple2->size());

                        Conjunction conj;
                        for (size_t k = 0; k < tuple1->size(); ++k)
                            conj.disjunct(checkIntersect(tuple1->get(k)->getValue(), tuple2->get(k)->getValue()));

                        m_pPrecondition->append(conj);

                        continue;
                    }

                    if (ConjunctionPtr pConj = checkIntersect(pExpr1, pExpr2))
                        m_pPrecondition->append(*pConj);
                }
        }
    return true;
}
bool Semantics::visitArrayPartExpr(const ArrayPartExprPtr& _ap) {
    if (!_ap->getObject() || !_ap->getObject()->getType())
        return true;

    assert(_ap->getObject()->getType()->getKind() == Type::ARRAY);
    const auto array = _ap->getObject()->getType()->as<ArrayType>();

    Collection<Type> dims;
    array->getDimensions(dims);

    assert(dims.size() == _ap->getIndices().size());

    size_t j = 0;
    for (auto i = dims.begin(); i != dims.end(); ++i, ++j)
        if ((*i)->getKind() == Type::SUBTYPE)
            m_pPrecondition->append(isElement((*i)->as<Subtype>(), _ap->getIndices().get(j)));

    return true;
}

vf::ConjunctionPtr getPreConditionForExpression(const ExpressionPtr& _pExpr) {
    return Semantics().getPrecondition(_pExpr);
}

vf::ConjunctionPtr getPreConditionForStatement(const StatementPtr& _pStmt, const PredicatePtr& _pPred, const vf::ContextPtr& _pContext) {
    ConjunctionPtr pPre = std::make_shared<Conjunction>();

    if (!_pStmt)
        return pPre;

    switch (_pStmt->getKind()) {
        case Statement::ASSIGNMENT:
            pPre->assign(getPreConditionForExpression(_pStmt->as<Assignment>()->getExpression()));
            break;

        case Statement::CALL: {
            const auto call = _pStmt->as<Call>();

            for (size_t i = 0; i < call->getArgs().size(); ++i)
                pPre->append(getPreConditionForExpression(call->getArgs().get(i)));

            if (!_pContext)
                break;

            pPre->addExpression(tr::makeCall(_pContext->getPrecondition(call), call));

            const PredicatePtr pPred = !_pPred ?
                (_pContext ? _pContext->m_pPredicate : PredicatePtr(NULL)) : _pPred;

            if (na::isRecursiveCall(call, pPred))
                if (const auto pMeasure =_pContext->getMeasure(call))
                    pPre->addExpression(std::make_shared<Binary>(Binary::LESS, tr::makeCall(pMeasure, call), tr::makeCall(pMeasure, pPred)));
            break;
        }

//TODO:dyp: check later
#ifdef CONDITIONS_FOR_IF
        case Statement::IF: {
            const If& iff = *_pStmt->as<If>();
            pPre->assign(getPreConditionForExpression(iff.getArg()));

            ConjunctionPtr
                pBody = getPreConditionForStatement(iff.getBody(), _pPred, _pContext),
                pElse = getPreConditionForStatement(iff.getElse(), _pPred, _pContext);

            for (std::set<ConjunctPtr>::iterator i = pBody->getConjuncts().begin();
                i != pBody->getConjuncts().end(); ++i)
                pPre->addExpression(std::make_shared<Binary>(Binary::IMPLIES, iff.getArg(), (*i)->mergeToExpression()));

            for (std::set<ConjunctPtr>::iterator i = pElse->getConjuncts().begin();
                i != pElse->getConjuncts().end(); ++i)
                pPre->addExpression(std::make_shared<Binary>(Binary::IMPLIES, std::make_shared<Unary>(Unary::BOOL_NEGATE, iff.getArg()),
                    (*i)->mergeToExpression()));

            break;
        }
#endif

        case Statement::BLOCK: {
            pPre->assign(getPreConditionForStatement(_pStmt->as<Block>()->get(0), _pPred, _pContext));

            const ConjunctionPtr
                pFirstPost = getPostConditionForStatement(_pStmt->as<Block>()->get(0), _pContext),
                pSecondPre = getPreConditionForStatement(_pStmt->as<Block>()->get(1), _pPred, _pContext);

            if (!pSecondPre || pSecondPre->empty())
                break;

            pPre->append(Conjunction::implies(pFirstPost, pSecondPre));
            break;
        }

        case Statement::PARALLEL_BLOCK:
            pPre->assign(getPreConditionForStatement(_pStmt->as<Block>()->get(0), _pPred, _pContext));
            pPre->append(getPreConditionForStatement(_pStmt->as<Block>()->get(1), _pPred, _pContext));
            break;
    }

    return pPre;
}

vf::ConjunctionPtr getPostConditionForStatement(const StatementPtr& _pStmt, const vf::ContextPtr& _pContext) {
    ConjunctionPtr pPost = std::make_shared<Conjunction>();

    if (!_pStmt)
        return pPost;

    switch (_pStmt->getKind()) {
        case Statement::ASSIGNMENT:
            pPost->addExpression(std::make_shared<Binary>(Binary::EQUALS, _pStmt->as<Assignment>()->getLValue(),
                _pStmt->as<Assignment>()->getExpression()));
            break;

        case Statement::CALL: {
            if (!_pContext)
                break;

            const auto call = _pStmt->as<Call>();
            pPost->addExpression(tr::makeCall(_pContext->getPostcondition(call), call));

            const auto pType = call->getPredicate()->getType()->as<PredicateType>();
            if (!pType || pType->getOutParams().size() <= 1)
                break;

            for (size_t i = 0; i < pType->getOutParams().size(); ++i)
                pPost->addExpression(std::make_shared<Binary>(Binary::IMPLIES,
                    tr::makeCall(_pContext->getPrecondition(call, i + 1), call),
                    tr::makeCall(_pContext->getPostcondition(call, i + 1), call)));
            break;
        }

//TODO:dyp: check later
#ifdef CONDITIONS_FOR_IF
        case Statement::IF: {
            const IfPtr pIf = _pStmt->as<If>();

            ConjunctionPtr
                pBody = getPostConditionForStatement(pIf->getBody(), _pContext),
                pElse = getPostConditionForStatement(pIf->getElse(), _pContext);

            for (std::set<ConjunctPtr>::iterator i = pBody->getConjuncts().begin();
                i != pBody->getConjuncts().end(); ++i)
                pPost->addExpression(std::make_shared<Binary>(Binary::IMPLIES, pIf->getArg(), (*i)->mergeToExpression()));

            for (std::set<ConjunctPtr>::iterator i = pElse->getConjuncts().begin();
                i != pElse->getConjuncts().end(); ++i)
                pPost->addExpression(std::make_shared<Binary>(Binary::IMPLIES, std::make_shared<Unary>(Unary::BOOL_NEGATE, pIf->getArg()),
                    (*i)->mergeToExpression()));

            break;
        }
#endif

        case Statement::PARALLEL_BLOCK:
            pPost->assign(getPostConditionForStatement(_pStmt->as<Block>()->get(0), _pContext));
            pPost->append(getPostConditionForStatement(_pStmt->as<Block>()->get(1), _pContext));
            break;

        case Statement::BLOCK:
            pPost->assign(getPostConditionForStatement(_pStmt->as<Block>()->get(0), _pContext));
            pPost->assign(getPostConditionForStatement(_pStmt->as<Block>()->get(1), _pContext));
            break;
    }

    return pPost;
}
