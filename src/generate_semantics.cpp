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
#include "verification.h"

using namespace ir;

ExpressionPtr CollectPreConditions::collectConditions() {
    ExpressionPtr pExpr;
    for (std::list<Loc>::reverse_iterator i = m_path.rbegin(); i != m_path.rend(); ++i) {
        switch (i->role) {

            case R_PredicateDecl: {
                FormulaPtr pFormula = ((Predicate *)(i->pNode))->getPreCondition();

                if(pFormula) {
                    if(!pExpr)
                        pExpr = pFormula->getSubformula();
                    else
                        pExpr = new Binary(Binary::BOOL_AND, pFormula->getSubformula(), pExpr);
                }
                break;
            }

            case R_IfBody: {
                assert(::next(i)->type == N_If);
                ExpressionPtr pExpr1 = ((If *)(::next(i)->pNode))->getArg();

                if(!pExpr)
                    pExpr = ExpressionPtr(pExpr1);
                else
                    pExpr = new Binary(Binary::BOOL_AND, pExpr1, pExpr);

                break;
            }

            case R_IfElse: {
                assert(::next(i)->type == N_If);
                ExpressionPtr pExpr1 = ((If *)(::next(i)->pNode))->getArg();

                if((pExpr1->getKind() == Expression::UNARY) && (pExpr1.as<Unary>()->getOperator() == Unary::BOOL_NEGATE))
                    pExpr1 = pExpr1.as<Unary>()->getExpression();
                else
                    pExpr1 = new Unary(Unary::BOOL_NEGATE, pExpr1);


                if(!pExpr)
                    pExpr = ExpressionPtr(pExpr1);
                else
                    pExpr = new Binary(Binary::BOOL_AND, pExpr1, pExpr);

                break;
            }

            case R_SwitchDefault: {
                SwitchPtr pSwitch = (Switch *)(::next(i)->pNode);
                ExpressionPtr pExpr1;

                for(size_t j = 0; j < pSwitch->size(); ++j) {

                    for(size_t k = 0; k < pSwitch->get(j)->getExpressions().size(); ++k) {

                        ExpressionPtr pExprCurrent = pSwitch->get(j)->getExpressions().get(k);

                        if(pExprCurrent->getKind() != Expression::TYPE) {
                            pExpr1 = new Binary(Binary::NOT_EQUALS, pSwitch->getArg(), pExprCurrent);
                        }
                        else {
                            if(pExprCurrent.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                                RangePtr range = pExprCurrent.as<TypeExpr>()->getContents().as<Range>();

                                pExpr1 = new Binary(Binary::BOOL_OR,
                                    new Binary(Binary::LESS, pSwitch->getArg(), range->getMin()),
                                    new Binary(Binary::GREATER,  pSwitch->getArg(), range->getMax()));
                            }
                        }

                        if(!pExpr)
                            pExpr = ExpressionPtr(pExpr1);
                        else
                            pExpr = new Binary(Binary::BOOL_AND, pExpr1, pExpr);
                    }
                }

                break;
            }

            case R_SwitchCase: {
                ExpressionPtr pExpr1;
                SwitchCase *pCase = (SwitchCase *)i->pNode;
                Switch *pSwitch = (Switch *)::next(i)->pNode;

                //case getExpressions more than 1, combining them in lemma with BOOL_OR
                for(size_t j = 0; j < pCase->getExpressions().size(); ++j) {
                    ExpressionPtr pExpr2;
                    ExpressionPtr pCurrent = pCase->getExpressions().get(j);

                    if(pCurrent->getKind() != Expression::TYPE) {
                        pExpr2 = new Binary(Binary::EQUALS, pSwitch->getArg(), pCurrent);
                    }
                    else {
                        if(pCurrent.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {
                            RangePtr range = pCurrent.as<TypeExpr>()->getContents().as<Range>();

                            pExpr2 = new Binary(Binary::BOOL_AND,
                                new Binary(Binary::GREATER_OR_EQUALS, pSwitch->getArg(), range->getMin()),
                                new Binary(Binary::LESS_OR_EQUALS,  pSwitch->getArg(), range->getMax()));
                        }
                    }

                    if(j == 0)
                        pExpr1 = pExpr2;
                    else
                        pExpr1 = new Binary(Binary::BOOL_OR, pExpr1, pExpr2);
                }

                if(!pExpr)
                    pExpr = pExpr1;
                else
                    pExpr = new Binary(Binary::BOOL_AND, pExpr1, pExpr);

                break;
            }

            case R_ArrayIterationPart: {
                Collection<Expression> conditions = ((ArrayPartDefinition *)(i->pNode))->getConditions();
                Collection<NamedValue> iterators = ((ArrayIteration *)(::next(i)->pNode))->getIterators();
                Collection<VariableReference> vars;
                ExpressionPtr pExpr1;

                for(int j = 0; j < iterators.size(); j++)
                    vars.add(new VariableReference(iterators.get(j)));

                for(int i = 0; i < conditions.size(); i++) {
                    ExpressionPtr pExpr2;
                    ExpressionPtr pCurrent = conditions.get(i);

                    if(iterators.size() == 1)
                        pExpr2 = varBelongsSetOneDimension(vars.get(0), pCurrent);
                    else
                        pExpr2 = varsBelongSetSeveralDimensions(vars, pCurrent);

                    if(i == 0)
                        pExpr1 = pExpr2;
                    else
                        pExpr1 = new Binary(Binary::BOOL_OR, pExpr1, pExpr2);
                }

                if(!pExpr)
                    pExpr = pExpr1;
                else
                    pExpr = new Binary(Binary::BOOL_AND, pExpr1, pExpr);

                break;
            }
            case R_ArrayIterationDefault: {
                ArrayIteration * pArrayIt = ((ArrayIteration *)(::next(i)->pNode));
                NamedValuePtr iterator = pArrayIt->getIterators().get(0);
                VariableReferencePtr pVar = new VariableReference(iterator);
                Collection<NamedValue> iterators = ((ArrayIteration *)(::next(i)->pNode))->getIterators();
                Collection<VariableReference> vars;
                ExpressionPtr pExpr1;

                for(int j = 0; j < iterators.size(); j++)
                    vars.add(new VariableReference(iterators.get(j)));

                for(int i = 0; i < pArrayIt->size(); i++) {
                    for(int j = 0; j < pArrayIt->get(i)->getConditions().size(); j++) {

                        ExpressionPtr pCurrent = pArrayIt->get(i)->getConditions().get(j);

                        if(iterators.size() == 1)
                            pExpr1 = new Unary(Unary::BOOL_NEGATE, varBelongsSetOneDimension(vars.get(0), pCurrent));
                        else
                            pExpr1 = new Unary(Unary::BOOL_NEGATE, varsBelongSetSeveralDimensions(vars, pCurrent));

                        if(!pExpr)
                            pExpr = ExpressionPtr(pExpr1);
                        else
                            pExpr = new Binary(Binary::BOOL_AND, pExpr1, pExpr);
                    }
                }
                break;
            }
        }
    }

    return pExpr;
}

///Creating modules for theory

int CollectPreConditions::handlePredicateDecl(Node &_node) {
    m_pPredicate = &(Predicate &)_node;
    m_pNewModule = new Module(m_nameGen.makeNameSubmoduleForPredicate(*m_pPredicate));
    m_module.getModules().add(m_pNewModule);
    return 0;
}

int CollectPreConditions::handleProcessDecl(Node &_node) {
    m_pProcess = &(Process &)_node;
    m_pNewModule = new Module(m_nameGen.makeNameSubmoduleForProcess(*m_pProcess));
    m_module.getModules().add(m_pNewModule);
    return 0;
}

bool CollectPreConditions::visitPredicateType(PredicateType &_node) {
    m_pNewModule = new Module(m_nameGen.makeNameSubmoduleForType());
    m_module.getModules().add(m_pNewModule);
    return true;
}

bool CollectPreConditions::visitLambda(Lambda &_node) {
 /*   Lambda &_lambda = (Lambda &)_node;
    m_pPredicate = new Predicate((AnonymousPredicate &)_lambda.getPredicate());
    m_pPredicate->setName(m_nameGen.makeNameLambdaToPredicate());

    m_pNewModule = new Module(m_nameGen.makeNameSubmoduleForLambda());
    m_module.getModules().add(m_pNewModule);*/
    return true;
}


///Adding preconditions to module

int CollectPreConditions::handlePredicatePreCondition(Node &_node) {

    m_pNewModule->getFormulas().add(extractFormulaFromPredicate(
        m_nameGen.makeNamePredicatePrecondition(*m_pPredicate),
        *m_pPredicate, (Formula &)_node));

    return 0;
}

int CollectPreConditions::handlePredicateBranchPreCondition(Node &_node) {

    m_pNewModule->getFormulas().add(extractFormulaFromPredicate(
        m_nameGen.makeNamePredicateBranchPrecondition(*m_pPredicate, getLoc().cPosInCollection),
        *m_pPredicate, (Formula &)_node));

    return 0;
}

int CollectPreConditions::handlePredicateTypePreCondition(Node &_node) {

    m_pNewModule->getFormulas().add(extractFormulaFromPredicate(
        m_nameGen.makeNameTypePreCondition(),
        *m_pPredicate, (Formula &)_node));

    return 0;
}

int CollectPreConditions::handlePredicateTypeBranchPreCondition(Node &_node) {

    m_pNewModule->getFormulas().add(extractFormulaFromPredicate(
        m_nameGen.makeNameTypeBranchPreCondition(getLoc().cPosInCollection),
        *m_pPredicate, (Formula &)_node));

    return 0;
}

int CollectPreConditions::handleProcessBranchPreCondition(Node &_node) {

    m_pNewModule->getFormulas().add(extractFormulaFromPredicate(
        m_nameGen.makeNameProcessBranchPreCondition(*m_pProcess, getLoc().cPosInCollection),
        *m_pPredicate, (Formula &)_node));

    return 0;
}


///Generating lemmas

int CollectPreConditions::handleFunctionCallee(Node &_node) {
    m_pNewModule->getLemmas().add(new LemmaDeclaration(collectConditions(),
                                    new Label(m_nameGen.makeNameLemmaCall())));
    return 0;
}

int CollectPreConditions::handlePredicateCallBranchResults(Node &_node) {
//    ((PredicateType &)((Call &)::prev(m_path.end())).getPredicate()->getType()).getOutParams().get(m_path.end()->cPosInCollection)->get(0);
//    ((Expression &)_node).getType();
    return 0;
}

int CollectPreConditions::handlePredicateCallArgs(Node &_node) {
    return 0;
}

bool CollectPreConditions::visitCall(Call &_node) {

    ExpressionPtr pCond = collectConditions();

    if(_node.getPredicate()->getKind() == Expression::PREDICATE) {

//compatibility of arguments
        for(int i = 0; i < _node.getArgs().size(); i++) {

            ExpressionPtr pCallArg = _node.getArgs().get(i);
            ParamPtr pPredParam = _node.getPredicate().as<PredicateReference>()->getTarget().as<Predicate>()->getInParams().get(i);

            ExpressionPtr pExpr;
            TypePtr pTypeCall = pCallArg->getType();
            TypePtr pTypePred = pPredParam->getType();

            pTypeCall = getNotNamedReferenceType(pTypeCall);
            pTypePred = getNotNamedReferenceType(pTypePred);

//for arrays
            if(pCallArg->getKind() == Expression::VAR) {

                if(pTypeCall && pTypeCall->getKind() == Type::PARAMETERIZED &&
                   pTypeCall.as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY &&
                   pTypePred && pTypePred->getKind() == Type::PARAMETERIZED &&
                   pTypePred.as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY) {

                    Collection<Range> rangesCall = arrayRangesWithCurrentParams(pCallArg);
                    ExpressionPtr pExpr;

                    Collection<Range> rangesPred, ranges;

//в разных случаях доступ к аргументам может быть и не таким
                    NamedValues params = pTypePred.as<ParameterizedType>()->getParams();
                    ArrayTypePtr pArray = pTypePred.as<ParameterizedType>()->getActualType()
                        .as<DerivedType>().as<ArrayType>();
                    if (pArray)
                        getRanges(*pArray, ranges);

                    Collection<Expression> args = pPredParam->getType().as<NamedReferenceType>()->getArgs();

                    for(int j = 0; j < ranges.size(); j++) {
                        RangePtr pNewRange = new Range(ranges.get(j)->getMin(), ranges.get(j)->getMax());

                        for(int l = 0; l < params.size(); l++) {
                            pNewRange = Expression::substitute(pNewRange, new VariableReference(params.get(l)), args.get(l)).as<Range>();
                        }

                        rangesPred.add(pNewRange);
                    }

                    if(rangesCall.size() == rangesPred.size()) {
                        for(int l = 0; l < rangesCall.size(); l++) {
                            if(l == 0)
                                pExpr = new Binary(Binary::BOOL_AND,
                                    new Binary(Binary::EQUALS, rangesCall.get(0)->getMin(), rangesPred.get(0)->getMin()),
                                    new Binary(Binary::EQUALS, rangesCall.get(0)->getMax(), rangesPred.get(0)->getMax()));
                            else
                                pExpr = new Binary(Binary::BOOL_AND, pExpr, new Binary(Binary::BOOL_AND,
                                    new Binary(Binary::EQUALS, rangesCall.get(l)->getMin(), rangesPred.get(l)->getMin()),
                                    new Binary(Binary::EQUALS, rangesCall.get(l)->getMax(), rangesPred.get(l)->getMax())));
                        }

                        if(pCond)
                            m_pNewModule->getLemmas().add(new LemmaDeclaration(
                                new Binary(Binary::IMPLIES, pCond, pExpr),
                                new Label(m_nameGen.makeNameLemmaCall())));
                        else
                            m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                                new Label(m_nameGen.makeNameLemmaCall())));
                    }
                    ///error lemma
                    else {
                        LiteralPtr pLiteral = new Literal(Number("0" , Number::INTEGER));

                        m_pNewModule->getLemmas().add(new LemmaDeclaration(
                            new Binary(Binary::NOT_EQUALS, pLiteral, pLiteral),
                            new Label(m_nameGen.makeNameLemmaCall())));
                    }
                }
            }

//for subtypes
            if(pTypeCall && pTypeCall->getKind() == Type::SUBTYPE && pTypePred && pTypePred->getKind() == Type::SUBTYPE) {

                ExpressionPtr pExprCall = pTypeCall.as<Subtype>()->getExpression();
                VarSubstitute substitute(pTypeCall.as<Subtype>()->getParam(), pCallArg);
                substitute.traverseNode(*(pExprCall.ptr()));

                ExpressionPtr pExprPred = pTypePred.as<Subtype>()->getExpression();
                substitute = VarSubstitute(pTypePred.as<Subtype>()->getParam(), pCallArg);
                substitute.traverseNode(*(pExprPred.ptr()));

                if(pCond)
                    pExpr = new Binary(Binary::BOOL_AND, pCond, pExprPred);
                else
                    pExpr = pExprPred;

                pExpr = new Binary(Binary::IFF, pExpr, pExprCall);

                m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                    new Label(m_nameGen.makeNameLemmaCall())));
            }

