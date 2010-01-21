/// \file parser.cpp
///

#include "parser.h"
#include "utils.h"
#include "parser_context.h"

#include "ir/declarations.h"
#include "ir/expressions.h"
#include "ir/statements.h"
#include "ir/expressions.h"
#include "ir/types.h"
#include "ir/base.h"

#include <iostream>
#include <algorithm>
#include <map>

using namespace ir;
using namespace lexer;

#define PARSER_FN(_Node,_Name,...) _Node * (CParser::* _Name) (CContext & _ctx, ## __VA_ARGS__)

#define CASE_BUILTIN_TYPE \
    case IntType: \
    case NatType: \
    case RealType: \
    case BoolType: \
    case CharType: \
    case Subtype: \
    case Enum: \
    case Struct: \
    case Union: \
    case StringType: \
    case Seq: \
    case Set: \
    case Array: \
    case Map: \
    case List: \
    case Var

#define ERROR(_CTX,_RETVAL,...) \
    do { \
        (_CTX).fmtError(__VA_ARGS__); \
        (_CTX).fmtError(L"Near token %ls", fmtQuote((_CTX).getValue()).c_str()); \
        (_CTX).fmtError(L"At %s:%d, %s", __FILE__, __LINE__, __FUNCTION__); \
        (_CTX).fail(); \
        return _RETVAL; \
    } while (0)

#define UNEXPECTED(_CTX,_TOK) \
    do { \
        (_CTX).fmtError(L"Expected %ls, got: %ls", fmtQuote(L##_TOK).c_str(), \
                fmtQuote((_CTX).getValue()).c_str()); \
        (_CTX).fail(); \
        return NULL; \
    } while (0)

#define WARNING(_CTX,...) (_CTX).fmtWarning(__VA_ARGS__)

#define DEBUG(_FMT,...) \
    do { \
        fwprintf(stderr, (_FMT), ## __VA_ARGS__); \
        fwprintf(stderr, L"\n"); \
    } while (0)

#define TOK_S(_CTX) (fmtQuote((_CTX).getValue()).c_str())

struct operator_t {
    int nPrecedence, nBinary, nUnary;

    operator_t() : nPrecedence(-1), nBinary(-1), nUnary(-1) {}

    operator_t(int _nPrecedence, int _nBinary = -1, int _nUnary = -1) :
        nPrecedence(_nPrecedence), nBinary(_nBinary), nUnary(_nUnary) {}
};

class CParser {
public:
    CParser(tokens_t & _tokens) : m_tokens(_tokens) { initOps(); }

    bool parseModule(CContext & _ctx, CModule * & _pModule);
    bool parseModuleHeader(CContext & _ctx, CModule * _pModule);
    bool parseImport(CContext & _ctx, CModule * _pModule);

    CType * parseType(CContext & _ctx);
    CType * parseDerivedTypeParameter(CContext & _ctx);
    CArrayType * parseArrayType(CContext & _ctx);
    CMapType * parseMapType(CContext & _ctx);
    CRange * parseRange(CContext & _ctx);
    CNamedReferenceType * parseTypeReference(CContext & _ctx);
    CStructType * parseStructType(CContext & _ctx);
    CUnionType * parseUnionType(CContext & _ctx);
    CEnumType * parseEnumType(CContext & _ctx);
    CSubtype * parseSubtype(CContext & _ctx);
    CPredicateType * parsePredicateType(CContext & _ctx);
    CExpression * parseCastOrTypeReference(CContext & _ctx, CType * _pType);

    CTypeDeclaration * parseTypeDeclaration(CContext & _ctx);
    CVariableDeclaration * parseVariableDeclaration(CContext & _ctx, bool _bLocal);
    CFormulaDeclaration * parseFormulaDeclaration(CContext & _ctx);
    CExpression * parseExpression(CContext & _ctx, int _nFlags);
    CExpression * parseExpression(CContext & _ctx) { return parseExpression(_ctx, 0); }
    CExpression * parseSubexpression(CContext & _ctx, CExpression * _lhs, int _minPrec, int _nFlags = 0);
    CExpression * parseAtom(CContext & _ctx, int _nFlags);
    CExpression * parseAtom(CContext & _ctx) { return parseAtom(_ctx, 0); }
    CExpression * parseComponent(CContext & _ctx, CExpression & _base);
    CFormula * parseFormula(CContext & _ctx);
    CArrayIteration * parseArrayIteration(CContext & _ctx);
    CArrayPartExpr * parseArrayPart(CContext & _ctx, CExpression & _base);
    CFunctionCall * parseFunctionCall(CContext & _ctx, CExpression & _base);
    CBinder * parseBinder(CContext & _ctx, CExpression & _base);
    CReplacement * parseReplacement(CContext & _ctx, CExpression & _base);
    CLambda * parseLambda(CContext & _ctx);
    bool parsePredicateParamsAndBody(CContext & _ctx, CAnonymousPredicate & _pred);
    CPredicate * parsePredicate(CContext & _ctx);
    CProcess * parseProcess(CContext & _ctx);

    CStatement * parseStatement(CContext & _ctx);
    CBlock * parseBlock(CContext & _ctx);
    CStatement * parseAssignment(CContext & _ctx);
    CMultiassignment * parseMultiAssignment(CContext & _ctx);
    CSwitch * parseSwitch(CContext & _ctx);
    CIf * parseConditional(CContext & _ctx);
    CJump * parseJump(CContext & _ctx);
//    CStatement * parsePragma(CContext & _ctx);
    CReceive * parseReceive(CContext & _ctx);
    CSend * parseSend(CContext & _ctx);
    CWith * parseWith(CContext & _ctx);
    CFor * parseFor(CContext & _ctx);
    CWhile * parseWhile(CContext & _ctx);
    CBreak * parseBreak(CContext & _ctx);
    CCall * parseCall(CContext & _ctx);
    CExpression * parseCallResult(CContext & _ctx, CVariableDeclaration * & _pDecl);
    bool parseCallResults(CContext & _ctx, CCall & _call, CCollection<CExpression> & _list);

    CContext * parsePragma(CContext & _ctx);

    bool parseDeclarations(CContext & _ctx, CModule & _module);

//    CDeclarationGroup

    // Parameter parsing constants.
    enum {
        AllowEmptyNames = 0x01,
        OutputParams = 0x02,
        AllowAstersk = 0x04,
    };

    // Expression parsing constants.
    enum {
        AllowFormulas = 0x01,
        RestrictTypes = 0x02,
    };

    template<class _Param>
    bool parseParamList(CContext & _ctx, CCollection<_Param> & _params,
            PARSER_FN(_Param,_parser,int), int _nFlags = 0);

    template<class _Node, class _Base>
    bool parseList(CContext & _ctx, CCollection<_Node, _Base> & _list, PARSER_FN(_Node,_parser),
            int _startMarker, int _endMarker, int _delimiter);

    bool parseActualParameterList(CContext & _ctx, CCollection<CExpression> & _exprs) {
        return parseList(_ctx, _exprs, & CParser::parseExpression, LeftParen, RightParen, Comma);
    }

    bool parseArrayIndices(CContext & _ctx, CCollection<CExpression> & _exprs) {
        return parseList(_ctx, _exprs, & CParser::parseExpression, LeftBracket, RightBracket, Comma);
    }

    CParam * parseParam(CContext & _ctx, int _nFlags = 0);
//    CStructFieldDefinition * parseParam(CContext & _ctx);
    CNamedValue * parseVariableName(CContext & _ctx, int _nFlags = 0);
    CEnumValue * parseEnumValue(CContext & _ctx);
    CNamedValue * parseNamedValue(CContext & _ctx);
    CElementDefinition * parseArrayElement(CContext & _ctx);
    CElementDefinition * parseMapElement(CContext & _ctx);
    CStructFieldDefinition * parseFieldDefinition(CContext & _ctx);
    CUnionConstructorDefinition * parseConstructorDefinition(CContext & _ctx);

    template<class _T>
    _T * findByName(const CCollection<_T> & _list, const std::wstring & _name);

    template<class _T>
    size_t findByNameIdx(const CCollection<_T> & _list, const std::wstring & _name);

    typedef std::map<std::wstring, CBranch *> branch_map_t;

    template<class _Pred>
    bool parsePreconditions(CContext & _ctx, _Pred & _pred, branch_map_t & _branches);

    template<class _Pred>
    bool parsePostconditions(CContext & _ctx, _Pred & _pred, branch_map_t & _branches);

private:
    tokens_t & m_tokens;
    std::vector<operator_t> m_ops;

    void initOps();
    int getPrecedence(int _token, bool _bExpecColon) const;
    int getUnaryOp(int _token) const;
    int getBinaryOp(int _token) const;

    const CType * resolveBaseType(const CType * _pType) const;
    bool isTypeVariable(const CNamedValue * _pVar, const CType * & _pType) const;
    bool isTypeName(CContext & _ctx, const std::wstring & _name) const;
    bool fixupAsteriskedParameters(CContext & _ctx, CParams & _in, CParams & _out);

    bool isTypeVariable(const CNamedValue * _pVar) const {
        const CType * pType = NULL;
        return isTypeVariable(_pVar, pType);
    }
};

const CType * CParser::resolveBaseType(const CType * _pType) const {
    if (! _pType)
        return NULL;

    while (_pType) {
        if (_pType->getKind() == CType::NamedReference) {
            const CNamedReferenceType * pRef = (CNamedReferenceType *) _pType;

            if (pRef->getDeclaration()) {
                _pType = pRef->getDeclaration()->getType();
            } else if (pRef->getVariable()) {
                if (! isTypeVariable(pRef->getVariable(), _pType))
                    return _pType;
            }
        } else if (_pType->getKind() == CType::Parameterized) {
            _pType = ((CParameterizedType *) _pType)->getActualType();
        } else
            break;
    }

    return _pType;
}

bool CParser::isTypeVariable(const CNamedValue * _pVar, const CType * & _pType) const {
    if (! _pVar || ! _pVar->getType())
        return false;

    const CType * pType = _pVar->getType();

    if (! pType)
        return false;

    DEBUG(L"kind: %d", pType->getKind());

    if (pType->getKind() == CType::Type) {
        if (_pType) _pType = pType;
        return true;
    }

    if (pType->getKind() != CType::NamedReference)
        return false;

    const CNamedReferenceType * pRef = (const CNamedReferenceType *) pType;

    if (pRef->getDeclaration() != NULL) {
        if (_pType) _pType = pRef->getDeclaration()->getType();
        return true;
    }

    return false;
}

template<class _Node, class _Base>
bool CParser::parseList(CContext & _ctx, CCollection<_Node,_Base> & _list, PARSER_FN(_Node,_parser),
        int _startMarker, int _endMarker, int _delimiter)
{
    CContext & ctx = * _ctx.createChild(false);

    if (_startMarker >= 0 && ! ctx.consume(_startMarker))
        return false;

    CCollection<_Node> list;
    _Node * pNode = (this->*_parser) (ctx);

    if (! pNode)
        return false;

    list.add(pNode);

    while (_delimiter < 0 || ctx.consume(_delimiter)) {
        if (! (pNode = (this->*_parser) (ctx)))
            return false;
        list.add(pNode, false);
    }

    if (_endMarker >= 0 && ! ctx.consume(_endMarker))
        return false;

    for (size_t i = 0; i < list.size(); ++ i)
        _list.add(list.get(i));

    //    _list.append(list);
    _ctx.mergeChildren();

    return true;
}

template<class _T>
_T * CParser::findByName(const CCollection<_T> & _list, const std::wstring & _name) {
    const size_t cIdx = findByNameIdx(_list, _name);
    return cIdx == (size_t) -1 ? _list.get(cIdx) : NULL;
}

template<class _T>
size_t CParser::findByNameIdx(const CCollection<_T> & _list, const std::wstring & _name) {
    for (size_t i = 0; i < _list.size(); ++ i)
        if (_list.get(i)->getName() == _name)
            return i;

    return (size_t) -1;
}

CArrayPartExpr * CParser::parseArrayPart(CContext & _ctx, CExpression & _base) {
    CContext & ctx = * _ctx.createChild(false);
    CArrayPartExpr * pParts = ctx.attach(new CArrayPartExpr());

    if (! parseArrayIndices(ctx, pParts->getIndices()))
        return NULL;

    pParts->setObject(& _base);
    _ctx.mergeChildren();

    return pParts;
}

CFunctionCall * CParser::parseFunctionCall(CContext & _ctx, CExpression & _base) {
    CContext & ctx = * _ctx.createChild(false);
    CFunctionCall * pCall = ctx.attach(new CFunctionCall());

    if (! parseActualParameterList(ctx, pCall->getParams()))
        return NULL;

    pCall->setPredicate(& _base);
    _ctx.mergeChildren();

    return pCall;
}

CBinder * CParser::parseBinder(CContext & _ctx, CExpression & _base) {
    CContext & ctx = * _ctx.createChild(false);
    CBinder * pBinder = ctx.attach(new CBinder());

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    CExpression * pParam = NULL;

    if (! ctx.consume(Ellipsis)) {
        if (ctx.consume(Underscore))
            pParam = NULL;
        else if (! (pParam = parseExpression(ctx)))
            ERROR(ctx, NULL, L"Error parsing expression");

        pBinder->getParams().add(pParam);

        while (ctx.consume(Comma)) {
            if (ctx.consume(Ellipsis))
                break;
            else if (ctx.consume(Underscore))
                pParam = NULL;
            else if (! (pParam = parseExpression(ctx)))
                ERROR(ctx, NULL, L"Error parsing expression");

            pBinder->getParams().add(pParam);
        }
    }

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    pBinder->setPredicate(& _base);
    _ctx.mergeChildren();

    return pBinder;
}

CReplacement * CParser::parseReplacement(CContext & _ctx, CExpression & _base) {
    CContext & ctx = * _ctx.createChild(false);
    CExpression * pNewValues = parseExpression(ctx);

    if (! pNewValues)
        ERROR(ctx, NULL, L"Error parsing replacement values");

    if (pNewValues->getKind() != CExpression::Constructor)
        ERROR(ctx, NULL, L"Constructor expected");

    CReplacement * pExpr = ctx.attach(new CReplacement());

    pExpr->setObject(& _base);
    pExpr->setNewValues((CConstructor *) pNewValues);
    _ctx.mergeChildren();

    return pExpr;
}

CExpression * CParser::parseComponent(CContext & _ctx, CExpression & _base) {
    CContext & ctx = * _ctx.createChild(false);
    CExpression * pExpr = NULL;

    if (ctx.is(Dot)) {
        const CType * pBaseType = resolveBaseType(_base.getType());

        if (! pBaseType || (pBaseType->getKind() != CType::Struct && pBaseType->getKind() != CType::Union))
            ERROR(ctx, NULL, L"Struct or union typed expression expected");

        const std::wstring & fieldName = ctx.scan(2, 1);
        CComponent * pExpr = NULL;

        if (pBaseType->getKind() == CType::Struct) {
            CStructType * pStruct = (CStructType *) pBaseType;
            const size_t cFieldIdx = findByNameIdx(pStruct->getFields(), fieldName);

            if (cFieldIdx != (size_t) -1)
                pExpr = new CStructFieldExpr(pStruct, cFieldIdx, false);
        } else {
            CUnionType * pUnion = (CUnionType *) pBaseType;
            const size_t cFieldIdx = findByNameIdx(pUnion->getAlternatives(), fieldName);

            if (cFieldIdx != (size_t) -1)
                pExpr = new CUnionAlternativeExpr(pUnion, cFieldIdx);
        }

        if (! pExpr)
            ERROR(ctx, NULL, L"Could not find component %ls", fieldName.c_str());

        _ctx.mergeChildren();
        _ctx.attach(pExpr);
        pExpr->setObject(& _base);

        return pExpr;
    } else if (ctx.is(LeftBracket)) {
        pExpr = parseArrayPart(ctx, _base);

        if (! pExpr)
            pExpr = parseReplacement(ctx, _base);
    } else if (ctx.is(LeftParen)) {
        pExpr = parseFunctionCall(ctx, _base);

        if (! pExpr)
            pExpr = parseBinder(ctx, _base);

        if (! pExpr)
            pExpr = parseReplacement(ctx, _base);
    } else if (ctx.in(LeftMapBracket, For)) {
        pExpr = parseReplacement(ctx, _base);
    }

    if (! pExpr)
        return NULL;

    _ctx.mergeChildren();

    return pExpr;
}

CArrayIteration * CParser::parseArrayIteration(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(true);

    if (! ctx.consume(For))
        UNEXPECTED(ctx, "for");

    CArrayIteration * pArray = _ctx.attach(new CArrayIteration());

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    if (! parseParamList(ctx, pArray->getIterators(), & CParser::parseVariableName))
        ERROR(ctx, NULL, L"Failed parsing list of iterators");

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    if (ctx.is(LeftBrace)) {
        CContext & ctxParts = * ctx.createChild(false);

        ++ ctxParts;

        while (ctxParts.is(Case)) {
            CArrayPartDefinition * pPart = ctxParts.attach(new CArrayPartDefinition());

            if (! parseList(ctxParts, pPart->getConditions(),
                    & CParser::parseExpression, Case, Colon, Comma))
                ERROR(ctxParts, NULL, L"Error parsing list of expressions");

            CExpression * pExpr = parseExpression(ctxParts);
            if (! pExpr)
                ERROR(ctxParts, NULL, L"Expression required");
            pPart->setExpression(pExpr);

            pArray->add(pPart);
        }

        if (ctxParts.is(Default, Colon)) {
            ctxParts.skip(2);
            CExpression * pExpr = parseExpression(ctxParts);
            if (! pExpr)
                ERROR(ctxParts, NULL, L"Expression required");
            pArray->setDefault(pExpr);
        }

        if (! pArray->empty() || pArray->getDefault()) {
            if (! ctxParts.consume(RightBrace))
                UNEXPECTED(ctxParts, "}");
            ctx.mergeChildren();
        }
    }

    if (pArray->empty() && ! pArray->getDefault()) {
        CExpression * pExpr = parseExpression(ctx);
        if (! pExpr)
            ERROR(ctx, NULL, L"Expression or parts definition required");
        pArray->setDefault(pExpr);
    }

    _ctx.mergeChildren();

    return pArray;
}

int CParser::getPrecedence(int _token, bool _bExpecColon) const {
    if (_bExpecColon && _token == Colon)
        return m_ops[Question].nPrecedence;
    return m_ops[_token].nPrecedence;
}

int CParser::getUnaryOp(int _token) const {
    return m_ops[_token].nUnary;
}

int CParser::getBinaryOp(int _token) const {
    return m_ops[_token].nBinary;
}

void CParser::initOps() {
    m_ops.resize(Eof + 1);

    int nPrec = 0;

    m_ops[Implies]    = operator_t(nPrec, CBinary::Implies);
    m_ops[Iff]        = operator_t(nPrec, CBinary::Iff);
    m_ops[Question]   = operator_t(++ nPrec);
//    m_ops[Colon]      = operator_t(nPrec);
	m_ops[Or]         = operator_t(++ nPrec, CBinary::BoolOr);
	m_ops[Xor]        = operator_t(++ nPrec, CBinary::BoolXor);
	m_ops[Ampersand]  = operator_t(++ nPrec, CBinary::BoolAnd);
	m_ops[Eq]         = operator_t(++ nPrec, CBinary::Equals);
	m_ops[Ne]         = operator_t(nPrec, CBinary::NotEquals);
	m_ops[Lt]         = operator_t(++ nPrec, CBinary::Less);
	m_ops[Lte]        = operator_t(nPrec, CBinary::LessOrEquals);
	m_ops[Gt]         = operator_t(nPrec, CBinary::Greater);
	m_ops[Gte]        = operator_t(nPrec, CBinary::GreaterOrEquals);
	m_ops[In]         = operator_t(++ nPrec, CBinary::In);
	m_ops[ShiftLeft]  = operator_t(++ nPrec, CBinary::ShiftLeft);
	m_ops[ShiftRight] = operator_t(nPrec, CBinary::ShiftRight);
	m_ops[Plus]       = operator_t(++ nPrec, CBinary::Add, CUnary::Plus);
    m_ops[Minus]      = operator_t(nPrec, CBinary::Subtract, CUnary::Minus);
    m_ops[Bang]       = operator_t(++ nPrec, -1, CUnary::BoolNegate);
    m_ops[Tilde]      = operator_t(nPrec, -1, CUnary::BitwiseNegate);
	m_ops[Asterisk]   = operator_t(++ nPrec, CBinary::Multiply);
	m_ops[Slash]      = operator_t(nPrec, CBinary::Divide);
	m_ops[Percent]    = operator_t(nPrec, CBinary::Remainder);
	m_ops[Caret]      = operator_t(++ nPrec, CBinary::Power);
}

CExpression * CParser::parseCastOrTypeReference(CContext & _ctx, CType * _pType) {
    CContext & ctx = * _ctx.createChild(false);
    CExpression * pExpr = NULL;

    if (ctx.in(LeftParen, LeftBracket, LeftMapBracket, LeftBrace, LeftListBracket)) {
        switch (_pType->getKind()) {
            case CType::Seq:
            case CType::Array:
            case CType::Set:
            case CType::Map:
            case CType::List:
            case CType::Optional:
            case CType::Parameterized:
            case CType::NamedReference:
                pExpr = parseAtom(ctx, RestrictTypes);
                if (pExpr)
                    pExpr->setType(_pType);
        }
    }

    if (! pExpr)
        pExpr = _ctx.attach(new CTypeExpr(_pType));

    _ctx.mergeChildren();

    return pExpr;
}

CLambda * CParser::parseLambda(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(true);

    if (! ctx.consume(Predicate))
        UNEXPECTED(ctx, "predicate");

    CLambda * pLambda = _ctx.attach(new CLambda());

    if (! parsePredicateParamsAndBody(ctx, pLambda->getPredicate()))
        return NULL;

    if (! pLambda->getPredicate().getBlock())
        ERROR(ctx, NULL, L"No body defined for anonymous predicate");

    _ctx.mergeChildren();

    return pLambda;
}

CFormula * CParser::parseFormula(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(true);

    if (! ctx.in(Bang, Question, Forall, Exists))
        ERROR(ctx, NULL, L"Quantifier expected");

    CFormula * pFormula = _ctx.attach(new CFormula(ctx.in(Bang, Forall) ?
            CFormula::Universal : CFormula::Existential));

    ++ ctx;

    if (! parseParamList(ctx, pFormula->getBoundVariables(),
            & CParser::parseVariableName, 0))
        ERROR(ctx, NULL, L"Failed parsing bound variables");

    if (! ctx.consume(Dot))
        UNEXPECTED(ctx, ".");

    // OK to use parseExpression instead of parseSubexpression
    // since quantifiers are lowest priority right-associative operators.
    CExpression * pSub = parseExpression(ctx, AllowFormulas);

    if (! pSub)
        ERROR(ctx, NULL, L"Failed parsing subformula");

    pFormula->setSubformula(pSub);
    _ctx.mergeChildren();

    return pFormula;
}

CExpression * CParser::parseAtom(CContext & _ctx, int _nFlags) {
    CContext & ctx = * _ctx.createChild(false);
    ir::CExpression * pExpr = NULL;
    const bool bAllowTypes = ! (_nFlags & RestrictTypes);

    _nFlags &= ~RestrictTypes;

    switch (ctx.getToken()) {
        case Integer:
            pExpr = ctx.attach(new CLiteral(CNumber(ctx.scan())));
            pExpr->setType(new CType(CType::Int));
            pExpr->getType()->setBits(ctx.getIntBits());
            break;
        case Real:
        case Nan:
        case Inf:
            pExpr = ctx.attach(new CLiteral(CNumber(ctx.scan())));
            pExpr->setType(new CType(CType::Real));
            pExpr->getType()->setBits(ctx.getRealBits());
            break;
        case True:
        case False:
            pExpr = ctx.attach(new CLiteral(ctx.getToken() == True));
            pExpr->setType(new CType(CType::Bool));
            ++ ctx;
            break;
        case Char:
            pExpr = ctx.attach(new CLiteral(ctx.scan()[0]));
            pExpr->setType(new CType(CType::Char));
            break;
        case String:
            pExpr = ctx.attach(new CLiteral(ctx.scan()));
            pExpr->setType(new CType(CType::String));
            break;
        case Nil:
            ++ ctx;
            pExpr = ctx.attach(new CLiteral());
            break;
        case LeftParen: {
            CContext * pCtx = ctx.createChild(false);
            ++ (* pCtx);
            pExpr = parseExpression(* pCtx, _nFlags);
            if (! pExpr || ! pCtx->consume(RightParen)) {
                // Try to parse as a struct literal.
                pCtx = ctx.createChild(false);
                CStructConstructor * pStruct = pCtx->attach(new CStructConstructor());
                if (! parseList(* pCtx, * pStruct, & CParser::parseFieldDefinition, LeftParen, RightParen, Comma))
                    ERROR(* pCtx, NULL, L"Expected \")\" or a struct literal");
                pExpr = pStruct;
            }
            ctx.mergeChildren();
            break;
        }
        case LeftBracket:
            pExpr = ctx.attach(new CArrayConstructor());
            if (! parseList(ctx, * (CArrayConstructor *) pExpr, & CParser::parseArrayElement,
                    LeftBracket, RightBracket, Comma))
                ERROR(ctx, NULL, L"Failed parsing array constructor");
            break;
        case LeftMapBracket:
            pExpr = ctx.attach(new CMapConstructor());
            if (! parseList(ctx, * (CMapConstructor *) pExpr, & CParser::parseMapElement,
                    LeftMapBracket, RightMapBracket, Comma))
                ERROR(ctx, NULL, L"Failed parsing map constructor");
            break;
        case LeftBrace:
            pExpr = ctx.attach(new CSetConstructor());
            if (! parseList(ctx, * (CSetConstructor *) pExpr, & CParser::parseExpression,
                    LeftBrace, RightBrace, Comma))
                ERROR(ctx, NULL, L"Failed parsing set constructor");
            break;
        case LeftListBracket:
            pExpr = ctx.attach(new CListConstructor());
            if (! parseList(ctx, * (CListConstructor *) pExpr, & CParser::parseExpression,
                    LeftListBracket, RightListBracket, Comma))
                ERROR(ctx, NULL, L"Failed parsing list constructor");
            break;
        case For:
            pExpr = parseArrayIteration(ctx);
            break;
        case Predicate:
            pExpr = parseLambda(ctx);
            if (pExpr)
                break;
            // No break; try to parse as a predicate type.
        CASE_BUILTIN_TYPE:
            if (bAllowTypes) {
                CType * pType = parseType(ctx);

                if (! pType)
                    ERROR(ctx, NULL, L"Type reference expected");

                pExpr = parseCastOrTypeReference(ctx, pType);
            }
            break;
        case Bang:
        case Question:
        case Forall:
        case Exists:
            if (_nFlags & AllowFormulas)
                pExpr = parseFormula(ctx);
            break;
    }

    if (! pExpr && ctx.is(Identifier)) {
        std::wstring str = ctx.getValue();
        const CNamedValue * pVar = NULL;
        bool bLinkedIdentifier = false;

        if (ctx.nextIs(SingleQuote)) {
            str += L'\'';
            bLinkedIdentifier = true;
        }

        if ((pVar = ctx.getVariable(str))) {
            pExpr = ctx.attach(new CVariableReference(pVar));
            pExpr->setType(pVar->getType(), false);
            WARNING(ctx, L"kind: %d", pExpr->getType()->getKind());
            ctx.skip(bLinkedIdentifier ? 2 : 1);
        }

        if (bLinkedIdentifier && ! pExpr)
            ERROR(ctx, NULL, L"Parameter with name %ls not found", str.c_str());

        const CPredicate * pPred = NULL;

        if (! pExpr && (pPred = ctx.getPredicate(str))) {
            pExpr = ctx.attach(new CPredicateReference(pPred));
            pExpr->setType(pPred->getType(), false);
            ++ ctx;
        }

        CFormulaDeclaration * pFormula = NULL;

        if (! pExpr && (_nFlags & AllowFormulas) && (pFormula = ctx.getFormula(str))) {
            CFormulaCall * pCall = ctx.attach(new CFormulaCall());

            ++ ctx;

            if (ctx.is(LeftParen, RightParen))
                ctx.skip(2);
            else if (! parseActualParameterList(ctx, pCall->getParams()))
                return NULL;

            pCall->setTarget(pFormula);
            pExpr = pCall;
        }

        if (! pExpr && bAllowTypes) {
            CType * pType = NULL;
            if ((pType = parseType(ctx)))
                pExpr = parseCastOrTypeReference(ctx, pType);
        }

        if (! pExpr)
            ERROR(ctx, NULL, L"Unknown identifier: %ls", str.c_str());
    }

    // Other things should be implemented HERE.

    if (pExpr) {
        CExpression * pCompound = NULL;
        while ((pCompound = parseComponent(ctx, * pExpr)))
            pExpr = pCompound;
    }

    if (pExpr && bAllowTypes && ctx.consume(DoubleDot)) {
        // Can be a range.
        CExpression * pMax = parseExpression(ctx, _nFlags);
        if (! pMax)
            return NULL;
        pExpr = ctx.attach(new CTypeExpr(new CRange(pExpr, pMax)));
    }

    if (! pExpr)
        ERROR(ctx, NULL, L"Unexpected token while parsing expression: %ls", TOK_S(ctx));

    _ctx.mergeChildren();
    return pExpr;
}

CExpression * CParser::parseSubexpression(CContext & _ctx, CExpression * _lhs, int _minPrec, int _nFlags)
{
    CContext & ctx = * _ctx.createChild(false);
    bool bParseElse = false;

    while (! _lhs || getPrecedence(ctx.getToken(), bParseElse) >= _minPrec) {
        const int op = ctx.getToken();
        CExpression * rhs = NULL;
        int nPrec = std::max(_minPrec, getPrecedence(op, bParseElse));

        if (_nFlags & AllowFormulas && ! _lhs && ctx.in(Bang, Question)) {
            // Try to parse as a quantified formula first.
            _lhs = parseFormula(ctx);
            if (_lhs)
                break;
        }

        ++ ctx;

        if (getUnaryOp(ctx.getToken()) >= 0) {
            rhs = parseSubexpression(ctx, NULL, nPrec + 1, _nFlags);
        } else {
            rhs = parseAtom(ctx, _nFlags);
        }

        if (! rhs) return NULL;

        while (getPrecedence(ctx.getToken(), bParseElse) > nPrec) {
            CExpression * rhsNew = parseSubexpression(ctx, rhs, getPrecedence(ctx.getToken(), bParseElse), _nFlags);
            if (! rhsNew) return NULL;
            rhs = rhsNew;
        }

        if (bParseElse) {
            if (op != Colon)
                ERROR(ctx, NULL, L"\":\" expected");
            ((CTernary *) _lhs)->setElse(rhs);
            bParseElse = false;
        } else if (op == Question) {
            if (! _lhs) return NULL;
            bParseElse = true;
            _lhs = ctx.attach(new CTernary(_lhs, rhs));
        } else if (! _lhs) {
            const int unaryOp = getUnaryOp(op);
            if (unaryOp < 0)
                ERROR(ctx, NULL, L"Unary operator expected");
            _lhs = ctx.attach(new CUnary(unaryOp, rhs));
            ((CUnary *) _lhs)->overflow().set(_ctx.getOverflow());
        } else {
            const int binaryOp = getBinaryOp(op);
            if (binaryOp < 0)
                ERROR(ctx, NULL, L"Binary operator expected");
            _lhs = ctx.attach(new CBinary(binaryOp, _lhs, rhs));
            ((CBinary *) _lhs)->overflow().set(_ctx.getOverflow());
        }
    }

    _ctx.mergeChildren();

    return _lhs;
}

CExpression * CParser::parseExpression(CContext & _ctx, int _nFlags) {
    CExpression * pExpr = parseAtom(_ctx, _nFlags);

    if (! pExpr) {
        CContext * pCtx = _ctx.getChild();
        _ctx.setChild(NULL);
        pExpr = parseSubexpression(_ctx, pExpr, 0, _nFlags);

        // Restore context if parsing failed.
        if (! pExpr && pCtx && ! pCtx->getMessages().empty())
            _ctx.setChild(pCtx);
        else
            delete pCtx;

        return pExpr;
    }

    return parseSubexpression(_ctx, pExpr, 0, _nFlags);
}

template<class _Pred>
bool CParser::parsePreconditions(CContext & _ctx, _Pred & _pred, branch_map_t & _branches) {
    if (! _ctx.consume(Pre))
        return false;

    branch_map_t::iterator iBranch = _branches.end();

    if (_ctx.in(Label, Identifier, Integer) && _ctx.nextIs(Colon))
        iBranch = _branches.find(_ctx.getValue());

    if (iBranch != _branches.end()) {
        CBranch * pBranch = iBranch->second;
        _ctx.skip(2);
        CExpression * pFormula = parseExpression(_ctx, AllowFormulas);
        if (! pFormula)
            ERROR(_ctx, false, L"Formula expected");
        if (pFormula->getKind() == CExpression::Formula)
            pBranch->setPreCondition((CFormula *) pFormula);
        else
            pBranch->setPreCondition(_ctx.attach(new CFormula(CFormula::None, pFormula)));
    } else {
        CExpression * pFormula = parseExpression(_ctx, AllowFormulas);
        if (! pFormula)
            ERROR(_ctx, false, L"Formula expected");
        if (pFormula->getKind() == CExpression::Formula)
            _pred.setPreCondition((CFormula *) pFormula);
        else
            _pred.setPreCondition(_ctx.attach(new CFormula(CFormula::None, pFormula)));
    }

    while (_ctx.consume(Pre)) {
        CBranch * pBranch = _branches[_ctx.getValue()];

        if (! _ctx.in(Label, Identifier, Integer) || ! _ctx.nextIs(Colon) || ! pBranch)
            ERROR(_ctx, false, L"Branch name expected");

        _ctx.skip(2);
        CExpression * pFormula = parseExpression(_ctx, AllowFormulas);
        if (! pFormula)
            ERROR(_ctx, NULL, L"Formula expected");
        if (pFormula->getKind() != CExpression::Formula)
            pFormula = _ctx.attach(new CFormula(CFormula::None, pFormula));
        pBranch->setPreCondition((CFormula *) pFormula);
    }

    return true;
}

template<class _Pred>
bool CParser::parsePostconditions(CContext & _ctx, _Pred & _pred, branch_map_t & _branches) {
    if (! _ctx.is(Post))
        return false;

    if (_pred.isHyperFunction()) {
        while (_ctx.consume(Post)) {
            CBranch * pBranch = _branches[_ctx.getValue()];

            if (! _ctx.in(Label, Identifier, Integer) || ! _ctx.nextIs(Colon) || ! pBranch)
                ERROR(_ctx, false, L"Branch name expected");

            _ctx.skip(2);
            CExpression * pFormula = parseExpression(_ctx, AllowFormulas);
            if (! pFormula)
                ERROR(_ctx, NULL, L"Formula expected");
            if (pFormula->getKind() != CExpression::Formula)
                pFormula = _ctx.attach(new CFormula(CFormula::None, pFormula));
            pBranch->setPostCondition((CFormula *) pFormula);
        }
    } else if (_ctx.consume(Post)) {
        CExpression * pFormula = parseExpression(_ctx, AllowFormulas);
        if (! pFormula)
            ERROR(_ctx, false, L"Formula expected");
        if (pFormula->getKind() != CExpression::Formula)
            pFormula = _ctx.attach(new CFormula(CFormula::None, pFormula));
        _pred.setPostCondition((CFormula *) pFormula);
    }

    return true;
}

bool CParser::fixupAsteriskedParameters(CContext & _ctx, CParams & _in, CParams & _out) {
    bool bResult = false;

    for (size_t i = 0; i < _in.size(); ++ i) {
        CParam * pInParam = _in.get(i);
        if (pInParam->getLinkedParam() != pInParam)
            continue;

        const std::wstring name = pInParam->getName() + L'\'';
        CParam * pOutParam = new CParam(name);

        _out.add(pOutParam);
        pInParam->setLinkedParam(pOutParam);
        pOutParam->setLinkedParam(pInParam);
        pOutParam->setType(pInParam->getType(), false);
        pOutParam->setOutput(true);
        bResult = true;
        _ctx.addVariable(pOutParam);
    }

    return bResult;
}

bool CParser::parsePredicateParamsAndBody(CContext & _ctx, CAnonymousPredicate & _pred) {
    if (! _ctx.consume(LeftParen))
        ERROR(_ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(_ctx));

    if (! parseParamList(_ctx, _pred.getInParams(), & CParser::parseParam, AllowAstersk | AllowEmptyNames))
        ERROR(_ctx, false, L"Failed to parse input parameters");

    branch_map_t branches;

    bool bHasAsterisked = false;

    while (_ctx.consume(Colon)) {
        CBranch * pBranch = new CBranch();

        _pred.getOutParams().add(pBranch);

        if (_pred.getOutParams().size() == 1)
            bHasAsterisked = fixupAsteriskedParameters(_ctx, _pred.getInParams(), * pBranch);
        else if (bHasAsterisked)
            ERROR(_ctx, false, L"Hyperfunctions cannot use '*' in parameter list");

        parseParamList(_ctx, * pBranch, & CParser::parseParam, OutputParams | AllowEmptyNames);
        for (size_t i = 0; i < pBranch->size(); ++ i)
            pBranch->get(i)->setOutput(true);

        if (_ctx.is(Hash) && _ctx.nextIn(Identifier, Label, Integer)) {
            ++ _ctx;
            std::wstring strLabel = _ctx.getValue();

            if (_ctx.is(Integer) && wcstoul(strLabel.c_str(), NULL, 10) != _pred.getOutParams().size())
                ERROR(_ctx, false, L"Numbers of numeric branch labels should correspond to branch order");

            ++ _ctx;

            if (! branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(_ctx, false, L"Duplicate branch name \"%ls\"", strLabel.c_str());

            pBranch->setLabel(new CLabel(strLabel));
            _ctx.addLabel(pBranch->getLabel());
        }
    }

    if (_pred.getOutParams().empty()) {
        CBranch * pBranch = new CBranch();
        _pred.getOutParams().add(pBranch);
        fixupAsteriskedParameters(_ctx, _pred.getInParams(), * pBranch);
    }

    // Create labels for unlabeled branches.
    if (_pred.getOutParams().size() > 1) {
        for (size_t i = 0; i < _pred.getOutParams().size(); ++ i) {
            CBranch * pBranch = _pred.getOutParams().get(i);
            if (! pBranch->getLabel()) {
                pBranch->setLabel(new CLabel(fmtInt(i + 1)));
                _ctx.addLabel(pBranch->getLabel());
                branches[pBranch->getLabel()->getName()] = pBranch;
            }
        }
    }

    if (! _ctx.consume(RightParen))
        ERROR(_ctx, false, L"Expected \")\", got: %ls", TOK_S(_ctx));

    if (_ctx.is(Pre))
        if (! parsePreconditions(_ctx, _pred, branches))
            ERROR(_ctx, false, L"Failed parsing preconditions");

    if (_ctx.is(LeftBrace)) {
        CBlock * pBlock = parseBlock(_ctx);

        if (! pBlock)
            ERROR(_ctx, false, L"Failed parsing predicate body");

        _pred.setBlock(pBlock);
    }

    if (_ctx.is(Post))
        if (! parsePostconditions(_ctx, _pred, branches))
            ERROR(_ctx, false, L"Failed parsing postconditions");

    return true;
}

CPredicate * CParser::parsePredicate(CContext & _ctx) {
    CContext * pCtx = _ctx.createChild(false);

    if (! pCtx->is(Identifier))
        return NULL;

    CPredicate * pPred = pCtx->attach(new CPredicate(pCtx->scan()));

    pCtx->addPredicate(pPred);
    pCtx = pCtx->createChild(true);

    if (! parsePredicateParamsAndBody(* pCtx, * pPred))
        return NULL;

    if (! pPred->getBlock() && ! pCtx->consume(Semicolon))
        ERROR(* pCtx, NULL, L"Expected block or a semicolon");

    _ctx.mergeChildren();

    return pPred;
}

CVariableDeclaration * CParser::parseVariableDeclaration(CContext & _ctx, bool _bLocal) {
    CContext & ctx = * _ctx.createChild(false);
    const bool bMutable = ctx.consume(Mutable);

    CType * pType = parseType(ctx);

    if (! pType)
        return NULL;

    if (! ctx.is(Identifier))
        ERROR(ctx, NULL, L"Expected identifier, got: %ls", TOK_S(ctx));

    CVariableDeclaration * pDecl = ctx.attach(new CVariableDeclaration(_bLocal, ctx.scan()));
    pDecl->getVariable().setType(pType);
    pDecl->getVariable().setMutable(bMutable);

    if (ctx.consume(Eq)) {
        CExpression * pExpr = parseExpression(ctx);

        if (! pExpr) return NULL;
        pDecl->setValue(pExpr);
    }

    _ctx.mergeChildren();
    _ctx.addVariable(& pDecl->getVariable());

    return pDecl;
}

bool CParser::parseModuleHeader(CContext & _ctx, CModule * _pModule) {
    if (_ctx.is(Module, Identifier, Semicolon)) {
        _pModule->setName(_ctx.scan(3, 1));
        return true;
    }

    return false;
}

bool CParser::parseImport(CContext & _ctx, CModule * _pModule) {
    if (_ctx.is(Import, Identifier, Semicolon)) {
        _pModule->getImports().push_back(_ctx.scan(3, 1));
        return true;
    }

    return false;
}

CType * CParser::parseDerivedTypeParameter(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CType * pType = NULL;

    if (! ctx.consume(LeftParen))
        ERROR(ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(ctx));
    if (! (pType = parseType(ctx)))
        return NULL;
    if (! ctx.consume(RightParen))
        ERROR(ctx, NULL, L"Expected \")\", got: %ls", TOK_S(ctx));

    _ctx.mergeChildren();
    return pType;
}

CArrayType * CParser::parseArrayType(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CNamedValue * parseNamedValue(CContext & _ctx);

    if (! ctx.consume(Array))
        ERROR(ctx, NULL, L"Expected \"array\", got: %ls", TOK_S(ctx));

    if (! ctx.consume(LeftParen))
        ERROR(ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(ctx));

    CType * pType = parseType(ctx);

    if (! pType)
        return NULL;

    CArrayType * pArray = ctx.attach(new CArrayType(pType));

    if (! parseList(ctx, pArray->getDimensions(), & CParser::parseRange, Comma, RightParen, Comma))
        return NULL;

    _ctx.mergeChildren();
    return pArray;
}

CMapType * CParser::parseMapType(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Map))
        UNEXPECTED(ctx, "map");

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    CType * pIndexType = parseType(ctx);

    if (! pIndexType)
        return NULL;

    if (! ctx.consume(Comma))
        UNEXPECTED(ctx, ",");

    CType * pBaseType = parseType(ctx);

    if (! pBaseType)
        return NULL;

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    CMapType * pType = ctx.attach(new CMapType(pIndexType, pBaseType));

    _ctx.mergeChildren();

    return pType;
}

bool CParser::isTypeName(CContext & _ctx, const std::wstring & _name) const {
    const CTypeDeclaration * pDecl = _ctx.getType(_name);

    if (pDecl)
        return true;

    const CNamedValue * pVar = _ctx.getVariable(_name);

    return pVar ? isTypeVariable(pVar) : false;
}

CNamedReferenceType * CParser::parseTypeReference(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.is(Identifier))
        UNEXPECTED(ctx, "identifier");

    CNamedReferenceType * pType = NULL;

    const std::wstring & str = ctx.scan();
    const CTypeDeclaration * pDecl = ctx.getType(str);

    DEBUG(L"%ls %d", str.c_str(), (pDecl != NULL));

    if (! pDecl) {
        const CNamedValue * pVar = ctx.getVariable(str);

        if (isTypeVariable(pVar))
            pType = ctx.attach(new CNamedReferenceType(pVar));
        else
            ERROR(ctx, NULL, L"Unknown type identifier: %ls", str.c_str());
    } else
        pType = ctx.attach(new CNamedReferenceType(pDecl));

    if (ctx.is(LeftParen)) {
        if (! parseActualParameterList(ctx, ((CNamedReferenceType *) pType)->getParams()))
            ERROR(ctx, NULL, L"Garbage in parameter list");
    }

    _ctx.mergeChildren();
    return pType;
}