//for predicate
//в леммы подставляются Formula а не Expression
//посмотреть, что еще тут может быть
            if(((pCallArg->getKind() == Expression::TYPE && 
                pCallArg.as<TypeExpr>()->getContents()->getKind() == Type::PREDICATE) ||
                (pCallArg->getKind() == Expression::LAMBDA)) &&
                pPredParam->getType()->getKind() == Type::PREDICATE) {

                FormulaPtr pPre, pPost;

                if(pCallArg->getKind() == Expression::LAMBDA) {
                    pPre = pCallArg.as<Lambda>()->getPredicate().getPreCondition();
                    pPost = pCallArg.as<Lambda>()->getPredicate().getPostCondition();
                }

                if(pCallArg->getKind() == Expression::TYPE && 
                    pCallArg.as<TypeExpr>()->getContents()->getKind() == Type::PREDICATE) {
                    pPre = pCallArg.as<TypeExpr>()->getContents().as<PredicateType>()->getPreCondition();
                    pPost = pCallArg.as<TypeExpr>()->getContents().as<PredicateType>()->getPostCondition();
                }

                PredicateTypePtr pArgPred = pPredParam->getType().as<PredicateType>();
                ExpressionPtr pExpr;

                if(pArgPred->getPreCondition()) {

                    pExpr = pArgPred->getPreCondition();

                    if(pPre)
                        pExpr = new Binary(Binary::IMPLIES, pExpr, pPre);
                }

                if(pExpr) {
                    if(pCond)
                        m_pNewModule->getLemmas().add(new LemmaDeclaration(
                            new Binary(Binary::IMPLIES, pCond, pExpr),
                            new Label(m_nameGen.makeNameLemmaCall())));
                    else
                        m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                            new Label(m_nameGen.makeNameLemmaCall())));
                }

                if(pPost) {

                    pExpr = pPost->getSubformula();

                    if(pArgPred->getPostCondition())
                        pExpr = new Binary(Binary::IMPLIES, pExpr, pArgPred->getPostCondition());
                }

                if(pExpr) {
                    if(pCond)
                        m_pNewModule->getLemmas().add(new LemmaDeclaration(
                            new Binary(Binary::IMPLIES, pCond, pExpr),
                            new Label(m_nameGen.makeNameLemmaCall())));
                    else
                        m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                            new Label(m_nameGen.makeNameLemmaCall())));
                }
            }
        }