CRange * CParser::parseRange(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CExpression * pMin = parseSubexpression(ctx, parseAtom(ctx, RestrictTypes), 0);

    if (! pMin) return NULL;
    if (! ctx.consume(DoubleDot))
        UNEXPECTED(ctx, "..");

    CExpression * pMax = parseExpression(ctx);

    if (! pMax) return NULL;

    _ctx.mergeChildren();

    return _ctx.attach(new CRange(pMin, pMax));
}

CStructType * CParser::parseStructType(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Struct))
        UNEXPECTED(ctx, "struct");

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    CStructType * pType = ctx.attach(new CStructType());

    if (! parseParamList(ctx, pType->getFields(), & CParser::parseVariableName, AllowEmptyNames))
        return NULL;

    DEBUG(L"%d", pType->getFields().size());

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    _ctx.mergeChildren();

    return pType;
}

CUnionType * CParser::parseUnionType(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Union))
        UNEXPECTED(ctx, "union");

    CUnionType * pType = ctx.attach(new CUnionType());

    if (! parseList(ctx, pType->getConstructors(), & CParser::parseConstructorDefiniton,
            LeftParen, RightParen, Comma))
        return NULL;

    _ctx.mergeChildren();

    return pType;
}

CEnumType * CParser::parseEnumType(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Enum))
        UNEXPECTED(ctx, "enum");

    CEnumType * pType = ctx.attach(new CEnumType());

    if (! parseList(ctx, pType->getValues(), & CParser::parseEnumValue,
            LeftParen, RightParen, Comma))
        return NULL;

    for (size_t i = 0; i < pType->getValues().size(); ++ i) {
        pType->getValues().get(i)->setType(pType, false);
        pType->getValues().get(i)->setOrdinal(i);
    }

    _ctx.mergeChildren();

    return pType;
}

CSubtype * CParser::parseSubtype(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Subtype))
        UNEXPECTED(ctx, "subtype");

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    CNamedValue * pVar = parseNamedValue(ctx);

    if (! pVar)
        return NULL;

    if (! ctx.consume(Colon))
        UNEXPECTED(ctx, ":");

    CExpression * pExpr = parseExpression(ctx);

    if (! pExpr)
        return NULL;

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    CSubtype * pType = ctx.attach(new CSubtype(pVar, pExpr));

    _ctx.mergeChildren();

    return pType;
}

CPredicateType * CParser::parsePredicateType(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Predicate))
        UNEXPECTED(ctx, "predicate");

    CPredicateType * pType = ctx.attach(new CPredicateType());

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    if (! parseParamList(ctx, pType->getInParams(), & CParser::parseParam, AllowEmptyNames))
        ERROR(ctx, false, L"Failed to parse input parameters");

    branch_map_t branches;

    while (ctx.consume(Colon)) {
        CBranch * pBranch = new CBranch();

        pType->getOutParams().add(pBranch);
        parseParamList(ctx, * pBranch, & CParser::parseParam, OutputParams | AllowEmptyNames);

        for (size_t i = 0; i < pBranch->size(); ++ i)
            pBranch->get(i)->setOutput(true);

        if (ctx.is(Hash) && ctx.nextIn(Identifier, Label, Integer)) {
            std::wstring strLabel = ctx.scan(2, 1);
            if (! branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(ctx, false, L"Duplicate branch name \"%ls\"", strLabel.c_str());
            pBranch->setLabel(new CLabel(strLabel));
        }
    }

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    if (ctx.is(Pre))
        if (! parsePreconditions(ctx, * pType, branches))
            ERROR(ctx, false, L"Failed parsing preconditions");

    if (ctx.is(Post))
        if (! parsePostconditions(ctx, * pType, branches))
            ERROR(ctx, false, L"Failed parsing postconditions");

    _ctx.mergeChildren();

    return pType;
}