//compatibility of results
        for(int i = 0; i < _node.getBranches().size(); i++) {
            for(int j = 0; j < _node.getBranches().get(i)->size(); j++) {

                ExpressionPtr pCallArg = _node.getBranches().get(i)->get(j);   //kind = VAR
                ParamPtr pPredParam = _node.getPredicate().as<PredicateReference>()->getTarget()
                    .as<Predicate>()->getOutParams().get(i)->get(j);   //kind = Param

                ExpressionPtr pExpr;
                TypePtr pTypeCall = pCallArg->getType();
                TypePtr pTypePred = pPredParam->getType();

                pTypeCall = getNotNamedReferenceType(pTypeCall);
                pTypePred = getNotNamedReferenceType(pTypePred);

//for arrays
                if(pCallArg->getKind() == Expression::VAR) {

                    if(pTypeCall && pTypeCall->getKind() == Type::PARAMETERIZED &&
                       pTypeCall.as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY &&
                       pTypePred && pTypePred->getKind() == Type::PARAMETERIZED &&
                       pTypePred.as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY) {

                        Collection<Range> rangesCall = arrayRangesWithCurrentParams(pCallArg);
                        ExpressionPtr pExpr;

                        Collection<Range> rangesPred, ranges;

//в разных случаях доступ к аргументам может быть и не таким
                        NamedValues params = pTypePred.as<ParameterizedType>()->getParams();

                        ArrayTypePtr pArray = pTypePred.as<ParameterizedType>()->getActualType()
                            .as<DerivedType>().as<ArrayType>();
                        if (pArray)
                            getRanges(*pArray, ranges);

                        Collection<Expression> args = pPredParam->getType().as<NamedReferenceType>()->getArgs();

                        for(int k = 0; k < ranges.size(); k++) {
                            RangePtr pNewRange = new Range(ranges.get(k)->getMin(), ranges.get(k)->getMax());

                            for(int l = 0; l < params.size(); l++) {
                                pNewRange = Expression::substitute(pNewRange, new VariableReference(params.get(l)), args.get(l)).as<Range>();
                            }

                            rangesPred.add(pNewRange);
                        }

                        if(rangesCall.size() == rangesPred.size()) {
                            for(int l = 0; l < rangesCall.size(); l++) {
                                if(l == 0)
                                    pExpr = new Binary(Binary::BOOL_AND,
                                        new Binary(Binary::EQUALS, rangesCall.get(0)->getMin(), rangesPred.get(0)->getMin()),
                                        new Binary(Binary::EQUALS, rangesCall.get(0)->getMax(), rangesPred.get(0)->getMax()));
                                else
                                    pExpr = new Binary(Binary::BOOL_AND, pExpr, new Binary(Binary::BOOL_AND,
                                        new Binary(Binary::EQUALS, rangesCall.get(l)->getMin(), rangesPred.get(l)->getMin()),
                                        new Binary(Binary::EQUALS, rangesCall.get(l)->getMax(), rangesPred.get(l)->getMax())));
                            }

                            if(pCond)
                                m_pNewModule->getLemmas().add(new LemmaDeclaration(
                                    new Binary(Binary::IMPLIES, pCond, pExpr),
                                    new Label(m_nameGen.makeNameLemmaCall())));
                            else
                                m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                                    new Label(m_nameGen.makeNameLemmaCall())));
                        }
                        //error lemma
                        else {
                            LiteralPtr pLiteral = new Literal(Number("0" , Number::INTEGER));

                            m_pNewModule->getLemmas().add(new LemmaDeclaration(
                                new Binary(Binary::NOT_EQUALS, pLiteral, pLiteral),
                                new Label(m_nameGen.makeNameLemmaCall())));
                        }
                    }
                }

//for subtypes
                if(pTypeCall && pTypeCall->getKind() == Type::SUBTYPE && pTypePred && pTypePred->getKind() == Type::SUBTYPE) {

                    ExpressionPtr pExprCall = pTypeCall.as<Subtype>()->getExpression();
                    VarSubstitute substitute(pTypeCall.as<Subtype>()->getParam(), pCallArg);
                    substitute.traverseNode(*(pExprCall.ptr()));

                    ExpressionPtr pExprPred = pTypePred.as<Subtype>()->getExpression();
                    substitute = VarSubstitute(pTypePred.as<Subtype>()->getParam(), pCallArg);
                    substitute.traverseNode(*(pExprPred.ptr()));

                    if(pCond)
                        pExpr = new Binary(Binary::BOOL_AND, pCond, pExprPred);
                    else
                        pExpr = pExprPred;

                    pExpr = new Binary(Binary::IMPLIES, pExpr, pExprCall);

                    m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                        new Label(m_nameGen.makeNameLemmaCall())));
                }
            }
        }
    }

//предикатов ведь не может быть в результатах?********************************************************
    return true;
}

bool CollectPreConditions::visitSwitch(Switch &_node) {

    for(size_t i = 0; i < _node.size(); ++i) {
        for(size_t k = 0; k < _node.get(i)->getExpressions().size(); ++k) {
            for(size_t j = i; j < _node.size(); ++j) {

                size_t l = (i == j) ? k+1: 0;
                for(; l < _node.get(j)->getExpressions().size(); ++l) {

                    ExpressionPtr pExpr1 = _node.get(i)->getExpressions().get(k);
                    ExpressionPtr pExpr2 = _node.get(j)->getExpressions().get(l);

                    caseNonintersection(pExpr1, pExpr2);
                }
            }
        }
    }
    return true;
}

bool CollectPreConditions::visitIf(If &_node) {
    ExpressionPtr pExprIf, pExprElse;

    if((_node.getArg()->getKind() == Expression::UNARY) && (_node.getArg().as<Unary>()->getOperator() == Unary::BOOL_NEGATE))
        pExprElse = _node.getArg().as<Unary>()->getExpression();
    else
        pExprElse = new Unary(Unary::BOOL_NEGATE, _node.getArg());
//использовать Мишин оптимизатор выражений, когда смержиться

    if(ExpressionPtr pCond = collectConditions()) {
        pExprIf = new Binary(Binary::BOOL_AND, pCond, _node.getArg());
        pExprElse = new Binary(Binary::BOOL_AND, pCond, pExprElse);
    }
    else
        pExprIf = _node.getArg();
        //pExprElse already initialized

    NamedValuesPtr params = varCollector(pExprIf);
    FormulaPtr pFormulaIf = new Formula(Formula::EXISTENTIAL, pExprIf);
    FormulaPtr pFormulaElse = new Formula(Formula::EXISTENTIAL, pExprElse);
    pFormulaIf->getBoundVariables().append(*params);
    pFormulaElse->getBoundVariables().append(*params);

    m_pNewModule->getLemmas().add(new LemmaDeclaration(pFormulaIf,
                                                       new Label(m_nameGen.makeNameLemmaIf())));

    m_pNewModule->getLemmas().add(new LemmaDeclaration(pFormulaElse,
                                                       new Label(m_nameGen.makeNameLemmaIf())));
    return true;
}

int CollectPreConditions::handleSwitchDefault(Node &_node) {
    m_pNewModule->getLemmas().add(new LemmaDeclaration(collectConditions(),
                                    new Label(m_nameGen.makeNameLemmaSwitchDefault())));
    return 0;
}

int CollectPreConditions::handleSwitchCase(Node &_node) {
 //   m_pNewModule->getLemmas().add(new LemmaDeclaration(collectConditions(),
   //                                 new Label(m_nameGen.makeNameLemmaSwitchCase())));
    return 0;
}

int CollectPreConditions::handleUnionConsField(Node &_node) {
    m_pNewModule->getLemmas().add(new LemmaDeclaration(collectConditions(),
                                    new Label(m_nameGen.makeNameLemmaUnionConsField())));
    return 0;
}

//x[j] => lemma 1 <= j <= n
bool CollectPreConditions::visitArrayPartExpr(ArrayPartExpr &_node) {

    ExpressionPtr pExpr;
    Collection<Range> pArrayRanges = arrayRangesWithCurrentParams(_node.getObject());

    for(int i = 0; i < _node.getIndices().size(); i++) {

        if(_node.getIndices().get(i)->getKind() != Expression::TYPE) {
            pExpr = new Binary(Binary::BOOL_AND,
                new Binary(Binary::LESS_OR_EQUALS, pArrayRanges.get(i)->getMin(), _node.getIndices().get(i)),
                new Binary(Binary::LESS_OR_EQUALS, _node.getIndices().get(i), pArrayRanges.get(i)->getMax()));
        }

        else if(_node.getIndices().get(i).as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

            RangePtr pRange = _node.getIndices().get(i).as<TypeExpr>()->getContents().as<Range>();

            pExpr = new Binary(Binary::BOOL_AND,
                new Binary(Binary::LESS_OR_EQUALS, pArrayRanges.get(i)->getMin(), pRange->getMin()),
                new Binary(Binary::LESS_OR_EQUALS, pRange->getMax(), pArrayRanges.get(i)->getMax()));
        }

        if(ExpressionPtr pCond = collectConditions())
            m_pNewModule->getLemmas().add(new LemmaDeclaration(
                new Binary(Binary::IMPLIES, pCond, pExpr),
                new Label(m_nameGen.makeNameLemmaArrayPartIndex())));
        else
            m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                new Label(m_nameGen.makeNameLemmaArrayPartIndex())));
    }

    return true;
}

///ar1 = ar [k: ar[m], m: ar[k] ]  =>  lemmas 1 <= k <= n & 1 <= m <= n  &  k != m
bool CollectPreConditions::visitReplacement(Replacement &_node) {

    if(_node.getNewValues()->getConstructorKind() != Constructor::ARRAY_ELEMENTS)
        return true;

    RangePtr pRangeArray = arrayRangeWithCurrentParams(_node.getObject());
    Collection<Range> pRangeArrays = arrayRangesWithCurrentParams(_node.getObject());

    //case 1 dimension
    if(pRangeArrays.size() == 1) {
        for(int i = 0; i < _node.getNewValues().as<ArrayConstructor>()->size(); i++) {

            ExpressionPtr pExpr1 = _node.getNewValues().as<ArrayConstructor>()->get(i).as<ElementDefinition>()->getIndex();

            ExpressionPtr pExpr = new Binary(Binary::BOOL_AND,
                new Binary(Binary::LESS_OR_EQUALS, pRangeArray->getMin(), pExpr1),
                new Binary(Binary::LESS_OR_EQUALS, pExpr1, pRangeArray->getMax()));

            if(ExpressionPtr pCond = collectConditions())
                m_pNewModule->getLemmas().add(new LemmaDeclaration(
                    new Binary(Binary::IMPLIES, pCond, pExpr),
                    new Label(m_nameGen.makeNameLemmaArrayMod())));
            else
                m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                    new Label(m_nameGen.makeNameLemmaArrayMod())));

            for(int j = i+1; j < _node.getNewValues().as<ArrayConstructor>()->size(); j++) {

                ExpressionPtr pExpr2 = _node.getNewValues().as<ArrayConstructor>()->get(j).as<ElementDefinition>()->getIndex();

                m_pNewModule->getLemmas().add(new LemmaDeclaration(
                    new Binary(Binary::NOT_EQUALS, pExpr1, pExpr2),
                    new Label(m_nameGen.makeNameLemmaArrayMod())));
            }
        }
    }
    //case several dimensions
    else {
        for(int i = 0; i < _node.getNewValues().as<ArrayConstructor>()->size(); i++) {

            ExpressionPtr pExpr;
            ExpressionPtr pExpr1 = _node.getNewValues().as<ArrayConstructor>()->get(i).as<ElementDefinition>()->getIndex();

            if(pExpr1->getKind() == Expression::CONSTRUCTOR &&
                pExpr1.as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

                for(int j = 0; j < pExpr1.as<Constructor>().as<StructConstructor>()->size(); j++) {

                    ExpressionPtr pExpr2 = pExpr1.as<Constructor>().as<StructConstructor>()->get(j)->getValue();

                    pExpr = new Binary(Binary::BOOL_AND,
                        new Binary(Binary::LESS_OR_EQUALS, pRangeArrays.get(j)->getMin(), pExpr2),
                        new Binary(Binary::LESS_OR_EQUALS, pExpr2, pRangeArrays.get(j)->getMax()));

                    if(ExpressionPtr pCond = collectConditions())
                        m_pNewModule->getLemmas().add(new LemmaDeclaration(
                            new Binary(Binary::IMPLIES, pCond, pExpr),
                            new Label(m_nameGen.makeNameLemmaArrayMod())));
                    else
                        m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                            new Label(m_nameGen.makeNameLemmaArrayMod())));
                }

                for(int j = i+1; j < _node.getNewValues().as<ArrayConstructor>()->size(); j++) {

                    ExpressionPtr pExpr2 = _node.getNewValues().as<ArrayConstructor>()->get(j).as<ElementDefinition>()->getIndex();

                    if(pExpr2->getKind() == Expression::CONSTRUCTOR &&
                        pExpr2.as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS &&
                        pExpr1.as<Constructor>().as<StructConstructor>()->size() ==
                        pExpr2.as<Constructor>().as<StructConstructor>()->size()) {

                        for(int j = 0; j < pExpr1.as<Constructor>().as<StructConstructor>()->size(); j++) {

                            ExpressionPtr pExpr3 = pExpr1.as<Constructor>().as<StructConstructor>()->get(j)->getValue();
                            ExpressionPtr pExpr4 = pExpr2.as<Constructor>().as<StructConstructor>()->get(j)->getValue();

                            if(j == 0)
                                pExpr = new Binary(Binary::NOT_EQUALS, pExpr3, pExpr4);
                            else
                                pExpr = new Binary(Binary::BOOL_OR, pExpr,
                                    new Binary(Binary::NOT_EQUALS, pExpr3, pExpr4));
                        }
                        m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                            new Label(m_nameGen.makeNameLemmaArrayMod())));
                    }
                }
            }
        }
    }
    return true;
}