CType * CParser::parseType(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CType * pType = NULL;

    bool bBuiltinType = true;

    switch (ctx.getToken()) {
        case NatType:
            pType = ctx.attach(new CType(CType::Nat));
            pType->setBits(ctx.getIntBits());
            ++ ctx;
            break;
        case IntType:
            pType = ctx.attach(new CType(CType::Int));
            pType->setBits(ctx.getIntBits());
            ++ ctx;
            break;
        case RealType:
            pType = ctx.attach(new CType(CType::Real));
            pType->setBits(ctx.getRealBits());
            ++ ctx;
            break;
        case BoolType:   pType = ctx.attach(new CType(CType::Bool)); ++ ctx; break;
        case CharType:   pType = ctx.attach(new CType(CType::Char)); ++ ctx; break;
        case StringType: pType = ctx.attach(new CType(CType::String)); ++ ctx; break;
        case Type:       pType = ctx.attach(new CType(CType::Type)); ++ ctx; break;
        case Var:        pType = ctx.attach(new CType(CType::Generic)); ++ ctx; break;

        case Struct:
            pType = parseStructType(ctx);
            break;
        case Union:
            pType = parseUnionType(ctx);
            break;
        case Enum:
            pType = parseEnumType(ctx);
            break;
        case Subtype:
            pType = parseSubtype(ctx);
            break;
        case Seq:
            if (! (pType = parseDerivedTypeParameter(++ ctx)))
                return NULL;
            pType = ctx.attach(new CSeqType(pType));
            break;
        case Set:
            if (! (pType = parseDerivedTypeParameter(++ ctx)))
                return NULL;
            pType = ctx.attach(new CSetType(pType));
            break;
        case List:
            if (! (pType = parseDerivedTypeParameter(++ ctx)))
                return NULL;
            pType = ctx.attach(new CListType(pType));
            break;
        case Array:
            pType = parseArrayType(ctx);
            break;
        case Map:
            pType = parseMapType(ctx);
            break;
        case Predicate:
            pType = parsePredicateType(ctx);
            break;
        default:
            bBuiltinType = false;
    }

    const bool bNumeric = pType && (pType->getKind() == CType::Nat ||
            pType->getKind() == CType::Int || pType->getKind() == CType::Real);

    if (bNumeric && ctx.is(LeftParen, Integer, RightParen))
        pType->setBits(wcstol(ctx.scan(3, 1).c_str(), NULL, 10));

    if (bBuiltinType && ! pType)
        ERROR(ctx, NULL, L"Error parsing type reference");

    if (! pType && ctx.is(Identifier))
        pType = parseTypeReference(ctx);

    if (! pType)
        pType = parseRange(ctx);

    if (! pType)
        ERROR(ctx, NULL, L"Unexpected token while parsing type reference: %ls", TOK_S(ctx));

    if (ctx.consume(Asterisk))
        pType = ctx.attach(new COptionalType(pType));

    _ctx.mergeChildren();

    return pType;
}

template<class _Param>
bool CParser::parseParamList(CContext & _ctx, CCollection<_Param> & _params,
        PARSER_FN(_Param,_parser,int), int _nFlags)
{
    if (_ctx.in(RightParen, Colon, Dot))
        return true;

    CContext & ctx = * _ctx.createChild(false);
    CType * pType = NULL;
    CCollection<_Param> params;

    do {
        DEBUG(ctx.getValue().c_str());

        const bool bNeedType = ! pType
            || ! ctx.is(Identifier)
            || ! ctx.nextIn(Comma, RightParen, Colon, Dot)
            || ((_nFlags & AllowEmptyNames) && isTypeName(ctx, ctx.getValue()));

        if (bNeedType) {
            pType = parseType(ctx);
            if (! pType)
                ERROR(ctx, false, L"Type required");
        }

        _Param * pParam = NULL;

        if (! ctx.is(Identifier)) {
            if (! (_nFlags & AllowEmptyNames))
                ERROR(ctx, false, L"Identifier required");
            pParam = ctx.attach(new _Param());
        } else
            pParam = (this->*_parser) (ctx, _nFlags);

        if (! pParam)
            ERROR(ctx, false, L"Variable or parameter definition required");

        pParam->setType(pType, ! pType->getParent());
        if (pParam->getType())
            DEBUG(L"Kind: %d", pParam->getType()->getKind());
        params.add(pParam, false);
    } while (ctx.consume(Comma));

    _params.append(params);
    _ctx.mergeChildren();

    return true;
}

CParam * CParser::parseParam(CContext & _ctx, int _nFlags) {
    if (! _ctx.is(Identifier))
        return NULL;

    CContext & ctx = * _ctx.createChild(false);

    std::wstring name = ctx.scan();

    CParam * pParam = NULL;

    if (ctx.consume(SingleQuote)) {
        if (! (_nFlags & OutputParams))
            ERROR(ctx, NULL, L"Only output parameters can be declared as joined");

        CNamedValue * pVar = ctx.getVariable(name, true);

        if (! pVar)
            ERROR(ctx, NULL, L"Parameter '%ls' is not defined.", name.c_str());

        if (pVar->getKind() != CNamedValue::PredicateParameter || ((CParam *) pVar)->isOutput())
            ERROR(ctx, NULL, L"Identifier '%ls' does not name a predicate input parameter.", name.c_str());

        name += L'\'';
        pParam = ctx.attach(new CParam(name));
        pParam->setLinkedParam((CParam *) pVar);
        ((CParam *) pVar)->setLinkedParam(pParam);
    } else
        pParam = ctx.attach(new CParam(name));

    if (ctx.consume(Asterisk)) {
        if (! (_nFlags & AllowAstersk))
            ERROR(ctx, NULL, L"Only input predicate parameters can automatically declare joined output parameters");
        pParam->setLinkedParam(pParam); // Just a mark, should be processed later.
    }

    pParam->setOutput(_nFlags & OutputParams);

    DEBUG(L"Adding var %ls", name.c_str());

    ctx.addVariable(pParam);
    _ctx.mergeChildren();

    return pParam;
}

CNamedValue * CParser::parseNamedValue(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    CType * pType = parseType(ctx);

    if (! pType)
        ERROR(ctx, false, L"Type required");

    if (! ctx.is(Identifier))
        ERROR(ctx, false, L"Identifier required");

    CNamedValue * pVar = ctx.attach(new CNamedValue(ctx.scan(), pType));

    _ctx.mergeChildren();
    _ctx.addVariable(pVar);

    return pVar;
}

CElementDefinition * CParser::parseArrayElement(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CElementDefinition * pElem = ctx.attach(new CElementDefinition());
    CExpression * pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected.");

    if (ctx.consume(Colon)) {
        pElem->setIndex(pExpr);
        pExpr = parseExpression(ctx);
    }

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected.");

    pElem->setValue(pExpr);
    _ctx.mergeChildren();

    return pElem;
}

CElementDefinition * CParser::parseMapElement(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CElementDefinition * pElem = ctx.attach(new CElementDefinition());
    CExpression * pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Index expression expected.");

    if (! ctx.consume(Colon))
        UNEXPECTED(ctx, ":");

    pElem->setIndex(pExpr);
    pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Value expression expected.");

    pElem->setValue(pExpr);
    _ctx.mergeChildren();

    return pElem;
}

CStructFieldDefinition * CParser::parseFieldDefinition(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CStructFieldDefinition * pField = ctx.attach(new CStructFieldDefinition());

    if (ctx.is(Identifier))
        pField->setName(ctx.getValue());

    CExpression * pExpr = parseExpression(ctx);

    if (! pExpr) {
        if (pField->getName().empty())
            ERROR(ctx, NULL, L"Field name expected.");
        ++ ctx;
    }

    if (ctx.consume(Colon)) {
        if (pField->getName().empty())
            ERROR(ctx, NULL, L"Field name expected.");
        pExpr = parseExpression(ctx);
    }

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected.");

    pField->setValue(pExpr);
    _ctx.mergeChildren();

    return pField;
}

CUnionConstructorDefinition * CParser::parseConstructorDefinition(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.is(Identifier))
        ERROR(ctx, NULL, L"Constructor name expected.");

    CUnionConstructorDefinition * pCons = ctx.attach(new CUnionConstructorDefinition(ctx.scan()));

    if (! ctx.consume(LeftParen)) {
        // No fields.
        _ctx.mergeChildren();
        return pCons;
    }

    if (! parseParamList(ctx, pType->getFields(), & CParser::parseVariableName))
        return NULL;

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    _ctx.mergeChildren();

    return pType;
}


CEnumValue * CParser::parseEnumValue(CContext & _ctx) {
    if (! _ctx.is(Identifier))
        return NULL;

    CEnumValue * pVar = _ctx.attach(new CEnumValue(_ctx.scan()));
    _ctx.addVariable(pVar);

    return pVar;
}

CNamedValue * CParser::parseVariableName(CContext & _ctx, int) {
    if (! _ctx.is(Identifier))
        return NULL;

    CNamedValue * pVar = _ctx.attach(new CNamedValue(_ctx.scan()));
    _ctx.addVariable(pVar);

    return pVar;
}

CTypeDeclaration * CParser::parseTypeDeclaration(CContext & _ctx) {
    if (! _ctx.is(Type, Identifier))
        return NULL;

    CContext * pCtx = _ctx.createChild(false);
    CTypeDeclaration * pDecl = pCtx->attach(new CTypeDeclaration(pCtx->scan(2, 1)));
    CParameterizedType * pParamType = NULL;

    if (pCtx->consume(LeftParen)) {
        pCtx = pCtx->createChild(true);
        pParamType = new CParameterizedType();
        pDecl->setType(pParamType);
        if (! parseParamList(* pCtx, pParamType->getParams(), & CParser::parseVariableName))
            return NULL;
        if (! pCtx->consume(RightParen))
            ERROR(* pCtx, NULL, L"Expected \")\", got: %ls", TOK_S(* pCtx));
    }

    _ctx.addType(pDecl); // So that recursice definitions would work.

    if (pCtx->consume(Eq)) {
        CType * pType = parseType(* pCtx);

        if (! pType)
            return NULL;

        if (pParamType)
            pParamType->setActualType(pType);
        else
            pDecl->setType(pType);
    }

    _ctx.mergeChildren();
//    _ctx.addType(pDecl);

    return pDecl;
}

CBlock * CParser::parseBlock(CContext & _ctx) {
    CContext * pCtx = _ctx.createChild(false);

    if (! pCtx->consume(LeftBrace))
        return NULL;

    CBlock * pBlock = pCtx->attach(new CBlock());
    pCtx = pCtx->createChild(true);

    while (! pCtx->is(RightBrace)) {
        bool bNeedSeparator = false;

        if (pCtx->is(Pragma)) {
            CContext * pCtxNew = parsePragma(* pCtx);
            if (! pCtxNew)
                ERROR(* pCtx, NULL, L"Failed parsing compiler directive");

            if (pCtxNew->is(LeftBrace)) {
                CBlock * pNewBlock = parseBlock(* pCtxNew);
                if (! pNewBlock)
                    ERROR(* pCtxNew, NULL, L"Failed parsing block");
                pBlock->add(pNewBlock);
                pCtx->mergeChildren();
            } else {
                pCtx = pCtxNew;
                bNeedSeparator = true;
            }
        } else {
            CStatement * pStmt = parseStatement(* pCtx);

            if (! pStmt)
                ERROR(* pCtx, NULL, L"Error parsing statement");

            if (pCtx->is(DoublePipe)) {
                CParallelBlock * pNewBlock = pCtx->attach(new CParallelBlock());
                pNewBlock->add(pStmt);
                while (pCtx->consume(DoublePipe)) {
                    CStatement * pStmt = parseStatement(* pCtx);
                    if (! pStmt)
                        ERROR(* pCtx, NULL, L"Error parsing parallel statement");
                    pNewBlock->add(pStmt);
                }
                pStmt = pNewBlock;
            }

            pBlock->add(pStmt);
            bNeedSeparator = ! pStmt->isBlockLike() && ! pCtx->in(LeftBrace, RightBrace);
        }

        if (bNeedSeparator && ! pCtx->consume(Semicolon))
            ERROR(* pCtx, NULL, L"Expected \";\", got: %ls", TOK_S(* pCtx));
    }

    ++ (* pCtx);
    _ctx.mergeChildren();

    return pBlock;
}

CMultiassignment * CParser::parseMultiAssignment(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CMultiassignment * pMA = ctx.attach(new CMultiassignment());

    if (! parseList(ctx, pMA->getLValues(), & CParser::parseAtom, Pipe, Pipe, Comma))
        ERROR(ctx, NULL, L"Error parsing list of l-values");

    if (! ctx.consume(Eq))
        UNEXPECTED(ctx, "=");

    if (ctx.is(Pipe)) {
        if (! parseList(ctx, pMA->getExpressions(), & CParser::parseExpression, Pipe, Pipe, Comma))
            ERROR(ctx, NULL, L"Error parsing list of expression");
    } else {
        CExpression * pExpr = parseExpression(ctx);
        if (! pExpr)
            ERROR(ctx, NULL, L"Expression expected");
        pMA->getExpressions().add(pExpr);
    }

    _ctx.mergeChildren();

    return pMA;
}

CSwitch * CParser::parseSwitch(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(true);

    if (! ctx.consume(Switch))
        UNEXPECTED(ctx, "switch");

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    CVariableDeclaration * pDecl = parseVariableDeclaration(ctx, true);
    CExpression * pExpr = NULL;

    if (! pDecl) {
        pExpr = parseExpression(ctx);
        if (! pExpr)
            ERROR(ctx, NULL, L"Expression or variable declaration expected");
    }

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    CSwitch * pSwitch = _ctx.attach(new CSwitch());

    if (pDecl) {
        pSwitch->setParamDecl(pDecl);
        pExpr = new CVariableReference(& pDecl->getVariable());
        pExpr->setType((CType *) resolveBaseType(pExpr->getType()));
        pSwitch->setParam(pExpr, true);
    } else
        pSwitch->setParam(pExpr);

    if (! ctx.consume(LeftBrace))
        UNEXPECTED(ctx, "{");

    const CType * pBaseType = resolveBaseType(pExpr->getType());
    typedef std::map<std::wstring, size_t> map_union_alts_t;
    map_union_alts_t mapUnionAlts;
    bool bUnion = false;

    if (pBaseType && pBaseType->getKind() == CType::Union) {
        CUnionType * pUnion = (CUnionType *) pBaseType;
        for (size_t i = 0; i < pUnion->getAlternatives().size(); ++ i)
            mapUnionAlts[pUnion->getAlternatives().get(i)->getName()] = i;
        bUnion = true;
    }

    while (ctx.in(Case)) {
        CSwitchCase * pCase = ctx.attach(new CSwitchCase());
        //CType * pParamType = NULL;

        if (bUnion) {
            ctx.consume(Case);

            if (! ctx.is(Identifier))
                ERROR(ctx, NULL, L"Expected identifier, got: %ls", TOK_S(ctx));

            while (ctx.in(Identifier)) {
                const std::wstring & strAlt = ctx.scan();
                map_union_alts_t::iterator iAlt = mapUnionAlts.find(strAlt);

                if (iAlt == mapUnionAlts.end())
                    ERROR(ctx, NULL, L"Unknown alternative identifier: %ls", strAlt.c_str());

                CUnionAlternativeExpr * pRef =
                    new CUnionAlternativeExpr((CUnionType *) pBaseType, iAlt->second);

                pCase->getExpressions().add(pRef, true);

                //if (! pParamType)
                //    pParamType = ((CUnionType *) pBaseType)->getAlternatives().get(iAlt->second)->getType();

                if (! ctx.consume(Comma))
                    break;
            }

            if (! ctx.consume(Colon))
                UNEXPECTED(ctx, ":");
        } else {
            if (! parseList(ctx, pCase->getExpressions(),
                    & CParser::parseExpression, Case, Colon, Comma))
                ERROR(ctx, NULL, L"Error parsing list of expressions");
        }

//        CContext & ctxBody = * ctx.createChild(true);
        CStatement * pStmt = parseStatement(ctx);

        if (! pStmt)
            ERROR(ctx, NULL, L"Statement required");

        pCase->setBody(pStmt);
        pSwitch->add(pCase);
//        ctx.mergeChildren();
    }

    if (ctx.is(Default, Colon)) {
        ctx.skip(2);
        CStatement * pStmt = parseStatement(ctx);

        if (! pStmt)
            ERROR(ctx, NULL, L"Statement required");

        pSwitch->setDefault(pStmt);
    }

    if (! ctx.consume(RightBrace))
        UNEXPECTED(ctx, "}");

    _ctx.mergeChildren();

    return pSwitch;
}