bool CollectPreConditions::visitAssignment(Assignment &_node) {

//для ArrayConstrucor надо в visitArrayIteration
    ExpressionPtr pCond = collectConditions();
//lemmas for ArrayConstrucor
//x = for(... var j ...) { case A1 : ... case An : ...}
    if(_node.getExpression()->getKind() == Expression::CONSTRUCTOR &&
       _node.getExpression().as<Constructor>()->getConstructorKind() == Constructor::ARRAY_ITERATION) {

        ArrayIterationPtr pArrayIt = _node.getExpression().as<Constructor>().as<ArrayIteration>();

        RangePtr pRangeArray = arrayRangeWithCurrentParams(_node.getLValue());
        Collection<Range> pRangeArrays = arrayRangesWithCurrentParams(_node.getLValue());

//Ai doesn't intersect with Aj
        for(size_t i = 0; i < pArrayIt->size(); ++i) {
            for(size_t k = 0; k < pArrayIt->get(i)->getConditions().size(); ++k) {
                for(size_t j = i; j < pArrayIt->size(); ++j) {

                    size_t l = (i == j) ? k+1: 0;
                    for(; l < pArrayIt->get(j)->getConditions().size(); ++l) {

                        ExpressionPtr pExpr1 = pArrayIt->get(i)->getConditions().get(k);
                        ExpressionPtr pExpr2 = pArrayIt->get(j)->getConditions().get(l);
                        ExpressionPtr pExpr3;

                        //size() == 1  correct?
                        if(pArrayIt->getIterators().size() == 1) {
                            pExpr3 = caseNonintersection(pExpr1, pExpr2);
                        }
                        else {
                            if(pExpr1->getKind() == Expression::CONSTRUCTOR &&
                               pExpr2->getKind() == Expression::CONSTRUCTOR &&
                               pExpr1.as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS &&
                               pExpr2.as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

                                for(int m = 0; m < pArrayIt->getIterators().size(); m++) {

                                     ExpressionPtr pExpr4 = pExpr1.as<Constructor>().as<StructConstructor>()->get(m)->getValue();
                                     ExpressionPtr pExpr5 = pExpr2.as<Constructor>().as<StructConstructor>()->get(m)->getValue();
                                     if(m == 0)
                                         pExpr3 = caseNonintersection(pExpr4, pExpr5);
                                     else
                                         pExpr3 = new Binary(Binary::BOOL_OR, pExpr3, caseNonintersection(pExpr4, pExpr5));
                                }
                            }
                        }

                        if(pCond)
                            m_pNewModule->getLemmas().add(new LemmaDeclaration(
                                new Binary(Binary::IMPLIES, pCond, pExpr3),
                                new Label(m_nameGen.makeNameLemmaArrayCons())));
                        else
                            m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr3,
                                new Label(m_nameGen.makeNameLemmaArrayCons())));
                    }
                }
            }
        }

//Ai contains in array's range
        for(size_t i = 0; i < pArrayIt->size(); ++i) {
            for(size_t k = 0; k < pArrayIt->get(i)->getConditions().size(); ++k) {

                ExpressionPtr pExpr1 = pArrayIt->get(i)->getConditions().get(k);
                ExpressionPtr pExpr2;

                if(pArrayIt->getIterators().size() == 1) {
                    if(pExpr1->getKind() == Expression::TYPE &&
                        pExpr1.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                        RangePtr pRange = pExpr1.as<TypeExpr>()->getContents().as<Range>();

                        pExpr2 = new Binary(Binary::BOOL_AND,
                            new Binary(Binary::GREATER_OR_EQUALS, pRange->getMin(), pRangeArray->getMin()),
                            new Binary(Binary::LESS_OR_EQUALS, pRange->getMax(), pRangeArray->getMax()));
                    }
                    else {
                        pExpr2 = new Binary(Binary::BOOL_AND,
                            new Binary(Binary::GREATER_OR_EQUALS, pExpr1, pRangeArray->getMin()),
                            new Binary(Binary::LESS_OR_EQUALS, pExpr1, pRangeArray->getMax()));
                    }
                }
                else {
                    if(pExpr1->getKind() == Expression::CONSTRUCTOR &&
                       pExpr1.as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

                        for(int m = 0; m < pArrayIt->getIterators().size(); m++) {

                            ExpressionPtr pExpr3 = pExpr1.as<Constructor>().as<StructConstructor>()->get(m)->getValue();
                            ExpressionPtr pExpr4;

                            if(pExpr3->getKind() == Expression::TYPE &&
                               pExpr3.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                                RangePtr pRange = pExpr3.as<TypeExpr>()->getContents().as<Range>();

                                pExpr4 = new Binary(Binary::BOOL_AND,
                                    new Binary(Binary::GREATER_OR_EQUALS, pRange->getMin(), pRangeArrays.get(m)->getMin()),
                                    new Binary(Binary::LESS_OR_EQUALS, pRange->getMax(), pRangeArrays.get(m)->getMax()));
                            }
                            else {
                                pExpr4 = new Binary(Binary::BOOL_AND,
                                    new Binary(Binary::GREATER_OR_EQUALS, pExpr3, pRangeArrays.get(m)->getMin()),
                                    new Binary(Binary::LESS_OR_EQUALS, pExpr3, pRangeArrays.get(m)->getMax()));
                            }

                            if(m == 0)
                                pExpr2 = pExpr4;
                            else
                                pExpr2 = new Binary(Binary::BOOL_AND, pExpr2, pExpr4);
                        }
                    }
                }

                if(pCond)
                    m_pNewModule->getLemmas().add(new LemmaDeclaration(
                        new Binary(Binary::IMPLIES, pCond, pExpr2),
                        new Label(m_nameGen.makeNameLemmaArrayCons())));
                else
                    m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr2,
                        new Label(m_nameGen.makeNameLemmaArrayCons())));
            }
        }