CIf * CParser::parseConditional(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(If))
        UNEXPECTED(ctx, "if");

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    CExpression * pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected");

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    CStatement * pStmt = parseStatement(ctx);

    if (! pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    CIf * pIf = ctx.attach(new CIf());

    pIf->setParam(pExpr);
    pIf->setBody(pStmt);

    if (ctx.consume(Else)) {
        pStmt = parseStatement(ctx);

        if (! pStmt)
            ERROR(ctx, NULL, L"Statement expected");

        pIf->setElse(pStmt);
    }

    _ctx.mergeChildren();

    return pIf;
}

CJump * CParser::parseJump(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Hash))
        UNEXPECTED(ctx, "#");

    if (! ctx.in(Label, Identifier, Integer))
        ERROR(ctx, NULL, L"Label identifier expected");

    std::wstring name = ctx.scan();
    CLabel * pLabel = ctx.getLabel(name);

    if (! pLabel)
        ERROR(ctx, NULL, L"Unknown label %ls", name.c_str());

    _ctx.mergeChildren();

    return _ctx.attach(new CJump(pLabel));
}

//CStatement * CParser::parsePragma(CContext & _ctx) {
//    return NULL;
//}

CReceive * CParser::parseReceive(CContext & _ctx) {
    return NULL;
}

CSend * CParser::parseSend(CContext & _ctx) {
    return NULL;
}

CWith * CParser::parseWith(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(With))
        UNEXPECTED(ctx, "with");

    CWith * pWith = ctx.attach(new CWith());

    if (! parseList(ctx, pWith->getParams(),
            & CParser::parseExpression, LeftParen, RightParen, Comma))
        ERROR(ctx, NULL, L"Error parsing list of expressions");

    CStatement * pStmt = parseStatement(ctx);

    if (! pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    pWith->setBody(pStmt);

    _ctx.mergeChildren();

    return pWith;
}

CFor * CParser::parseFor(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(true);

    if (! ctx.consume(For))
        UNEXPECTED(ctx, "for");

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    CFor * pFor = _ctx.attach(new CFor());

    if (! ctx.in(Semicolon)) {
        CVariableDeclaration * pDecl = parseVariableDeclaration(ctx, true);
        if (! pDecl)
            ERROR(ctx, NULL, L"Variable declaration expected");
        pFor->setIterator(pDecl);
    }

    if (! ctx.consume(Semicolon))
        UNEXPECTED(ctx, ";");

    if (! ctx.in(Semicolon)) {
        CExpression * pExpr = parseExpression(ctx);
        if (! pExpr)
            ERROR(ctx, NULL, L"Expression expected");
        pFor->setInvariant(pExpr);
    }

    if (! ctx.consume(Semicolon))
        UNEXPECTED(ctx, ";");

    if (! ctx.is(RightParen)) {
        CStatement * pStmt = parseStatement(ctx);
        if (! pStmt)
            ERROR(ctx, NULL, L"Statement expected");
        pFor->setIncrement(pStmt);
    }

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    CStatement * pStmt = parseStatement(ctx);

    if (! pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    pFor->setBody(pStmt);

    _ctx.mergeChildren();

    return pFor;
}

CWhile * CParser::parseWhile(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(While))
        UNEXPECTED(ctx, "while");

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    CWhile * pWhile = ctx.attach(new CWhile());
    CExpression * pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected");

    pWhile->setInvariant(pExpr);

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    CStatement * pStmt = parseStatement(ctx);

    if (! pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    pWhile->setBody(pStmt);

    _ctx.mergeChildren();

    return pWhile;
}

CBreak * CParser::parseBreak(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Break))
        UNEXPECTED(ctx, "break");

    _ctx.mergeChildren();

    return _ctx.attach(new CBreak());
}

CStatement * CParser::parseAssignment(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    CExpression * pLHS = parseAtom(ctx);
    CStatement * pStmt = NULL;

    if (! pLHS)
        ERROR(ctx, NULL, L"Error parsing expression");

    if (ctx.consume(Eq)) {
        CExpression * pRHS = parseExpression(ctx);

        if (! pRHS)
            ERROR(ctx, NULL, L"Error parsing expression");

        pStmt = ctx.attach(new CAssignment(pLHS, pRHS));

        // TODO: RHS's type cannot be derived here actually. Tmp solution until
        // proper type inference gets implemented.
        if (pLHS->getType() && ! pRHS->getType())
            pRHS->setType((CType *) resolveBaseType(pLHS->getType()), false);
    } else if (ctx.consume(Comma)) {
        CMultiassignment * pMA = ctx.attach(new CMultiassignment());

        pMA->getLValues().add(pLHS);

        if (! parseList(ctx, pMA->getLValues(), & CParser::parseAtom, -1, Eq, Comma))
            ERROR(ctx, NULL, L"Error parsing list of l-values");

        if (! parseList(ctx, pMA->getExpressions(), & CParser::parseExpression, -1, -1, Comma))
            ERROR(ctx, NULL, L"Error parsing list of expression");

        pStmt = pMA;
    } else
        ERROR(ctx, NULL, L"Expected \"=\" or \",\", got: %ls", TOK_S(ctx));

    _ctx.mergeChildren();

    return pStmt;
}

CExpression * CParser::parseCallResult(CContext & _ctx, CVariableDeclaration * & _pDecl) {
    CContext * pCtx = _ctx.createChild(false);
    CExpression * pExpr = NULL;

    // Try variable declaration.
    CType * pType = parseType(* pCtx);

    if (pType && pCtx->is(Identifier)) {
        // TODO Need to check (was: don't attach to context, caller is responsible for management of this object.)
        _pDecl = pCtx->attach(new CVariableDeclaration(true, pCtx->scan()));
        _pDecl->getVariable().setType(pType);
        _pDecl->getVariable().setMutable(false);
        pExpr = pCtx->attach(new CVariableReference(& _pDecl->getVariable()));
        _ctx.addVariable(& _pDecl->getVariable());
    }

    if (! pExpr) {
        _pDecl = NULL;
        pCtx = _ctx.createChild(false);
        pExpr = parseExpression(* pCtx);
        if (! pExpr)
            ERROR(* pCtx, NULL, L"Error parsing output parameter");
    }

    _ctx.mergeChildren();

    return pExpr;
}

bool CParser::parseCallResults(CContext & _ctx, CCall & _call, CCollection<CExpression> & _list) {
    CContext & ctx = * _ctx.createChild(false);
    CExpression * pExpr = NULL;
    CVariableDeclaration * pDecl = NULL;

    if (ctx.consume(Underscore))
        pExpr = NULL;
    else if (! (pExpr = parseCallResult(ctx, pDecl)))
        ERROR(ctx, false, L"Error parsing output parameter");

    // It's okay to modify call object since if this function fails parseCall() fails too.
    _list.add(pExpr);
    if (pDecl)
        _call.getDeclarations().add(pDecl);

    while (ctx.consume(Comma)) {
        if (ctx.consume(Underscore))
            pExpr = NULL;
        else if (! (pExpr = parseCallResult(ctx, pDecl)))
            ERROR(ctx, false, L"Error parsing output parameter");

        _list.add(pExpr);
        if (pDecl)
            _call.getDeclarations().add(pDecl);
    }

    _ctx.mergeChildren();

    return true;
}

CCall * CParser::parseCall(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);
    std::wstring name = ctx.getValue();
    CPredicate * pPred = ctx.getPredicate(name);
    CExpression * pExpr = NULL;

    DEBUG(name.c_str());

    if (pPred) {
        ++ ctx;
        pExpr = ctx.attach(new CPredicateReference(pPred));
        pExpr->setType(pPred->getType(), false);
    } else
        pExpr = parseAtom(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Predicate expression expected");

    CCall * pCall = ctx.attach(new CCall());

    pCall->setPredicate(pExpr);

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    if (! parseList(ctx, pCall->getParams(), & CParser::parseExpression, -1, -1, Comma))
        ERROR(ctx, false, L"Failed to parse input parameters");

    typedef std::multimap<std::wstring, CCallBranch *> call_branch_map_t;
    call_branch_map_t branches;

    while (ctx.consume(Colon)) {
        CCallBranch * pBranch = new CCallBranch();

        pCall->getBranches().add(pBranch);

        if (! ctx.in(RightParen, Hash, Colon)) {
            if (! parseCallResults(ctx, * pCall, * pBranch))
                ERROR(ctx, NULL, L"Failed to parse output parameters");
        }

        if (ctx.is(Hash) && ctx.nextIn(Identifier, Label, Integer)) {
            std::wstring strLabel = ctx.scan(2, 1);
            CLabel * pLabel = ctx.getLabel(strLabel);

            if (pLabel)
                pBranch->setHandler(new CJump(pLabel));
            else
                branches.insert(std::make_pair(strLabel, pBranch));
        }
    }

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    while (ctx.consume(Case)) {
        if (! ctx.in(Label, Identifier, Integer) || ! ctx.nextIs(Colon))
            ERROR(ctx, NULL, L"Label identifier expected");

        typedef call_branch_map_t::iterator I;
        std::pair<I, I> bounds = branches.equal_range(ctx.scan());

        ++ ctx;

        if (bounds.first == bounds.second)
            ERROR(ctx, NULL, L"Label identifier expected");

        CStatement * pStmt = parseStatement(ctx);

        if (! pStmt)
            ERROR(ctx, NULL, L"Statement required");

        for (I i = bounds.first; i != bounds.second; ++ i)
            i->second->setHandler(pStmt);
    }

    _ctx.mergeChildren();

    return pCall;
}

CStatement * CParser::parseStatement(CContext & _ctx) {
    switch (_ctx.getToken()) {
        case Pipe:      return parseMultiAssignment(_ctx);
        case LeftBrace: return parseBlock(_ctx);
        case Switch:    return parseSwitch(_ctx);
        case If:        return parseConditional(_ctx);
        case Hash:      return parseJump(_ctx);

//        case Pragma: return parsePragma(_ctx);

        case Receive: return parseReceive(_ctx);
        case Send:    return parseSend(_ctx);
        case With:    return parseWith(_ctx);

        case For:   return parseFor(_ctx);
        case While: return parseWhile(_ctx);
        case Break: return parseBreak(_ctx);

        case Type: return parseTypeDeclaration(_ctx);

        case Predicate:
        CASE_BUILTIN_TYPE: return parseVariableDeclaration(_ctx, true);
    }

    CStatement * pStmt = NULL;

    if (_ctx.in(Identifier, Label, Integer) && _ctx.nextIs(Colon)) {
        CContext & ctx = * _ctx.createChild(false);
        // A label.
        CLabel * pLabel = ctx.attach(new CLabel(ctx.scan(2)));

        if (ctx.is(RightBrace))
            pStmt = ctx.attach(new CStatement());
        else
            pStmt = parseStatement(ctx);

        if (! pStmt) return NULL;

        if (pStmt->getLabel()) {
            // We should somehow make two labels to point to the same statement.
            CBlock * pBlock = ctx.attach(new CBlock());
            pBlock->add(pStmt);
            pStmt = pBlock;
        }

        pStmt->setLabel(pLabel);
        ctx.addLabel(pLabel);
        _ctx.mergeChildren();
        return pStmt;
    }

    if (_ctx.getType(_ctx.getValue()))
        return parseVariableDeclaration(_ctx, true);

    CContext & ctx = * _ctx.createChild(false);

    // Maybe call?
    if (! pStmt) pStmt = parseCall(ctx);

    // Maybe assignment?
    if (! pStmt) pStmt = parseAssignment(ctx);

    // Maybe nested predicate?
    if (! pStmt) pStmt = parsePredicate(ctx);

    if (! pStmt)
        return NULL;

    _ctx.mergeChildren();

    return pStmt;
}