//case ArrayConstrucor doesn't have default
        if(!pArrayIt->getDefault()) {

            ExpressionPtr pExpr, pExpr1;

            for(size_t i = 0; i < pArrayIt->size(); ++i) {
                for(size_t k = 0; k < pArrayIt->get(i)->getConditions().size(); ++k) {

                ExpressionPtr pExpr2 = pArrayIt->get(i)->getConditions().get(k);

                if(pArrayIt->getIterators().size() == 1) {

                    VariableReferencePtr pVar = new VariableReference(pArrayIt->getIterators().get(0));

                    if(pExpr2->getKind() == Expression::TYPE &&
                        pExpr2.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                        RangePtr pRange = pExpr2.as<TypeExpr>()->getContents().as<Range>();

                        pExpr1 = new Binary(Binary::BOOL_AND,
                            new Binary(Binary::LESS_OR_EQUALS, pRange->getMin(), pVar),
                            new Binary(Binary::LESS_OR_EQUALS, pVar, pRange->getMax()));
                    }
                    else {
                        pExpr1 = new Binary(Binary::EQUALS, pVar, pExpr2);
                    }
                }
                else {
                    if(pExpr2->getKind() == Expression::CONSTRUCTOR &&
                       pExpr2.as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

                        for(int m = 0; m < pArrayIt->getIterators().size(); m++) {

                            VariableReferencePtr pVar = new VariableReference(pArrayIt->getIterators().get(m));
                            ExpressionPtr pExpr3 = pExpr2.as<Constructor>().as<StructConstructor>()->get(m)->getValue();
                            ExpressionPtr pExpr4;

                            if(pExpr3->getKind() == Expression::TYPE &&
                               pExpr3.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                                RangePtr pRange = pExpr3.as<TypeExpr>()->getContents().as<Range>();

                                pExpr4 = new Binary(Binary::BOOL_AND,
                                    new Binary(Binary::LESS_OR_EQUALS, pRange->getMin(), pVar),
                                    new Binary(Binary::LESS_OR_EQUALS, pVar, pRange->getMax()));
                            }
                            else {
                                pExpr4 = new Binary(Binary::EQUALS, pVar, pExpr3);
                            }

                            if(m == 0)
                                pExpr1 = pExpr4;
                            else
                                pExpr1 = new Binary(Binary::BOOL_AND, pExpr1, pExpr4);
                        }
                    }
                }

                if(i == 0 && k == 0)
                    pExpr = pExpr1;
                else
                    pExpr = new Binary(Binary::BOOL_OR, pExpr, pExpr1);
                }
            }

            for(int m = 0; m < pArrayIt->getIterators().size(); m++) {

                VariableReferencePtr pVar = new VariableReference(pArrayIt->getIterators().get(m));
                if(m == 0)
                    pExpr1 = new Binary(Binary::BOOL_AND,
                        new Binary(Binary::LESS_OR_EQUALS, pRangeArrays.get(0)->getMin(), pVar),
                        new Binary(Binary::LESS_OR_EQUALS, pVar, pRangeArrays.get(0)->getMax()));
                else
                    pExpr1 = new Binary(Binary::BOOL_AND, pExpr1,
                        new Binary(Binary::BOOL_AND,
                        new Binary(Binary::LESS_OR_EQUALS, pRangeArrays.get(m)->getMin(), pVar),
                        new Binary(Binary::LESS_OR_EQUALS, pVar, pRangeArrays.get(m)->getMax())));
            }

            pExpr = new Binary(Binary::IFF, pExpr1, pExpr);

            m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                new Label(m_nameGen.makeNameLemmaArrayCons())));
        }
    }

//Compatibility
    if(_node.getExpression()->getKind() == Expression::VAR) {

        ExpressionPtr pExpr;
        TypePtr pTypeL = _node.getLValue()->getType();
        TypePtr pTypeR = _node.getExpression()->getType();

        if(pTypeL->getKind() == Type::NAMED_REFERENCE)
            pTypeL = pTypeL.as<NamedReferenceType>()->getDeclaration()->getType();

        if(pTypeR->getKind() == Type::NAMED_REFERENCE)
            pTypeR = pTypeR.as<NamedReferenceType>()->getDeclaration()->getType();

///for subtypes
        if(pTypeL->getKind() == Type::SUBTYPE && pTypeR->getKind() == Type::SUBTYPE) {

            ExpressionPtr pExprL = pTypeL.as<Subtype>()->getExpression();
            VarSubstitute substitute(pTypeL.as<Subtype>()->getParam(), _node.getExpression());
            substitute.traverseNode(*(pExprL.ptr()));

            ExpressionPtr pExprR = pTypeR.as<Subtype>()->getExpression();
            substitute = VarSubstitute(pTypeR.as<Subtype>()->getParam(), _node.getExpression());
            substitute.traverseNode(*(pExprR.ptr()));

            if(pCond)
                pExpr = new Binary(Binary::BOOL_AND, pCond, pExprR);
            else
                pExpr = pExprR;

            pExpr = new Binary(Binary::IMPLIES, pExpr, pExprL);

            m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                new Label(m_nameGen.makeNameLemmaAssignment())));
        }

///for arrays
        if(pTypeL->getKind() == Type::PARAMETERIZED &&
           pTypeL.as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY &&
           pTypeR->getKind() == Type::PARAMETERIZED &&
           pTypeR.as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY) {

            Collection<Range> rangesL = arrayRangesWithCurrentParams(_node.getLValue());
            Collection<Range> rangesR = arrayRangesWithCurrentParams(_node.getExpression());
            ExpressionPtr pExpr;

            if(rangesL.size() == rangesR.size()) {
                for(int i = 0; i < rangesL.size(); i++) {
                    if(i == 0)
                        pExpr = new Binary(Binary::BOOL_AND,
                            new Binary(Binary::EQUALS, rangesL.get(0)->getMin(), rangesR.get(0)->getMin()),
                            new Binary(Binary::EQUALS, rangesL.get(0)->getMax(), rangesR.get(0)->getMax()));
                    else
                        pExpr = new Binary(Binary::BOOL_AND, pExpr, new Binary(Binary::BOOL_AND,
                            new Binary(Binary::EQUALS, rangesL.get(i)->getMin(), rangesR.get(i)->getMin()),
                            new Binary(Binary::EQUALS, rangesL.get(i)->getMax(), rangesR.get(i)->getMax())));
                }

                if(pCond)
                    m_pNewModule->getLemmas().add(new LemmaDeclaration(
                        new Binary(Binary::IMPLIES, pCond, pExpr),
                        new Label(m_nameGen.makeNameLemmaAssignment())));
                else
                    m_pNewModule->getLemmas().add(new LemmaDeclaration(pExpr,
                        new Label(m_nameGen.makeNameLemmaAssignment())));
            }
            ///error lemma
            else {
                LiteralPtr pLiteral = new Literal(Number("0" , Number::INTEGER));

                m_pNewModule->getLemmas().add(new LemmaDeclaration(
                    new Binary(Binary::NOT_EQUALS, pLiteral, pLiteral),
                    new Label(m_nameGen.makeNameLemmaAssignment())));
            }
        }
    }

///lemmas for union of arrays
    if(_node.getExpression()->getKind() == Expression::BINARY) {

        Collection<Expression> exprs;
        ExpressionPtr pExpr = _node.getExpression();
        ExpressionPtr pExpr1, pExpr2, pExprEqual, pExprNonintersect;

        while(pExpr->getKind() == Expression::BINARY) {

            exprs.add(pExpr.as<Binary>()->getRightSide());
            pExpr = pExpr.as<Binary>()->getLeftSide();
        }
        exprs.add(pExpr);

//checking that expressions are arrays and have same dimensions
        bool bArrays = true;
        size_t dim;

        for(int i = 0; i < exprs.size(); i++) {

            TypePtr pType = exprs.get(i)->getType();

            if(pType->getKind() == Type::NAMED_REFERENCE)
                pType = pType.as<NamedReferenceType>()->getDeclaration()->getType();

            if(!(pType && pType->getKind() == Type::PARAMETERIZED &&
                pType.as<ParameterizedType>()->getActualType()->getKind() == Type::ARRAY))

                bArrays = false;

            Collection<Range> ranges = arrayRangesWithCurrentParams(exprs.get(i));
            if(i == 0)
                dim = ranges.size();
            else
                if(dim != ranges.size())
                    bArrays = false;
        }

        if(bArrays) {
            for(int i = exprs.size() - 1; i >= 0 ; i--) {

                Collection<Range> ranges = arrayRangesWithCurrentParams(exprs.get(i));

                for(int j = 0; j < ranges.size(); j++) {

                    std::wstring strName = fmtInt(j+1, L"i%u");
                    VariableReferencePtr pVar = new VariableReference(strName);

                    if(j == 0)
                        pExpr1 = new Binary(Binary::BOOL_AND,
                            new Binary(Binary::LESS_OR_EQUALS, ranges.get(0)->getMin(), pVar),
                            new Binary(Binary::LESS_OR_EQUALS, pVar, ranges.get(0)->getMax()));
                    else
                        pExpr1 = new Binary(Binary::BOOL_AND, pExpr1,
                            new Binary(Binary::BOOL_AND,
                            new Binary(Binary::LESS_OR_EQUALS, ranges.get(j)->getMin(), pVar),
                            new Binary(Binary::LESS_OR_EQUALS, pVar, ranges.get(j)->getMax())));
                }

                if(i == exprs.size() - 1)
                    pExprEqual = pExpr1;
                else
                    pExprEqual = new Binary(Binary::BOOL_OR, pExprEqual, pExpr1);

                //nonintersection
                for(int j = i-1; j >= 0 ; j--) {

                    Collection<Range> ranges1 = arrayRangesWithCurrentParams(exprs.get(j));

                    for(int k = 0; k < ranges.size(); k++) {

                        if(k == 0)
                            pExprNonintersect = new Binary(Binary::BOOL_OR,
                                new Binary(Binary::LESS, ranges.get(k)->getMax(), ranges1.get(k)->getMin()),
                                new Binary(Binary::GREATER, ranges.get(k)->getMin(), ranges1.get(k)->getMax()));
                        else
                            pExprNonintersect = new Binary(Binary::BOOL_OR, pExprNonintersect,
                                new Binary(Binary::BOOL_OR,
                                new Binary(Binary::LESS, ranges.get(k)->getMax(), ranges1.get(k)->getMin()),
                                new Binary(Binary::GREATER, ranges.get(k)->getMin(), ranges1.get(k)->getMax())));
                    }

                    m_pNewModule->getLemmas().add(new LemmaDeclaration(pExprNonintersect,
                        new Label(m_nameGen.makeNameLemmaArrayUnion())));
                }
            }

            Collection<Range> ranges = arrayRangesWithCurrentParams(_node.getLValue());

            for(int j = 0; j < ranges.size(); j++) {

                std::wstring strName = fmtInt(j+1, L"i%u");
                VariableReferencePtr pVar = new VariableReference(strName);

                if(j == 0)
                    pExpr1 = new Binary(Binary::BOOL_AND,
                        new Binary(Binary::LESS_OR_EQUALS, ranges.get(0)->getMin(), pVar),
                        new Binary(Binary::LESS_OR_EQUALS, pVar, ranges.get(0)->getMax()));
                else
                    pExpr1 = new Binary(Binary::BOOL_AND, pExpr1,
                        new Binary(Binary::BOOL_AND,
                        new Binary(Binary::LESS_OR_EQUALS, ranges.get(j)->getMin(), pVar),
                        new Binary(Binary::LESS_OR_EQUALS, pVar, ranges.get(j)->getMax())));
            }
            pExprEqual = new Binary(Binary::IFF, pExpr1, pExprEqual);

            m_pNewModule->getLemmas().add(new LemmaDeclaration(pExprEqual,
                new Label(m_nameGen.makeNameLemmaArrayUnion())));
        }
    }
    return true;
}

// x/y => lemma y != 0
bool CollectPreConditions::visitBinary(Binary &_node) {
    if(_node.getOperator() == Binary::DIVIDE) {
        LiteralPtr pLiteral = new Literal(Number("0" , Number::INTEGER));

        m_pNewModule->getLemmas().add(new LemmaDeclaration(
            new Binary(Binary::IMPLIES, collectConditions(),
            new Binary(Binary::NOT_EQUALS, _node.getRightSide(), pLiteral)),
            new Label(m_nameGen.makeNameLemmaDivide())));
    }

    return true;
}

bool CollectPreConditions::visitVariableDeclaration(VariableDeclaration &_node) {
    return true;
}

bool CollectPreConditions::visitNamedValue(NamedValue &_node) {

    TypePtr pType = _node.getType();
    if(pType && pType->getKind() == Type::NAMED_REFERENCE) {
        pType = pType.as<NamedReferenceType>()->getDeclaration()->getType();

        if(pType && pType->getKind() == Type::PARAMETERIZED) {

            _node.setType(clone(_node.getType()));
            Collection<Expression> namedRefArgs(_node.getType().as<NamedReferenceType>()->getArgs());
            NamedValues paramTypeParams(pType.as<ParameterizedType>()->getParams());
            TypePtr paramTypeActualType(pType.as<ParameterizedType>()->getActualType());

            for(int i = 0; i < namedRefArgs.size() && i < paramTypeParams.size(); i++) {
                if(namedRefArgs.get(i)->getKind() == Expression::TYPE)
                    pType = namedRefArgs.get(i).as<TypeExpr>()->getContents();

                VarSubstitute substitute(paramTypeParams.get(i), pType);
                substitute.traverseNode(*(paramTypeActualType.ptr()));
            }

        }
    }
    return true;
}

//for ArrayConstructor and Switch
ExpressionPtr CollectPreConditions::caseNonintersection(ExpressionPtr _pExpr1,ExpressionPtr _pExpr2) {
    ExpressionPtr pExpr;

    if((_pExpr1->getKind() != Expression::TYPE) && (_pExpr2->getKind() != Expression::TYPE))
        pExpr = new Binary(Binary::NOT_EQUALS, _pExpr1, _pExpr2);

    if((_pExpr1->getKind() != Expression::TYPE) && (_pExpr2->getKind() == Expression::TYPE)
        && (_pExpr2.as<TypeExpr>()->getContents()->getKind() == Type::RANGE)) {

        RangePtr pRange = _pExpr2.as<TypeExpr>()->getContents().as<Range>();

        pExpr = new Binary(Binary::BOOL_OR,
            new Binary(Binary::LESS, _pExpr1, pRange->getMin()),
            new Binary(Binary::GREATER, _pExpr1, pRange->getMax()));
    }

    if((_pExpr2->getKind() != Expression::TYPE) && (_pExpr1->getKind() == Expression::TYPE)
        && (_pExpr1.as<TypeExpr>()->getContents()->getKind() == Type::RANGE)) {

        RangePtr pRange = _pExpr1.as<TypeExpr>()->getContents().as<Range>();

        pExpr = new Binary(Binary::BOOL_OR,
            new Binary(Binary::LESS, _pExpr2, pRange->getMin()),
            new Binary(Binary::GREATER, _pExpr2, pRange->getMax()));
    }

    if((_pExpr1->getKind() == Expression::TYPE) && (_pExpr1.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) &&
        (_pExpr2->getKind() == Expression::TYPE) && (_pExpr2.as<TypeExpr>()->getContents()->getKind() == Type::RANGE)) {

        RangePtr pRange1 = _pExpr1.as<TypeExpr>()->getContents().as<Range>();
        RangePtr pRange2 = _pExpr2.as<TypeExpr>()->getContents().as<Range>();

        pExpr = new Binary(Binary::BOOL_OR,
            new Binary(Binary::LESS, pRange1->getMax(), pRange2->getMin()),
            new Binary(Binary::GREATER, pRange1->getMin(), pRange2->getMax()));
    }

    return pExpr;
}