CProcess * CParser::parseProcess(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(true);

    if (! ctx.consume(Process))
        UNEXPECTED(ctx, "process");

    if (! ctx.is(Identifier))
        ERROR(ctx, NULL, L"Identifier expected");

    CProcess * pProcess = _ctx.attach(new CProcess(ctx.scan()));

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    if (! parseParamList(ctx, pProcess->getInParams(), & CParser::parseParam, 0))
        ERROR(ctx, false, L"Failed to parse input parameters");

    branch_map_t branches;

    while (ctx.consume(Colon)) {
        CBranch * pBranch = new CBranch();

        pProcess->getOutParams().add(pBranch);
        parseParamList(ctx, * pBranch, & CParser::parseParam, OutputParams);

        for (size_t i = 0; i < pBranch->size(); ++ i)
            pBranch->get(i)->setOutput(true);

        if (ctx.is(Hash) && ctx.nextIn(Identifier, Label, Integer)) {
            std::wstring strLabel = ctx.scan(2, 1);
            if (! branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(ctx, false, L"Duplicate branch name \"%ls\"", strLabel.c_str());
            pBranch->setLabel(new CLabel(strLabel));
        }
    }

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    CBlock * pBlock = parseBlock(ctx);

    if (! pBlock)
        ERROR(ctx, false, L"Failed parsing process body");

    pProcess->setBlock(pBlock);

    _ctx.mergeChildren();
    _ctx.addProcess(pProcess);

    return pProcess;
}

CFormulaDeclaration * CParser::parseFormulaDeclaration(CContext & _ctx) {
    if (! _ctx.is(Formula, Identifier))
        return NULL;

    CContext * pCtx = _ctx.createChild(false);
    CFormulaDeclaration * pDecl = pCtx->attach(new CFormulaDeclaration(pCtx->scan(2, 1)));

    if (pCtx->consume(LeftParen) && ! pCtx->consume(RightParen)) {
        pCtx = pCtx->createChild(true);
        if (! parseParamList(* pCtx, pDecl->getParams(), & CParser::parseVariableName))
            return NULL;
        if (! pCtx->consume(RightParen))
            UNEXPECTED(* pCtx, ")");
    }

    if (pCtx->consume(Eq)) {
        CExpression * pFormula = parseExpression(* pCtx, AllowFormulas);

        if (! pFormula)
            return NULL;

        pDecl->setFormula(pFormula);
    }

    _ctx.mergeChildren();
    _ctx.addFormula(pDecl);

    return pDecl;
}

CContext * CParser::parsePragma(CContext & _ctx) {
    CContext & ctx = * _ctx.createChild(false);

    if (! ctx.consume(Pragma))
        UNEXPECTED(ctx, "pragma");

    if (! ctx.consume(LeftParen))
        UNEXPECTED(ctx, "(");

    do {
        if (! ctx.is(Identifier))
            ERROR(ctx, NULL, L"Pragma name expected");

        std::wstring name = ctx.scan();

        if (! ctx.consume(Colon))
            UNEXPECTED(ctx, ":");

        if (name == L"int_bitness") {
            int nBitness = 0;
            if (ctx.is(Integer)) {
                nBitness = wcstol(ctx.scan().c_str(), NULL, 10);
                if (nBitness < 1 || nBitness > 64)
                    ERROR(ctx, NULL, L"Integer bitness out of range: %d", nBitness);
            } else if (ctx.is(Identifier)) {
                std::wstring strBitness = ctx.scan();
                if (strBitness == L"native")
                    nBitness = CNumber::Native;
                else if (strBitness == L"unbounded")
                    nBitness = CNumber::Generic;
                else
                    ERROR(ctx, NULL, L"Unknown bitness value: %ls", strBitness.c_str());
            }
            ctx.getPragma().setIntBitness(nBitness);
        } else if (name == L"real_bitness") {
            int nBitness = 0;
            if (ctx.is(Integer)) {
                nBitness = wcstol(ctx.scan().c_str(), NULL, 10);
                if (nBitness != 32 && nBitness != 64 && nBitness != 128)
                    ERROR(ctx, NULL, L"Real bitness out of range: %d", nBitness);
            } else if (ctx.is(Identifier)) {
                std::wstring strBitness = ctx.scan();
                if (strBitness == L"native")
                    nBitness = CNumber::Native;
                else if (strBitness == L"unbounded")
                    nBitness = CNumber::Generic;
                else
                    ERROR(ctx, NULL, L"Unknown bitness value: %ls", strBitness.c_str());
            }
            ctx.getPragma().setRealBitness(nBitness);
        } else if (name == L"overflow") {
            if (ctx.consume(Hash)) {
                if (! ctx.in(Label, Identifier, Integer))
                    ERROR(ctx, NULL, L"Label identifier expected");

                std::wstring strLabel = ctx.scan();
                CLabel * pLabel = ctx.getLabel(strLabel);

                if (! pLabel)
                    ERROR(ctx, NULL, L"Unknown label %ls", strLabel.c_str());

                ctx.getPragma().overflow().set(COverflow::Return, pLabel);
            } else if (ctx.is(Identifier)) {
                std::wstring strOverflow = ctx.scan();
                int nOverflow = COverflow::Wrap;

                if (strOverflow == L"wrap") {
                    nOverflow = COverflow::Wrap;
                } else if (strOverflow == L"saturate") {
                    nOverflow = COverflow::Saturate;
                } else if (strOverflow == L"strict") {
                    nOverflow = COverflow::Strict;
                } else
                    ERROR(ctx, NULL, L"Unknown overflow value: %ls", strOverflow.c_str());

                ctx.getPragma().overflow().set(nOverflow);
            }
            ctx.getPragma().set(CPragma::Overflow, true);
        } else
            ERROR(ctx, NULL, L"Unknown pragma %ls", name.c_str());
    } while (ctx.consume(Comma));

    if (! ctx.consume(RightParen))
        UNEXPECTED(ctx, ")");

    return & ctx;
}

bool CParser::parseDeclarations(CContext & _ctx, CModule & _module) {
    CContext * pCtx = _ctx.createChild(false);

    while (! pCtx->is(Eof)) {
        switch (pCtx->getToken()) {
            case Identifier: {
                if (! pCtx->getType(pCtx->getValue())) {
                    CPredicate * pPred = parsePredicate(* pCtx);
                    if (! pPred)
                        ERROR(* pCtx, false, L"Failed parsing predicate");
                    _module.getPredicates().add(pPred);
                     break;
                }
                // no break;
            }
            CASE_BUILTIN_TYPE:
            case Mutable: {
                CVariableDeclaration * pDecl = parseVariableDeclaration(* pCtx, false);
                if (! pDecl)
                    ERROR(* pCtx, false, L"Failed parsing variable declaration");
                if (! pCtx->consume(Semicolon))
                    ERROR(* pCtx, false, L"Semicolon expected");
                _module.getVariables().add(pDecl);
                break;
            }
            case Type: {
                CTypeDeclaration * pDecl = parseTypeDeclaration(* pCtx);
                if (! pDecl)
                    ERROR(* pCtx, false, L"Failed parsing type declaration");
                if (! pCtx->consume(Semicolon))
                    ERROR(* pCtx, false, L"Semicolon expected");
                _module.getTypes().add(pDecl);
                 break;
            }
            case Process: {
                CProcess * pProcess = parseProcess(* pCtx);
                if (! pProcess)
                    ERROR(* pCtx, false, L"Failed parsing process declaration");
                _module.getProcesses().add(pProcess);
                 break;
            }
            case Formula: {
                CFormulaDeclaration * pFormula = parseFormulaDeclaration(* pCtx);
                if (! pFormula)
                    ERROR(* pCtx, false, L"Failed parsing formula declaration");
                _module.getFormulas().add(pFormula);
                 break;
            }
            case Pragma: {
                CContext * pCtxNew = parsePragma(* pCtx);
                if (! pCtxNew)
                    ERROR(* pCtx, false, L"Failed parsing compiler directive");

                if (pCtxNew->consume(Semicolon)) {
                    pCtx = pCtxNew;
                    break;
                } else if (pCtxNew->consume(LeftBrace)) {
                    if (! parseDeclarations(* pCtxNew, _module))
                        ERROR(* pCtxNew, false, L"Failed parsing declarations");
                    if (! pCtxNew->consume(RightBrace))
                        ERROR(* pCtxNew, false, L"Closing brace expected");
                    pCtx->mergeChildren();
                    break;
                } else
                    ERROR(* pCtx, false, L"Semicolon or opening brace expected");
            }
            default:
                return false;
        }
    }

    _ctx.mergeChildren();
    return true;
}

bool CParser::parseModule(CContext & _ctx, ir::CModule * & _pModule) {
    _pModule = new ir::CModule();

    parseModuleHeader(_ctx, _pModule);

    while (_ctx.is(Import))
        if (! parseImport(_ctx, _pModule)) {
            _ctx.fmtError(L"Invalid import statement");
            return false;
        }

    if (_ctx.is(Eof) || _ctx.loc() == m_tokens.end())
        return true;

    return parseDeclarations(_ctx, * _pModule);
}

bool parse(tokens_t & _tokens, ir::CModule * & _pModule) {
    loc_t loc = _tokens.begin();
    CParser parser(_tokens);
    CContext ctx(loc, true);

    if (! parser.parseModule(ctx, _pModule)) {
/*        CContext * pCtx = & ctx;

        while (pCtx->getChild())
            pCtx = pCtx->getChild();

//        std::wcerr << L"Parsing failed at line " << pCtx->loc()->getLine();

        std::wcerr << pCtx->getMessages().size() << std::endl;
*/
        ctx.mergeChildren(true);
        std::wcerr << L"Parsing failed at line " << ctx.loc()->getLine() << std::endl;

        for (messages_t::const_iterator i = ctx.getMessages().begin();
            i != ctx.getMessages().end(); ++ i)
        {
            const message_t & msg = * i;
            std::wcerr << msg;
        }

        delete _pModule;
        _pModule = NULL;
        return false;
    }

    ctx.mergeChildren(true);

    DEBUG(L"Done.");

    return true;
}