//into definition of array substitute params from current array
RangePtr CollectPreConditions::arrayRangeWithCurrentParams(ExpressionPtr _pArray) {

//Никита:    Попробуй разнести по разным функам подстановку аргументов в произвольный параметризованный тип и выдирание диапазонов из массива
    RangePtr pNewRange;

    if(_pArray->getKind() == Expression::VAR) {

        NamedValues params = _pArray.as<VariableReference>()->getTarget().as<Param>()->
            getType().as<NamedReferenceType>()->getDeclaration().as<TypeDeclaration>()->
            getType().as<ParameterizedType>()->getParams();

        std::list<TypePtr> dims;
        _pArray.as<VariableReference>()->getTarget().as<Param>()->
            getType().as<NamedReferenceType>()->getDeclaration().as<TypeDeclaration>()->
            getType().as<ParameterizedType>()->getActualType().as<DerivedType>().as<ArrayType>()->getDimensions(dims);
        TypePtr pType = getNotNamedReferenceType(dims.front());
        RangePtr pRange = NULL;
        if (pType && pType->getKind() == Type::SUBTYPE)
            pType.as<Subtype>()->asRange();

        Collection<Expression> args = _pArray.as<VariableReference>()->getTarget().as<Param>()->
            getType().as<NamedReferenceType>()->getArgs();

        pNewRange = new Range(pRange->getMin(), pRange->getMax());

        for(int i = 0; i < params.size(); i++) {
            pNewRange = Expression::substitute(pNewRange, new VariableReference(params.get(i)), args.get(i)).as<Range>();
        }
    }

    return pNewRange;
}

Collection<Range> CollectPreConditions::arrayRangesWithCurrentParams(ExpressionPtr _pArray) {

    Collection<Range> newRanges;

    if(_pArray->getKind() == Expression::VAR) {

        NamedValues params = _pArray.as<VariableReference>()->getTarget().as<Param>()->
            getType().as<NamedReferenceType>()->getDeclaration().as<TypeDeclaration>()->
            getType().as<ParameterizedType>()->getParams();

        ArrayTypePtr pArray = _pArray.as<VariableReference>()->getTarget().as<Param>()->
            getType().as<NamedReferenceType>()->getDeclaration().as<TypeDeclaration>()->
            getType().as<ParameterizedType>()->getActualType().as<DerivedType>().as<ArrayType>();

        Collection<Range> ranges;

        if (pArray)
            getRanges(*pArray, ranges);

        Collection<Expression> args = _pArray.as<VariableReference>()->getTarget().as<Param>()->
            getType().as<NamedReferenceType>()->getArgs();

        //if borders of range is complicated, then /Collection<Range> ranges/ changes
        //попробовать ranges.clone()
        for(int j = 0; j < ranges.size(); j++) {
            RangePtr pNewRange = new Range(ranges.get(j)->getMin(), ranges.get(j)->getMax());

            for(int i = 0; i < params.size(); i++) {
                pNewRange = Expression::substitute(pNewRange, new VariableReference(params.get(i)), args.get(i)).as<Range>();
            }

            newRanges.add(pNewRange);
        }
    }

    return newRanges;
}

ExpressionPtr CollectPreConditions::varBelongsSetOneDimension(VariableReferencePtr _pVar, ExpressionPtr _pExpr) {

    ExpressionPtr pExpr;

    if(_pExpr->getKind() != Expression::TYPE) {
        pExpr = new Binary(Binary::EQUALS, _pVar, _pExpr);
    }
    else {
        if(_pExpr.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {
            RangePtr range = _pExpr.as<TypeExpr>()->getContents().as<Range>();

            pExpr = new Binary(Binary::BOOL_AND,
                new Binary(Binary::LESS_OR_EQUALS, range->getMin(), _pVar),
                new Binary(Binary::LESS_OR_EQUALS, _pVar, range->getMax()));
        }
    }
    return pExpr;
}

ExpressionPtr CollectPreConditions::varsBelongSetSeveralDimensions(Collection<VariableReference> _vars, ExpressionPtr _pExpr) {

    ExpressionPtr pExpr;

    if(_pExpr->getKind() == Expression::CONSTRUCTOR &&
        _pExpr.as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {

        for(int m = 0; m < _vars.size(); m++) {

            ExpressionPtr pExpr1 = _pExpr.as<Constructor>().as<StructConstructor>()->get(m)->getValue();
            ExpressionPtr pExpr2;

            if(pExpr1->getKind() == Expression::TYPE &&
                pExpr1.as<TypeExpr>()->getContents()->getKind() == Type::RANGE) {

                RangePtr pRange = pExpr1.as<TypeExpr>()->getContents().as<Range>();

                pExpr2 = new Binary(Binary::BOOL_AND,
                    new Binary(Binary::LESS_OR_EQUALS, pRange->getMin(), _vars.get(m)),
                    new Binary(Binary::LESS_OR_EQUALS, _vars.get(m), pRange->getMax()));
            }
            else {
                pExpr2 = new Binary(Binary::EQUALS, _vars.get(m), pExpr1);
            }

            if(m == 0)
                pExpr = pExpr2;
            else
                pExpr = new Binary(Binary::BOOL_AND, pExpr, pExpr2);
        }
    }
    return pExpr;
}

TypePtr CollectPreConditions::getNotNamedReferenceType(TypePtr _pType) {
    while(_pType && _pType->getKind() == Type::NAMED_REFERENCE) {
        _pType = _pType.as<NamedReferenceType>()->getDeclaration()->getType();
    }
    return _pType;
}

///Executing function
Auto<Module> ir::processPreConditions(Module &_module) {
    Auto<Module> pNewModule = new Module();
    CollectPreConditions collector(*pNewModule);
    collector.traverseNode(_module);
    return pNewModule;
}

void ir::getRanges(const ArrayType &_array, Collection<Range> &_ranges) {
    std::list<TypePtr> dims;
    _array.getDimensions(dims);

    for(std::list<TypePtr>::iterator i = dims.begin(); i != dims.end(); ++i) {
        TypePtr pType = CollectPreConditions::getNotNamedReferenceType(*i);
        if (!pType) {
            _ranges.add(NULL);
            continue;
        }

        switch (pType->getKind()) {
            case Type::SUBTYPE:
                pType = pType.as<Subtype>()->asRange();
                // no brake.
            case Type::RANGE:
                _ranges.add(pType.as<Range>());
                break;
            default:
                _ranges.add(NULL);
        }
    }
}
