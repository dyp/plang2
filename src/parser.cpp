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

#include "typecheck.h"
#include "prettyprinter.h"
#include "collect_constraints.h"
#include "options.h"

#include <iostream>
#include <algorithm>
#include <map>

using namespace ir;
using namespace lexer;

#define PARSER_FN(_Node,_Name,...) _Node * (Parser::* _Name) (Context & _ctx, ## __VA_ARGS__)

#define CASE_BUILTIN_TYPE \
    case INT_TYPE: \
    case NAT_TYPE: \
    case REAL_TYPE: \
    case BOOL_TYPE: \
    case CHAR_TYPE: \
    case SUBTYPE: \
    case ENUM: \
    case STRUCT: \
    case UNION: \
    case STRING_TYPE: \
    case SEQ: \
    case SET: \
    case ARRAY: \
    case MAP: \
    case LIST: \
    case VAR

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
        if (Options::instance().bVerbose) { \
            fwprintf(stderr, (_FMT), ## __VA_ARGS__); \
            fwprintf(stderr, L"\n"); \
        } \
    } while (0)

#define TOK_S(_CTX) (fmtQuote((_CTX).getValue()).c_str())

struct operator_t {
    int nPrecedence, nBinary, nUnary;

    operator_t() : nPrecedence(-1), nBinary(-1), nUnary(-1) {}

    operator_t(int _nPrecedence, int _nBinary = -1, int _nUnary = -1) :
        nPrecedence(_nPrecedence), nBinary(_nBinary), nUnary(_nUnary) {}
};

class Parser {
public:
    Parser(Tokens & _tokens) : m_tokens(_tokens) { initOps(); }

    bool parseModule(Context & _ctx, Module * & _pModule);
    bool parseModuleHeader(Context & _ctx, Module * _pModule);
    bool parseImport(Context & _ctx, Module * _pModule);

    Type * parseType(Context & _ctx);
    Type * parseDerivedTypeParameter(Context & _ctx);
    ArrayType * parseArrayType(Context & _ctx);
    MapType * parseMapType(Context & _ctx);
    Range * parseRange(Context & _ctx);
    NamedReferenceType * parseTypeReference(Context & _ctx);
    StructType * parseStructType(Context & _ctx);
    UnionType * parseUnionType(Context & _ctx);
    EnumType * parseEnumType(Context & _ctx);
    Subtype * parseSubtype(Context & _ctx);
    PredicateType * parsePredicateType(Context & _ctx);
    Expression * parseCastOrTypeReference(Context & _ctx, Type * _pType);

    TypeDeclaration * parseTypeDeclaration(Context & _ctx);
    VariableDeclaration * parseVariableDeclaration(Context & _ctx, int _nFlags);
    FormulaDeclaration * parseFormulaDeclaration(Context & _ctx);
    Expression * parseExpression(Context & _ctx, int _nFlags);
    Expression * parseExpression(Context & _ctx) { return parseExpression(_ctx, 0); }
    Expression * parseSubexpression(Context & _ctx, Expression * _lhs, int _minPrec, int _nFlags = 0);
    Expression * parseAtom(Context & _ctx, int _nFlags);
    Expression * parseAtom(Context & _ctx) { return parseAtom(_ctx, 0); }
    Expression * parseComponent(Context & _ctx, Expression & _base);
    Formula * parseFormula(Context & _ctx);
    ArrayIteration * parseArrayIteration(Context & _ctx);
    ArrayPartExpr * parseArrayPart(Context & _ctx, Expression & _base);
    FunctionCall * parseFunctionCall(Context & _ctx, Expression & _base);
    Binder * parseBinder(Context & _ctx, Expression & _base);
    Replacement * parseReplacement(Context & _ctx, Expression & _base);
    Lambda * parseLambda(Context & _ctx);
    bool parsePredicateParamsAndBody(Context & _ctx, AnonymousPredicate & _pred);
    Predicate * parsePredicate(Context & _ctx);
    Process * parseProcess(Context & _ctx);

    Statement * parseStatement(Context & _ctx);
    Block * parseBlock(Context & _ctx);
    Statement * parseAssignment(Context & _ctx);
    Multiassignment * parseMultiAssignment(Context & _ctx);
    Switch * parseSwitch(Context & _ctx);
    If * parseConditional(Context & _ctx);
    Jump * parseJump(Context & _ctx);
//    Statement * parsePragma(Context & _ctx);
    Receive * parseReceive(Context & _ctx);
    Send * parseSend(Context & _ctx);
    With * parseWith(Context & _ctx);
    For * parseFor(Context & _ctx);
    While * parseWhile(Context & _ctx);
    Break * parseBreak(Context & _ctx);
    Call * parseCall(Context & _ctx);
    Expression * parseCallResult(Context & _ctx, VariableDeclaration * & _pDecl);
    bool parseCallResults(Context & _ctx, Call & _call, Collection<Expression> & _list);

    Context * parsePragma(Context & _ctx);

    bool parseDeclarations(Context & _ctx, Module & _module);

//    DeclarationGroup

    // Parameter parsing constants.
    enum {
        ALLOW_EMPTY_NAMES = 0x01,
        OUTPUT_PARAMS = 0x02,
        ALLOW_ASTERSK = 0x04,

        // Variable declarations.
        ALLOW_INITIALIZATION = 0x08,
        LOCAL_VARIABLE = 0x10,
        PART_OF_LIST = 0x20,
    };

    // Expression parsing constants.
    enum {
        ALLOW_FORMULAS = 0x01,
        RESTRICT_TYPES = 0x02,
    };

    template<class _Param>
    bool parseParamList(Context & _ctx, Collection<_Param> & _params,
            PARSER_FN(_Param,_parser,int), int _nFlags = 0);

    template<class _Node, class _Base>
    bool parseList(Context & _ctx, Collection<_Node, _Base> & _list, PARSER_FN(_Node,_parser),
            int _startMarker, int _endMarker, int _delimiter);

    bool parseActualParameterList(Context & _ctx, Collection<Expression> & _exprs) {
        return parseList(_ctx, _exprs, & Parser::parseExpression, LPAREN, RPAREN, COMMA);
    }

    bool parseArrayIndices(Context & _ctx, Collection<Expression> & _exprs) {
        return parseList(_ctx, _exprs, & Parser::parseExpression, LBRACKET, RBRACKET, COMMA);
    }

    Param * parseParam(Context & _ctx, int _nFlags = 0);
//    StructFieldDefinition * parseParam(Context & _ctx);
    NamedValue * parseVariableName(Context & _ctx, int _nFlags = 0);
    EnumValue * parseEnumValue(Context & _ctx);
    NamedValue * parseNamedValue(Context & _ctx);
    ElementDefinition * parseArrayElement(Context & _ctx);
    ElementDefinition * parseMapElement(Context & _ctx);
    StructFieldDefinition * parseFieldDefinition(Context & _ctx);
    StructFieldDefinition * parseConstructorField(Context & _ctx);
    UnionConstructorDeclaration * parseConstructorDeclaration(Context & _ctx);
    UnionConstructor * parseConstructor(Context & _ctx, UnionType * _pUnion);

    template<class _T>
    _T * findByName(const Collection<_T> & _list, const std::wstring & _name);

    template<class _T>
    size_t findByNameIdx(const Collection<_T> & _list, const std::wstring & _name);

    typedef std::map<std::wstring, Branch *> branch_map_t;

    template<class _Pred>
    bool parsePreconditions(Context & _ctx, _Pred & _pred, branch_map_t & _branches);

    template<class _Pred>
    bool parsePostconditions(Context & _ctx, _Pred & _pred, branch_map_t & _branches);

    template<class _Pred>
    bool parseMeasure(Context & _ctx, _Pred & _pred);

private:
    Tokens & m_tokens;
    std::vector<operator_t> m_ops;

    void initOps();
    int getPrecedence(int _token, bool _bExpecColon) const;
    int getUnaryOp(int _token) const;
    int getBinaryOp(int _token) const;

    bool isTypeName(Context & _ctx, const std::wstring & _name) const;
    bool fixupAsteriskedParameters(Context & _ctx, Params & _in, Params & _out);

    bool isTypeVariable(const NamedValue * _pVar) const {
        const Type * pType = NULL;
        return ir::isTypeVariable(_pVar, pType);
    }

    void typecheck(Context &_ctx, Node &_node);
};

template<class _Node, class _Base>
bool Parser::parseList(Context & _ctx, Collection<_Node,_Base> & _list, PARSER_FN(_Node,_parser),
        int _startMarker, int _endMarker, int _delimiter)
{
    Context & ctx = * _ctx.createChild(false);

    if (_startMarker >= 0 && ! ctx.consume(_startMarker))
        return false;

    Collection<_Node> list;
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
_T * Parser::findByName(const Collection<_T> & _list, const std::wstring & _name) {
    const size_t cIdx = findByNameIdx(_list, _name);
    return cIdx == (size_t) -1 ? _list.get(cIdx) : NULL;
}

template<class _T>
size_t Parser::findByNameIdx(const Collection<_T> & _list, const std::wstring & _name) {
    for (size_t i = 0; i < _list.size(); ++ i)
        if (_list.get(i)->getName() == _name)
            return i;

    return (size_t) -1;
}

ArrayPartExpr * Parser::parseArrayPart(Context & _ctx, Expression & _base) {
    Context & ctx = * _ctx.createChild(false);
    ArrayPartExpr * pParts = ctx.attach(new ArrayPartExpr());

    if (! parseArrayIndices(ctx, pParts->getIndices()))
        return NULL;

    pParts->setObject(& _base);
    _ctx.mergeChildren();

    return pParts;
}

FunctionCall * Parser::parseFunctionCall(Context & _ctx, Expression & _base) {
    Context & ctx = * _ctx.createChild(false);
    FunctionCall * pCall = ctx.attach(new FunctionCall());

    if (! parseActualParameterList(ctx, pCall->getArgs()))
        return NULL;

    pCall->setPredicate(& _base);
    _ctx.mergeChildren();

    return pCall;
}

Binder * Parser::parseBinder(Context & _ctx, Expression & _base) {
    Context & ctx = * _ctx.createChild(false);
    Binder * pBinder = ctx.attach(new Binder());

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    Expression * pParam = NULL;

    if (! ctx.consume(ELLIPSIS)) {
        if (ctx.consume(UNDERSCORE))
            pParam = NULL;
        else if (! (pParam = parseExpression(ctx)))
            ERROR(ctx, NULL, L"Error parsing expression");

        pBinder->getArgs().add(pParam);

        while (ctx.consume(COMMA)) {
            if (ctx.consume(ELLIPSIS))
                break;
            else if (ctx.consume(UNDERSCORE))
                pParam = NULL;
            else if (! (pParam = parseExpression(ctx)))
                ERROR(ctx, NULL, L"Error parsing expression");

            pBinder->getArgs().add(pParam);
        }
    }

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    pBinder->setPredicate(& _base);
    _ctx.mergeChildren();

    return pBinder;
}

Replacement * Parser::parseReplacement(Context & _ctx, Expression & _base) {
    Context & ctx = * _ctx.createChild(false);
    Expression * pNewValues = parseExpression(ctx);

    if (! pNewValues)
        ERROR(ctx, NULL, L"Error parsing replacement values");

    if (pNewValues->getKind() != Expression::CONSTRUCTOR)
        ERROR(ctx, NULL, L"Constructor expected");

    Replacement * pExpr = ctx.attach(new Replacement());

    pExpr->setObject(& _base);
    pExpr->setNewValues((Constructor *) pNewValues);
    _ctx.mergeChildren();

    return pExpr;
}

Expression * Parser::parseComponent(Context & _ctx, Expression & _base) {
    Context & ctx = * _ctx.createChild(false);
    Expression * pExpr = NULL;

    if (ctx.is(DOT)) {
        const std::wstring & fieldName = ctx.scan(2, 1);
        Component * pExpr = new StructFieldExpr(fieldName);
        /*const Type * pBaseType = resolveBaseType(_base.getType());

        if (! pBaseType || (pBaseType->getKind() != Type::Struct && pBaseType->getKind() != Type::Union))
            ERROR(ctx, NULL, L"Struct or union typed expression expected");

        const std::wstring & fieldName = ctx.scan(2, 1);
        Component * pExpr = NULL;

        if (pBaseType->getKind() == Type::Struct) {
            StructType * pStruct = (StructType *) pBaseType;
            const size_t cFieldIdx = findByNameIdx(pStruct->getFields(), fieldName);

            if (cFieldIdx != (size_t) -1)
                pExpr = new StructFieldExpr(pStruct, cFieldIdx, false);
        } else {
            UnionType * pUnion = (UnionType *) pBaseType;
            union_field_idx_t idx = pUnion->findField(fieldName);

            if (idx.first != (size_t) -1) {
                pExpr = new UnionAlternativeExpr(pUnion, idx);
            }
        }

        if (! pExpr)
            ERROR(ctx, NULL, L"Could not find component %ls", fieldName.c_str());
*/
        _ctx.mergeChildren();
        _ctx.attach(pExpr);
        pExpr->setObject(& _base);

        return pExpr;
    } else if (ctx.is(LBRACKET)) {
        pExpr = parseArrayPart(ctx, _base);

        if (! pExpr)
            pExpr = parseReplacement(ctx, _base);
    } else if (ctx.is(LPAREN)) {
        if (_base.getKind() == Expression::TYPE) {
            pExpr = parseExpression(ctx);

            if (! pExpr)
                return NULL;

            CastExpr * pCast = new CastExpr();

            pCast->setToType((TypeExpr *) & _base, true);
            pCast->setExpression(pExpr);
            _ctx.mergeChildren();
            _ctx.attach(pCast);

            return pCast;
        }

        pExpr = parseFunctionCall(ctx, _base);

        if (! pExpr)
            pExpr = parseBinder(ctx, _base);

        if (! pExpr)
            pExpr = parseReplacement(ctx, _base);
    } else if (ctx.in(MAP_LBRACKET, FOR)) {
        pExpr = parseReplacement(ctx, _base);
    }

    if (! pExpr)
        return NULL;

    _ctx.mergeChildren();

    return pExpr;
}

ArrayIteration * Parser::parseArrayIteration(Context & _ctx) {
    Context & ctx = * _ctx.createChild(true);

    if (! ctx.consume(FOR))
        UNEXPECTED(ctx, "for");

    ArrayIteration * pArray = _ctx.attach(new ArrayIteration());

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    if (! parseParamList(ctx, pArray->getIterators(), & Parser::parseVariableName))
        ERROR(ctx, NULL, L"Failed parsing list of iterators");

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    if (ctx.is(LBRACE)) {
        Context & ctxParts = * ctx.createChild(false);

        ++ ctxParts;

        while (ctxParts.is(CASE)) {
            ArrayPartDefinition * pPart = ctxParts.attach(new ArrayPartDefinition());

            if (! parseList(ctxParts, pPart->getConditions(),
                    & Parser::parseExpression, CASE, COLON, COMMA))
                ERROR(ctxParts, NULL, L"Error parsing list of expressions");

            Expression * pExpr = parseExpression(ctxParts);
            if (! pExpr)
                ERROR(ctxParts, NULL, L"Expression required");
            pPart->setExpression(pExpr);

            pArray->add(pPart);
        }

        if (ctxParts.is(DEFAULT, COLON)) {
            ctxParts.skip(2);
            Expression * pExpr = parseExpression(ctxParts);
            if (! pExpr)
                ERROR(ctxParts, NULL, L"Expression required");
            pArray->setDefault(pExpr);
        }

        if (! pArray->empty() || pArray->getDefault()) {
            if (! ctxParts.consume(RBRACE))
                UNEXPECTED(ctxParts, "}");
            ctx.mergeChildren();
        }
    }

    if (pArray->empty() && ! pArray->getDefault()) {
        Expression * pExpr = parseExpression(ctx);
        if (! pExpr)
            ERROR(ctx, NULL, L"Expression or parts definition required");
        pArray->setDefault(pExpr);
    }

    _ctx.mergeChildren();

    return pArray;
}

int Parser::getPrecedence(int _token, bool _bExpecColon) const {
    if (_bExpecColon && _token == COLON)
        return m_ops[QUESTION].nPrecedence;
    return m_ops[_token].nPrecedence;
}

int Parser::getUnaryOp(int _token) const {
    return m_ops[_token].nUnary;
}

int Parser::getBinaryOp(int _token) const {
    return m_ops[_token].nBinary;
}

void Parser::initOps() {
    m_ops.resize(END_OF_FILE + 1);

    int nPrec = 0;

    m_ops[IMPLIES]    = operator_t(nPrec, Binary::IMPLIES);
    m_ops[IFF]        = operator_t(nPrec, Binary::IFF);
    m_ops[QUESTION]   = operator_t(++ nPrec);
//    m_oPS[COLON]      = operator_t(nPrec);
    m_ops[OR]         = operator_t(++ nPrec, Binary::BOOL_OR);
    m_ops[XOR]        = operator_t(++ nPrec, Binary::BOOL_XOR);
    m_ops[AMPERSAND]  = operator_t(++ nPrec, Binary::BOOL_AND);
    m_ops[AND]        = operator_t(nPrec, Binary::BOOL_AND);
    m_ops[EQ]         = operator_t(++ nPrec, Binary::EQUALS);
    m_ops[NE]         = operator_t(nPrec, Binary::NOT_EQUALS);
    m_ops[LT]         = operator_t(++ nPrec, Binary::LESS);
    m_ops[LTE]        = operator_t(nPrec, Binary::LESS_OR_EQUALS);
    m_ops[GT]         = operator_t(nPrec, Binary::GREATER);
    m_ops[GTE]        = operator_t(nPrec, Binary::GREATER_OR_EQUALS);
    m_ops[IN]         = operator_t(++ nPrec, Binary::IN);
    m_ops[SHIFTLEFT]  = operator_t(++ nPrec, Binary::SHIFT_LEFT);
    m_ops[SHIFTRIGHT] = operator_t(nPrec, Binary::SHIFT_RIGHT);
    m_ops[PLUS]       = operator_t(++ nPrec, Binary::ADD, Unary::PLUS);
    m_ops[MINUS]      = operator_t(nPrec, Binary::SUBTRACT, Unary::MINUS);
    m_ops[BANG]       = operator_t(++ nPrec, -1, Unary::BOOL_NEGATE);
    m_ops[TILDE]      = operator_t(nPrec, -1, Unary::BITWISE_NEGATE);
    m_ops[ASTERISK]   = operator_t(++ nPrec, Binary::MULTIPLY);
    m_ops[SLASH]      = operator_t(nPrec, Binary::DIVIDE);
    m_ops[PERCENT]    = operator_t(nPrec, Binary::REMAINDER);
    m_ops[CARET]      = operator_t(++ nPrec, Binary::POWER);
}

Expression * Parser::parseCastOrTypeReference(Context & _ctx, Type * _pType) {
    Expression * pExpr = NULL;
    Context & ctx = * _ctx.createChild(false);

    if (ctx.in(LPAREN, LBRACKET, MAP_LBRACKET, LBRACE, LIST_LBRACKET)) {
        switch (_pType->getKind()) {
            case Type::SEQ:
            case Type::ARRAY:
            case Type::SET:
            case Type::MAP:
            case Type::LIST:
            //case Type::Optional:
            //case Type::Parameterized:
            //case Type::NamedReference:
                pExpr = parseAtom(ctx, RESTRICT_TYPES);
                if (pExpr)
                    pExpr->setType(_pType);
        }
    }

    if (! pExpr)
        pExpr = _ctx.attach(new TypeExpr(_pType));

    _ctx.mergeChildren();

    return pExpr;
}

Lambda * Parser::parseLambda(Context & _ctx) {
    Context & ctx = * _ctx.createChild(true);

    if (! ctx.consume(PREDICATE))
        UNEXPECTED(ctx, "predicate");

    Lambda * pLambda = _ctx.attach(new Lambda());

    if (! parsePredicateParamsAndBody(ctx, pLambda->getPredicate()))
        return NULL;

    if (! pLambda->getPredicate().getBlock())
        ERROR(ctx, NULL, L"No body defined for anonymous predicate");

    _ctx.mergeChildren();

    return pLambda;
}

Formula * Parser::parseFormula(Context & _ctx) {
    Context & ctx = * _ctx.createChild(true);

    if (! ctx.in(BANG, QUESTION, FORALL, EXISTS))
        ERROR(ctx, NULL, L"Quantifier expected");

    Formula * pFormula = _ctx.attach(new Formula(ctx.in(BANG, FORALL) ?
            Formula::UNIVERSAL : Formula::EXISTENTIAL));

    ++ ctx;

    if (! parseParamList(ctx, pFormula->getBoundVariables(),
            & Parser::parseVariableName, 0))
        ERROR(ctx, NULL, L"Failed parsing bound variables");

    if (! ctx.consume(DOT))
        UNEXPECTED(ctx, ".");

    // OK to use parseExpression instead of parseSubexpression
    // since quantifiers are lowest priority right-associative operators.
    Expression * pSub = parseExpression(ctx, ALLOW_FORMULAS);

    if (! pSub)
        ERROR(ctx, NULL, L"Failed parsing subformula");

    pFormula->setSubformula(pSub);
    _ctx.mergeChildren();

    return pFormula;
}

Expression * Parser::parseAtom(Context & _ctx, int _nFlags) {
    Context & ctx = * _ctx.createChild(false);
    ir::Expression * pExpr = NULL;
    const bool bAllowTypes = ! (_nFlags & RESTRICT_TYPES);
    int token = ctx.getToken();

    _nFlags &= ~RESTRICT_TYPES;

    if (ctx.is(LPAREN, RPAREN) || ctx.is(LBRACKET, RBRACKET) || ctx.is(LBRACE, RBRACE) ||
            ctx.is(LIST_LBRACKET, LIST_RBRACKET) || ctx.is(MAP_LBRACKET, MAP_RBRACKET))
    {
        ctx.skip(2);
        pExpr = ctx.attach(new Literal());
        token = -1;
    }

    switch (token) {
        case INTEGER: {
            Number num(ctx.scan(), Number::INTEGER);
            pExpr = ctx.attach(new Literal(num));
            break;
        }
        case REAL:
        case NOT_A_NUMBER:
        case INF: {
            Number num(ctx.scan(), Number::REAL);
            pExpr = ctx.attach(new Literal(num));
            break;
        }
        case TRUE:
        case FALSE:
            pExpr = ctx.attach(new Literal(ctx.getToken() == TRUE));
            ++ ctx;
            break;
        case CHAR:
            pExpr = ctx.attach(new Literal(ctx.scan()[0]));
            break;
        case STRING:
            pExpr = ctx.attach(new Literal(ctx.scan()));
            break;
        case NIL:
            ++ ctx;
            pExpr = ctx.attach(new Literal());
            break;
        case LPAREN: {
            Context * pCtx = ctx.createChild(false);
            ++ (* pCtx);

            pExpr = parseExpression(* pCtx, _nFlags);
            if (! pExpr || ! pCtx->consume(RPAREN)) {
                // Try to parse as a struct literal.
                pCtx = ctx.createChild(false);
                StructConstructor * pStruct = pCtx->attach(new StructConstructor());
                if (! parseList(* pCtx, * pStruct, & Parser::parseFieldDefinition, LPAREN, RPAREN, COMMA))
                    ERROR(* pCtx, NULL, L"Expected \")\" or a struct literal");
                pExpr = pStruct;
            }
            ctx.mergeChildren();
            break;
        }
        case LBRACKET:
            pExpr = ctx.attach(new ArrayConstructor());
            if (! parseList(ctx, * (ArrayConstructor *) pExpr, & Parser::parseArrayElement,
                    LBRACKET, RBRACKET, COMMA))
                ERROR(ctx, NULL, L"Failed parsing array constructor");
            break;
        case MAP_LBRACKET:
            pExpr = ctx.attach(new MapConstructor());
            if (! parseList(ctx, * (MapConstructor *) pExpr, & Parser::parseMapElement,
                    MAP_LBRACKET, MAP_RBRACKET, COMMA))
                ERROR(ctx, NULL, L"Failed parsing map constructor");
            break;
        case LBRACE:
            pExpr = ctx.attach(new SetConstructor());
            if (! parseList(ctx, * (SetConstructor *) pExpr, & Parser::parseExpression,
                    LBRACE, RBRACE, COMMA))
                ERROR(ctx, NULL, L"Failed parsing set constructor");
            break;
        case LIST_LBRACKET:
            pExpr = ctx.attach(new ListConstructor());
            if (! parseList(ctx, * (ListConstructor *) pExpr, & Parser::parseExpression,
                    LIST_LBRACKET, LIST_RBRACKET, COMMA))
                ERROR(ctx, NULL, L"Failed parsing list constructor");
            break;
        case FOR:
            pExpr = parseArrayIteration(ctx);
            break;
        case PREDICATE:
            pExpr = parseLambda(ctx);
            if (pExpr)
                break;
            // No break; try to parse as a predicate type.
        CASE_BUILTIN_TYPE:
            if (bAllowTypes) {
                Type * pType = parseType(ctx);

                if (! pType)
                    ERROR(ctx, NULL, L"Type reference expected");

                pExpr = parseCastOrTypeReference(ctx, pType);
            }
            break;
        case BANG:
        case QUESTION:
        case FORALL:
        case EXISTS:
            if (_nFlags & ALLOW_FORMULAS)
                pExpr = parseFormula(ctx);
            break;
    }

    if (! pExpr && ctx.is(IDENTIFIER)) {
        std::wstring str = ctx.getValue();
        const NamedValue * pVar = NULL;
        bool bLinkedIdentifier = false;

        if (ctx.nextIs(SINGLE_QUOTE)) {
            str += L'\'';
            bLinkedIdentifier = true;
        }

        if ((pVar = ctx.getVariable(str)) && (!bAllowTypes || !isTypeVariable(pVar))) {
            pExpr = ctx.attach(new VariableReference(pVar));
            ctx.skip(bLinkedIdentifier ? 2 : 1);
        }

        if (bLinkedIdentifier && ! pExpr)
            ERROR(ctx, NULL, L"Parameter with name %ls not found", str.c_str());

        if (! pExpr && _ctx.getConstructor(str) != NULL)
            pExpr = parseConstructor(ctx, NULL);

        const Predicate * pPred = NULL;

        if (! pExpr && (pPred = ctx.getPredicate(str))) {
//            pExpr = ctx.attach(new PredicateReference(pPred));
//            pExpr->setType(pPred->getType(), false);
            pExpr = ctx.attach(new PredicateReference(str));
            //pExpr->setType(pPred->getType(), false);
            ++ ctx;
        }

        FormulaDeclaration * pFormula = NULL;

        if (! pExpr && (_nFlags & ALLOW_FORMULAS) && (pFormula = ctx.getFormula(str))) {
            FormulaCall * pCall = ctx.attach(new FormulaCall());

            ++ ctx;

            if (ctx.is(LPAREN, RPAREN))
                ctx.skip(2);
            else if (! parseActualParameterList(ctx, pCall->getArgs()))
                return NULL;

            pCall->setTarget(pFormula);
            pExpr = pCall;
        }

        TypeDeclaration * pTypeDecl = NULL;
        const Type * pRealType = NULL;

        if (! pExpr) {
            pTypeDecl = ctx.getType(str);
            if (pTypeDecl != NULL)
                pRealType = pTypeDecl->getType();
//            pRealType = resolveBaseType(pTypeDecl->getType());
        }

        if (! pExpr && pRealType != NULL && pRealType->getKind() == Type::UNION && ctx.nextIs(DOT, IDENTIFIER)) {
            // It's ok since we always know the UnionType in UnionType.ConstructorName expression even
            // before type inference.
            ctx.skip(2);
            pExpr = parseConstructor(ctx, (UnionType *) pRealType);
            if (pExpr == NULL)
                return NULL;
        }

        //if (! pExpr && pType != NULL && pType->getKind() == Type::Union && ctx.nextIs(Dot, Identifier)) {
        //}


        if (! pExpr && bAllowTypes) {
            Type * pType = parseType(ctx);
            if (pType != NULL)
                pExpr = parseCastOrTypeReference(ctx, pType);
        }

        if (! pExpr)
            ERROR(ctx, NULL, L"Unknown identifier: %ls", str.c_str());
    }

    // Other things should be implemented HERE.

    if (pExpr) {
        Expression * pCompound = NULL;
        while ((pCompound = parseComponent(ctx, * pExpr)))
            pExpr = pCompound;
    }

    if (pExpr && bAllowTypes && ctx.consume(DOUBLE_DOT)) {
        // Can be a range.
        Expression * pMax = parseExpression(ctx, _nFlags);
        if (! pMax)
            return NULL;
        pExpr = ctx.attach(new TypeExpr(new Range(pExpr, pMax)));
    }

    if (! pExpr)
        ERROR(ctx, NULL, L"Unexpected token while parsing expression: %ls", TOK_S(ctx));

    _ctx.mergeChildren();
    return pExpr;
}

Expression * Parser::parseSubexpression(Context & _ctx, Expression * _lhs, int _minPrec, int _nFlags)
{
    Context & ctx = * _ctx.createChild(false);
    bool bParseElse = false;

    while (! _lhs || getPrecedence(ctx.getToken(), bParseElse) >= _minPrec) {
        const int op = ctx.getToken();
        Expression * rhs = NULL;
        int nPrec = std::max(_minPrec, getPrecedence(op, bParseElse));

        if ((_nFlags & ALLOW_FORMULAS) && ! _lhs && ctx.in(BANG, QUESTION)) {
            // Try to parse as a quantified formula first.
            _lhs = parseFormula(ctx);
            if (_lhs)
                break;
        }

        ++ ctx;

        const int tokRHS = ctx.getToken();

        if (getUnaryOp(ctx.getToken()) >= 0) {
            rhs = parseSubexpression(ctx, NULL, nPrec + 1, _nFlags);
        } else {
            rhs = parseAtom(ctx, _nFlags);
        }

        if (! rhs) return NULL;

        while (getPrecedence(ctx.getToken(), bParseElse) > nPrec) {
            Expression * rhsNew = parseSubexpression(ctx, rhs, getPrecedence(ctx.getToken(), bParseElse), _nFlags);
            if (! rhsNew) return NULL;
            rhs = rhsNew;
        }

        if (bParseElse) {
            if (op != COLON)
                ERROR(ctx, NULL, L"\":\" expected");
            ((Ternary *) _lhs)->setElse(rhs);
            bParseElse = false;
        } else if (op == QUESTION) {
            if (! _lhs) return NULL;
            bParseElse = true;
            _lhs = ctx.attach(new Ternary(_lhs, rhs));
        } else if (! _lhs) {
            const int unaryOp = getUnaryOp(op);

            if (unaryOp < 0)
                ERROR(ctx, NULL, L"Unary operator expected");

            if (tokRHS != LPAREN && // Disable optimization of "-(NUMBER)" expressions for now.
                    rhs->getKind() == Expression::LITERAL &&
                    ((Literal *) rhs)->getLiteralKind() == Literal::NUMBER)
            {
                // Ok, handle unary plus/minus here.
                if (unaryOp == Unary::MINUS) {
                    Number num = ((Literal *)rhs)->getNumber();
                    num.negate();
                    ((Literal *)rhs)->setNumber(num);
                }
                if (unaryOp == Unary::MINUS || unaryOp == Unary::PLUS) {
                    _lhs = rhs;
                    continue;
                }
            }

            _lhs = ctx.attach(new Unary(unaryOp, rhs));
            ((Unary *) _lhs)->getOverflow().set(_ctx.getOverflow());
        } else {
            const int binaryOp = getBinaryOp(op);
            if (binaryOp < 0)
                ERROR(ctx, NULL, L"Binary operator expected");
            _lhs = ctx.attach(new Binary(binaryOp, _lhs, rhs));
            ((Binary *) _lhs)->getOverflow().set(_ctx.getOverflow());
        }
    }

    _ctx.mergeChildren();

    return _lhs;
}

Expression * Parser::parseExpression(Context & _ctx, int _nFlags) {
    Expression * pExpr = parseAtom(_ctx, _nFlags);

    if (! pExpr) {
        Context * pCtx = _ctx.getChild();
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
bool Parser::parsePreconditions(Context & _ctx, _Pred & _pred, branch_map_t & _branches) {
    if (! _ctx.consume(PRE))
        return false;

    branch_map_t::iterator iBranch = _branches.end();

    if (_ctx.in(LABEL, IDENTIFIER, INTEGER) && _ctx.nextIs(COLON))
        iBranch = _branches.find(_ctx.getValue());

    if (iBranch != _branches.end()) {
        Branch * pBranch = iBranch->second;
        _ctx.skip(2);
        Expression * pFormula = parseExpression(_ctx, ALLOW_FORMULAS);
        if (! pFormula)
            ERROR(_ctx, false, L"Formula expected");
        if (pFormula->getKind() == Expression::FORMULA)
            pBranch->setPreCondition((Formula *) pFormula);
        else
            pBranch->setPreCondition(_ctx.attach(new Formula(Formula::NONE, pFormula)));
    } else {
        Expression * pFormula = parseExpression(_ctx, ALLOW_FORMULAS);
        if (! pFormula)
            ERROR(_ctx, false, L"Formula expected");
        if (pFormula->getKind() == Expression::FORMULA)
            _pred.setPreCondition((Formula *) pFormula);
        else
            _pred.setPreCondition(_ctx.attach(new Formula(Formula::NONE, pFormula)));
    }

    while (_ctx.consume(PRE)) {
        Branch * pBranch = _branches[_ctx.getValue()];

        if (! _ctx.in(LABEL, IDENTIFIER, INTEGER) || ! _ctx.nextIs(COLON) || ! pBranch)
            ERROR(_ctx, false, L"Branch name expected");

        _ctx.skip(2);
        Expression * pFormula = parseExpression(_ctx, ALLOW_FORMULAS);
        if (! pFormula)
            ERROR(_ctx, NULL, L"Formula expected");
        if (pFormula->getKind() != Expression::FORMULA)
            pFormula = _ctx.attach(new Formula(Formula::NONE, pFormula));
        pBranch->setPreCondition((Formula *) pFormula);
    }

    return true;
}

template<class _Pred>
bool Parser::parsePostconditions(Context & _ctx, _Pred & _pred, branch_map_t & _branches) {
    if (! _ctx.is(POST))
        return false;

    if (_pred.isHyperFunction()) {
        while (_ctx.consume(POST)) {
            Branch * pBranch = _branches[_ctx.getValue()];

            if (! _ctx.in(LABEL, IDENTIFIER, INTEGER) || ! _ctx.nextIs(COLON) || ! pBranch)
                ERROR(_ctx, false, L"Branch name expected");

            _ctx.skip(2);
            Expression * pFormula = parseExpression(_ctx, ALLOW_FORMULAS);
            if (! pFormula)
                ERROR(_ctx, NULL, L"Formula expected");
            if (pFormula->getKind() != Expression::FORMULA)
                pFormula = _ctx.attach(new Formula(Formula::NONE, pFormula));
            pBranch->setPostCondition((Formula *) pFormula);
        }
    } else if (_ctx.consume(POST)) {
        Expression * pFormula = parseExpression(_ctx, ALLOW_FORMULAS);
        if (! pFormula)
            ERROR(_ctx, false, L"Formula expected");
        if (pFormula->getKind() != Expression::FORMULA)
            pFormula = _ctx.attach(new Formula(Formula::NONE, pFormula));
        _pred.setPostCondition((Formula *) pFormula);
    }

    return true;
}

template<class _Pred>
bool Parser::parseMeasure(Context &_ctx, _Pred &_pred) {
    if (!_ctx.consume(MEASURE))
        return false;

    Expression *pMeasure = parseExpression(_ctx);

    if (!pMeasure)
        ERROR(_ctx, false, L"Expression expected");

    _pred.setMeasure(pMeasure);

    return true;
}

bool Parser::fixupAsteriskedParameters(Context & _ctx, Params & _in, Params & _out) {
    bool bResult = false;

    for (size_t i = 0; i < _in.size(); ++ i) {
        Param * pInParam = _in.get(i);
        if (pInParam->getLinkedParam() != pInParam)
            continue;

        const std::wstring name = pInParam->getName() + L'\'';
        Param * pOutParam = new Param(name);

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

bool Parser::parsePredicateParamsAndBody(Context & _ctx, AnonymousPredicate & _pred) {
    if (! _ctx.consume(LPAREN))
        ERROR(_ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(_ctx));

    if (! parseParamList(_ctx, _pred.getInParams(), & Parser::parseParam, ALLOW_ASTERSK | ALLOW_EMPTY_NAMES))
        ERROR(_ctx, false, L"Failed to parse input parameters");

    branch_map_t branches;

    bool bHasAsterisked = false;

    while (_ctx.consume(COLON)) {
        Branch * pBranch = new Branch();

        _pred.getOutParams().add(pBranch);

        if (_pred.getOutParams().size() == 1)
            bHasAsterisked = fixupAsteriskedParameters(_ctx, _pred.getInParams(), * pBranch);
        else if (bHasAsterisked)
            ERROR(_ctx, false, L"Hyperfunctions cannot use '*' in parameter list");

        parseParamList(_ctx, * pBranch, & Parser::parseParam, OUTPUT_PARAMS | ALLOW_EMPTY_NAMES);
        for (size_t i = 0; i < pBranch->size(); ++ i)
            pBranch->get(i)->setOutput(true);

        if (_ctx.is(HASH) && _ctx.nextIn(IDENTIFIER, LABEL, INTEGER)) {
            ++ _ctx;
            std::wstring strLabel = _ctx.getValue();

            if (_ctx.is(INTEGER) && wcstoul(strLabel.c_str(), NULL, 10) != _pred.getOutParams().size())
                ERROR(_ctx, false, L"Numbers of numeric branch labels should correspond to branch order");

            ++ _ctx;

            if (! branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(_ctx, false, L"Duplicate branch name \"%ls\"", strLabel.c_str());

            pBranch->setLabel(new Label(strLabel));
            _ctx.addLabel(pBranch->getLabel());
        }
    }

    if (_pred.getOutParams().empty()) {
        Branch * pBranch = new Branch();
        _pred.getOutParams().add(pBranch);
        fixupAsteriskedParameters(_ctx, _pred.getInParams(), * pBranch);
    }

    // Create labels for unlabeled branches.
    if (_pred.getOutParams().size() > 1) {
        for (size_t i = 0; i < _pred.getOutParams().size(); ++ i) {
            Branch * pBranch = _pred.getOutParams().get(i);
            if (! pBranch->getLabel()) {
                pBranch->setLabel(new Label(fmtInt(i + 1)));
                _ctx.addLabel(pBranch->getLabel());
                branches[pBranch->getLabel()->getName()] = pBranch;
            }
        }
    }

    if (! _ctx.consume(RPAREN))
        ERROR(_ctx, false, L"Expected \")\", got: %ls", TOK_S(_ctx));

    if (_ctx.is(PRE))
        if (! parsePreconditions(_ctx, _pred, branches))
            ERROR(_ctx, false, L"Failed parsing preconditions");

    if (_ctx.is(LBRACE)) {
        Block * pBlock = parseBlock(_ctx);

        if (! pBlock)
            ERROR(_ctx, false, L"Failed parsing predicate body");

        _pred.setBlock(pBlock);
    }

    if (_ctx.is(POST))
        if (! parsePostconditions(_ctx, _pred, branches))
            ERROR(_ctx, false, L"Failed parsing postconditions");

    if (_ctx.is(MEASURE))
        if (!parseMeasure(_ctx, _pred))
            ERROR(_ctx, false, L"Failed parsing measure");

    return true;
}

Predicate * Parser::parsePredicate(Context & _ctx) {
    Context * pCtx = _ctx.createChild(false);

    if (! pCtx->is(IDENTIFIER))
        return NULL;

    Predicate * pPred = pCtx->attach(new Predicate(pCtx->scan()));

    pCtx->addPredicate(pPred);
    pCtx = pCtx->createChild(true);

    if (! parsePredicateParamsAndBody(* pCtx, * pPred))
        return NULL;

    if (! pPred->getBlock() && ! pCtx->consume(SEMICOLON))
        ERROR(* pCtx, NULL, L"Expected block or a semicolon");

    _ctx.mergeChildren();

    return pPred;
}

VariableDeclaration * Parser::parseVariableDeclaration(Context & _ctx, int _nFlags) {
    Context & ctx = * _ctx.createChild(false);
    const bool bMutable = ctx.consume(MUTABLE);
    Type * pType = NULL;

    if ((_nFlags & PART_OF_LIST) == 0) {
        pType = parseType(ctx);

        if (! pType)
            return NULL;
    }

    if (! ctx.is(IDENTIFIER))
        ERROR(ctx, NULL, L"Expected identifier, got: %ls", TOK_S(ctx));

    VariableDeclaration * pDecl = ctx.attach(new VariableDeclaration(_nFlags & LOCAL_VARIABLE, ctx.scan()));

    if ((_nFlags & PART_OF_LIST) == 0)
        pDecl->getVariable()->setType(pType);

    pDecl->getVariable()->setMutable(bMutable);

    if ((_nFlags & ALLOW_INITIALIZATION) && ctx.consume(EQ)) {
        Expression * pExpr = parseExpression(ctx);

        if (! pExpr) return NULL;
        pDecl->setValue(pExpr);
    }

    _ctx.mergeChildren();
    _ctx.addVariable(pDecl->getVariable());

    return pDecl;
}

bool Parser::parseModuleHeader(Context & _ctx, Module * _pModule) {
    if (_ctx.is(MODULE, IDENTIFIER, SEMICOLON)) {
        _pModule->setName(_ctx.scan(3, 1));
        return true;
    }

    return false;
}

bool Parser::parseImport(Context & _ctx, Module * _pModule) {
    if (_ctx.is(IMPORT, IDENTIFIER, SEMICOLON)) {
        _pModule->getImports().push_back(_ctx.scan(3, 1));
        return true;
    }

    return false;
}

Type * Parser::parseDerivedTypeParameter(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    Type * pType = NULL;

    if (! ctx.consume(LPAREN))
        ERROR(ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(ctx));
    if (! (pType = parseType(ctx)))
        return NULL;
    if (! ctx.consume(RPAREN))
        ERROR(ctx, NULL, L"Expected \")\", got: %ls", TOK_S(ctx));

    _ctx.mergeChildren();
    return pType;
}

ArrayType * Parser::parseArrayType(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    NamedValue * parseNamedValue(Context & _ctx);

    if (! ctx.consume(ARRAY))
        ERROR(ctx, NULL, L"Expected \"array\", got: %ls", TOK_S(ctx));

    if (! ctx.consume(LPAREN))
        ERROR(ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(ctx));

    Type * pType = parseType(ctx);

    if (! pType)
        return NULL;

    ArrayType * pArray = ctx.attach(new ArrayType(pType));

    if (! parseList(ctx, pArray->getDimensions(), & Parser::parseRange, COMMA, RPAREN, COMMA))
        return NULL;

    _ctx.mergeChildren();
    return pArray;
}

MapType * Parser::parseMapType(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(MAP))
        UNEXPECTED(ctx, "map");

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    Type * pIndexType = parseType(ctx);

    if (! pIndexType)
        return NULL;

    if (! ctx.consume(COMMA))
        UNEXPECTED(ctx, ",");

    Type * pBaseType = parseType(ctx);

    if (! pBaseType)
        return NULL;

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    MapType * pType = ctx.attach(new MapType(pIndexType, pBaseType));

    _ctx.mergeChildren();

    return pType;
}

bool Parser::isTypeName(Context & _ctx, const std::wstring & _name) const {
    const TypeDeclaration * pDecl = _ctx.getType(_name);

    if (pDecl)
        return true;

    const NamedValue * pVar = _ctx.getVariable(_name);

    return pVar ? isTypeVariable(pVar) : false;
}

NamedReferenceType * Parser::parseTypeReference(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.is(IDENTIFIER))
        UNEXPECTED(ctx, "identifier");

    NamedReferenceType * pType = NULL;

    const std::wstring & str = ctx.scan();
    const TypeDeclaration * pDecl = ctx.getType(str);

    DEBUG(L"%ls %d", str.c_str(), (pDecl != NULL));

    if (pDecl == NULL)
        ERROR(ctx, NULL, L"Unknown type identifier: %ls", str.c_str());

    assert(pDecl != NULL);

//    if (! pDecl) {
//        const NamedValue * pVar = ctx.getVariable(str);
//
//        if (isTypeVariable(pVar))
//            pType = ctx.attach(new NamedReferenceType(pVar));
//        else
//            ERROR(ctx, NULL, L"Unknown type identifier: %ls", str.c_str());
//    } else
        pType = ctx.attach(new NamedReferenceType(pDecl));

    if (pDecl != NULL && pDecl->getType() && pDecl->getType()->hasParameters() && ctx.is(LPAREN)) {
        if (! parseActualParameterList(ctx, ((NamedReferenceType *) pType)->getArgs()))
            ERROR(ctx, NULL, L"Garbage in argument list");
    }

    _ctx.mergeChildren();
    return pType;
}

Range * Parser::parseRange(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    Expression * pMin = parseSubexpression(ctx, parseAtom(ctx, RESTRICT_TYPES), 0);

    if (! pMin) return NULL;
    if (! ctx.consume(DOUBLE_DOT))
        UNEXPECTED(ctx, "..");

    Expression * pMax = parseExpression(ctx);

    if (! pMax) return NULL;

    _ctx.mergeChildren();

    return _ctx.attach(new Range(pMin, pMax));
}

StructType * Parser::parseStructType(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(STRUCT))
        UNEXPECTED(ctx, "struct");

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    StructType * pType = ctx.attach(new StructType());

    if (! parseParamList(ctx, pType->getFields(), & Parser::parseVariableName, ALLOW_EMPTY_NAMES))
        return NULL;

    DEBUG(L"%d", pType->getFields().size());

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    _ctx.mergeChildren();

    return pType;
}

UnionType * Parser::parseUnionType(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(UNION))
        UNEXPECTED(ctx, "union");

    UnionType * pType = ctx.attach(new UnionType());

    if (! parseList(ctx, pType->getConstructors(), & Parser::parseConstructorDeclaration,
            LPAREN, RPAREN, COMMA))
        return NULL;

    for (size_t i = 0; i < pType->getConstructors().size(); ++ i) {
        pType->getConstructors().get(i)->setOrdinal(i);
        pType->getConstructors().get(i)->setUnion(pType);
    }

    _ctx.mergeChildren();

    return pType;
}

EnumType * Parser::parseEnumType(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(ENUM))
        UNEXPECTED(ctx, "enum");

    EnumType * pType = ctx.attach(new EnumType());

    if (! parseList(ctx, pType->getValues(), & Parser::parseEnumValue,
            LPAREN, RPAREN, COMMA))
        return NULL;

    for (size_t i = 0; i < pType->getValues().size(); ++ i) {
        pType->getValues().get(i)->setType(pType, false);
        pType->getValues().get(i)->setOrdinal(i);
    }

    _ctx.mergeChildren();

    return pType;
}

Subtype * Parser::parseSubtype(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(SUBTYPE))
        UNEXPECTED(ctx, "subtype");

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    NamedValue * pVar = parseNamedValue(ctx);

    if (! pVar)
        return NULL;

    if (! ctx.consume(COLON))
        UNEXPECTED(ctx, ":");

    Expression * pExpr = parseExpression(ctx);

    if (! pExpr)
        return NULL;

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    Subtype * pType = ctx.attach(new Subtype(pVar, pExpr));

    _ctx.mergeChildren();

    return pType;
}

PredicateType * Parser::parsePredicateType(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(PREDICATE))
        UNEXPECTED(ctx, "predicate");

    PredicateType * pType = ctx.attach(new PredicateType());

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    if (! parseParamList(ctx, pType->getInParams(), & Parser::parseParam, ALLOW_EMPTY_NAMES))
        ERROR(ctx, false, L"Failed to parse input parameters");

    branch_map_t branches;

    while (ctx.consume(COLON)) {
        Branch * pBranch = new Branch();

        pType->getOutParams().add(pBranch);
        parseParamList(ctx, * pBranch, & Parser::parseParam, OUTPUT_PARAMS | ALLOW_EMPTY_NAMES);

        for (size_t i = 0; i < pBranch->size(); ++ i)
            pBranch->get(i)->setOutput(true);

        if (ctx.is(HASH) && ctx.nextIn(IDENTIFIER, LABEL, INTEGER)) {
            std::wstring strLabel = ctx.scan(2, 1);
            if (! branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(ctx, false, L"Duplicate branch name \"%ls\"", strLabel.c_str());
            pBranch->setLabel(new Label(strLabel));
        }
    }

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    if (ctx.is(PRE))
        if (! parsePreconditions(ctx, * pType, branches))
            ERROR(ctx, false, L"Failed parsing preconditions");

    if (ctx.is(POST))
        if (! parsePostconditions(ctx, * pType, branches))
            ERROR(ctx, false, L"Failed parsing postconditions");

    _ctx.mergeChildren();

    return pType;
}

Type * Parser::parseType(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    Type * pType = NULL;

    bool bBuiltinType = true;

    switch (ctx.getToken()) {
        case NAT_TYPE:
            pType = ctx.attach(new Type(Type::NAT));
            pType->setBits(ctx.getIntBits());
            ++ ctx;
            break;
        case INT_TYPE:
            pType = ctx.attach(new Type(Type::INT));
            pType->setBits(ctx.getIntBits());
            ++ ctx;
            break;
        case REAL_TYPE:
            pType = ctx.attach(new Type(Type::REAL));
            pType->setBits(ctx.getRealBits());
            ++ ctx;
            break;
        case BOOL_TYPE:   pType = ctx.attach(new Type(Type::BOOL)); ++ ctx; break;
        case CHAR_TYPE:   pType = ctx.attach(new Type(Type::CHAR)); ++ ctx; break;
        case STRING_TYPE: pType = ctx.attach(new Type(Type::STRING)); ++ ctx; break;
        case TYPE:        pType = ctx.attach(new TypeType()); ++ ctx; break;
        case VAR:         pType = ctx.attach(new Type(Type::GENERIC)); ++ ctx; break;

        case STRUCT:
            pType = parseStructType(ctx);
            break;
        case UNION:
            pType = parseUnionType(ctx);
            break;
        case ENUM:
            pType = parseEnumType(ctx);
            break;
        case SUBTYPE:
            pType = parseSubtype(ctx);
            break;
        case SEQ:
            if (! (pType = parseDerivedTypeParameter(++ ctx)))
                return NULL;
            pType = ctx.attach(new SeqType(pType));
            break;
        case SET:
            if (! (pType = parseDerivedTypeParameter(++ ctx)))
                return NULL;
            pType = ctx.attach(new SetType(pType));
            break;
        case LIST:
            if (! (pType = parseDerivedTypeParameter(++ ctx)))
                return NULL;
            pType = ctx.attach(new ListType(pType));
            break;
        case ARRAY:
            pType = parseArrayType(ctx);
            break;
        case MAP:
            pType = parseMapType(ctx);
            break;
        case PREDICATE:
            pType = parsePredicateType(ctx);
            break;
        default:
            bBuiltinType = false;
    }

    const bool bNumeric = pType && (pType->getKind() == Type::NAT ||
            pType->getKind() == Type::INT || pType->getKind() == Type::REAL);

    if (bNumeric && ctx.is(LPAREN, INTEGER, RPAREN))
        pType->setBits(wcstol(ctx.scan(3, 1).c_str(), NULL, 10));

    if (bBuiltinType && ! pType)
        ERROR(ctx, NULL, L"Error parsing type reference");

    if (! pType && ctx.is(IDENTIFIER))
        pType = parseTypeReference(ctx);

    if (! pType)
        pType = parseRange(ctx);

    if (! pType)
        ERROR(ctx, NULL, L"Unexpected token while parsing type reference: %ls", TOK_S(ctx));

    if (ctx.consume(ASTERISK))
        pType = ctx.attach(new OptionalType(pType));

    _ctx.mergeChildren();

    return pType;
}

template<class _Param>
bool Parser::parseParamList(Context & _ctx, Collection<_Param> & _params,
        PARSER_FN(_Param,_parser,int), int _nFlags)
{
    if (_ctx.in(RPAREN, COLON, DOT))
        return true;

    Context & ctx = * _ctx.createChild(false);
    Type * pType = NULL;
    Collection<_Param> params;

    do {
        const bool bNeedType = ! pType
            || ! ctx.is(IDENTIFIER)
            || ! (ctx.nextIn(COMMA, RPAREN, COLON, DOT) ||  ctx.nextIn(SEMICOLON, EQ))
            || ((_nFlags & ALLOW_EMPTY_NAMES) && isTypeName(ctx, ctx.getValue()));

        if (bNeedType) {
            pType = parseType(ctx);
            if (! pType)
                ERROR(ctx, false, L"Type required");
        }

        _Param * pParam = NULL;

        if (! ctx.is(IDENTIFIER)) {
            if (! (_nFlags & ALLOW_EMPTY_NAMES))
                ERROR(ctx, false, L"Identifier required");
            pParam = ctx.attach(new _Param());
        } else
            pParam = (this->*_parser) (ctx, _nFlags);

        if (! pParam)
            ERROR(ctx, false, L"Variable or parameter definition required");

        if (pType->getKind() == Type::TYPE) {
            TypeDeclaration *pDecl = ((TypeType *)pType)->getDeclaration();
            pDecl->setName(pParam->getName());
            _ctx.addType(pDecl);
        }

        pParam->setType(pType, ! pType->getParent());
        params.add(pParam, false);
    } while (ctx.consume(COMMA));

    _params.append(params);
    _ctx.mergeChildren();

    return true;
}

Param * Parser::parseParam(Context & _ctx, int _nFlags) {
    if (! _ctx.is(IDENTIFIER))
        return NULL;

    Context & ctx = * _ctx.createChild(false);

    std::wstring name = ctx.scan();

    Param * pParam = NULL;

    if (ctx.consume(SINGLE_QUOTE)) {
        if (! (_nFlags & OUTPUT_PARAMS))
            ERROR(ctx, NULL, L"Only output parameters can be declared as joined");

        NamedValue * pVar = ctx.getVariable(name, true);

        if (! pVar)
            ERROR(ctx, NULL, L"Parameter '%ls' is not defined.", name.c_str());

        if (pVar->getKind() != NamedValue::PREDICATE_PARAMETER || ((Param *) pVar)->isOutput())
            ERROR(ctx, NULL, L"Identifier '%ls' does not name a predicate input parameter.", name.c_str());

        name += L'\'';
        pParam = ctx.attach(new Param(name));
        pParam->setLinkedParam((Param *) pVar);
        ((Param *) pVar)->setLinkedParam(pParam);
    } else
        pParam = ctx.attach(new Param(name));

    if (ctx.consume(ASTERISK)) {
        if (! (_nFlags & ALLOW_ASTERSK))
            ERROR(ctx, NULL, L"Only input predicate parameters can automatically declare joined output parameters");
        pParam->setLinkedParam(pParam); // Just a mark, should be processed later.
    }

    pParam->setOutput(_nFlags & OUTPUT_PARAMS);

    DEBUG(L"Adding var %ls", name.c_str());

    ctx.addVariable(pParam);
    _ctx.mergeChildren();

    return pParam;
}

NamedValue * Parser::parseNamedValue(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    Type * pType = parseType(ctx);

    if (! pType)
        ERROR(ctx, false, L"Type required");

    if (! ctx.is(IDENTIFIER))
        ERROR(ctx, false, L"Identifier required");

    NamedValue * pVar = ctx.attach(new NamedValue(ctx.scan(), pType));

    _ctx.mergeChildren();
    _ctx.addVariable(pVar);

    return pVar;
}

ElementDefinition * Parser::parseArrayElement(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    ElementDefinition * pElem = ctx.attach(new ElementDefinition());
    Expression * pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected.");

    if (ctx.consume(COLON)) {
        pElem->setIndex(pExpr);
        pExpr = parseExpression(ctx);
    }

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected.");

    pElem->setValue(pExpr);
    _ctx.mergeChildren();

    return pElem;
}

ElementDefinition * Parser::parseMapElement(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    ElementDefinition * pElem = ctx.attach(new ElementDefinition());
    Expression * pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Index expression expected.");

    if (! ctx.consume(COLON))
        UNEXPECTED(ctx, ":");

    pElem->setIndex(pExpr);
    pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Value expression expected.");

    pElem->setValue(pExpr);
    _ctx.mergeChildren();

    return pElem;
}

StructFieldDefinition * Parser::parseFieldDefinition(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    StructFieldDefinition * pField = ctx.attach(new StructFieldDefinition());

    if (ctx.is(IDENTIFIER, COLON))
        pField->setName(ctx.getValue());

    Expression * pExpr = parseExpression(ctx);

    if (! pExpr) {
        if (pField->getName().empty())
            ERROR(ctx, NULL, L"Field name expected.");
        ++ ctx;
    }

    if (ctx.consume(COLON)) {
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

StructFieldDefinition * Parser::parseConstructorField(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    StructFieldDefinition * pField = ctx.attach(new StructFieldDefinition());
    std::wstring strIdent;

    if (ctx.is(IDENTIFIER))
        strIdent = ctx.getValue();

    Expression * pExpr = parseExpression(ctx, RESTRICT_TYPES);

    if (! pExpr) {
        VariableDeclaration * pDecl = parseVariableDeclaration(ctx, LOCAL_VARIABLE);

        if (! pDecl && strIdent.empty())
            ERROR(ctx, NULL, L"Expression, declaration or identifier expected.");

        if (! pDecl) {
            // Unresolved identifier treated as variable declaration.
            pDecl = ctx.attach(new VariableDeclaration(true, ctx.scan()));
            pDecl->getVariable()->setType(new Type(Type::GENERIC));
        }

        if (ctx.getCurrentConstructor())
            ctx.getCurrentConstructor()->getDeclarations().add(pDecl);

        //pExpr = ctx.attach(new VariableReference(& pDecl->getVariable()));
        pField->setField(pDecl->getVariable());
    } else
        pField->setValue(pExpr);

    _ctx.mergeChildren();

    return pField;
}

UnionConstructor * Parser::parseConstructor(Context & _ctx, UnionType * _pUnion) {
    if (! _ctx.is(IDENTIFIER))
        return NULL;

    Context & ctx = * _ctx.createChild(false);
    UnionConstructor * pCons = ctx.attach(new UnionConstructor());
    UnionConstructorDeclaration * pDef = NULL;
    const std::wstring & strName = ctx.scan();

    if (_pUnion)
        pDef = _pUnion->getConstructors().get(_pUnion->getConstructors().findByNameIdx(strName));
    else
        pDef = _ctx.getConstructor(strName);

    if (! pDef)
        ERROR(ctx, NULL, L"Unknown or ambiguous union constructor reference: %ls", strName.c_str());

    pCons->setPrototype(pDef);

    if (ctx.is(LPAREN)) {
        ctx.setCurrentConstructor(pCons);
        if (! parseList(ctx, * pCons, & Parser::parseConstructorField, LPAREN, RPAREN, COMMA))
            ERROR(ctx, NULL, L"Union constructor parameters expected");
    }

    pCons->setType(pDef->getUnion(), false);

    _ctx.mergeChildren();

    return pCons;
}

UnionConstructorDeclaration * Parser::parseConstructorDeclaration(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.is(IDENTIFIER))
        ERROR(ctx, NULL, L"Constructor name expected.");

    UnionConstructorDeclaration * pCons = ctx.attach(new UnionConstructorDeclaration(ctx.scan()));

    if (ctx.consume(LPAREN)) {
        if (! parseParamList(ctx, pCons->getStruct().getFields(), & Parser::parseVariableName))
            return NULL;

        if (! ctx.consume(RPAREN))
            UNEXPECTED(ctx, ")");
    }

    _ctx.mergeChildren();
    _ctx.addConstructor(pCons);

    return pCons;
}


EnumValue * Parser::parseEnumValue(Context & _ctx) {
    if (! _ctx.is(IDENTIFIER))
        return NULL;

    EnumValue * pVar = _ctx.attach(new EnumValue(_ctx.scan()));
    _ctx.addVariable(pVar);

    return pVar;
}

NamedValue * Parser::parseVariableName(Context & _ctx, int) {
    if (! _ctx.is(IDENTIFIER))
        return NULL;

    NamedValue * pVar = _ctx.attach(new NamedValue(_ctx.scan()));
    _ctx.addVariable(pVar);

    return pVar;
}

TypeDeclaration * Parser::parseTypeDeclaration(Context & _ctx) {
    if (! _ctx.is(TYPE, IDENTIFIER))
        return NULL;

    Context * pCtx = _ctx.createChild(false);
    TypeDeclaration * pDecl = pCtx->attach(new TypeDeclaration(pCtx->scan(2, 1)));
    ParameterizedType * pParamType = NULL;

    if (pCtx->consume(LPAREN)) {
        pCtx = pCtx->createChild(true);
        pParamType = new ParameterizedType();
        pDecl->setType(pParamType);
        if (! parseParamList(* pCtx, pParamType->getParams(), & Parser::parseVariableName))
            return NULL;
        if (! pCtx->consume(RPAREN))
            ERROR(* pCtx, NULL, L"Expected \")\", got: %ls", TOK_S(* pCtx));
    }

    _ctx.addType(pDecl); // So that recursive definitions would work.

    if (pCtx->consume(EQ)) {
        Type * pType = parseType(* pCtx);

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

Block * Parser::parseBlock(Context & _ctx) {
    Context * pCtx = _ctx.createChild(false);

    if (! pCtx->consume(LBRACE))
        return NULL;

    Block * pBlock = pCtx->attach(new Block());
    pCtx = pCtx->createChild(true);

    while (! pCtx->is(RBRACE)) {
        bool bNeedSeparator = false;

        if (pCtx->is(PRAGMA)) {
            Context * pCtxNew = parsePragma(* pCtx);
            if (! pCtxNew)
                ERROR(* pCtx, NULL, L"Failed parsing compiler directive");

            if (pCtxNew->is(LBRACE)) {
                Block * pNewBlock = parseBlock(* pCtxNew);
                if (! pNewBlock)
                    ERROR(* pCtxNew, NULL, L"Failed parsing block");
                pBlock->add(pNewBlock);
                pCtx->mergeChildren();
            } else {
                pCtx = pCtxNew;
                bNeedSeparator = true;
            }
        } else {
            Statement * pStmt = parseStatement(* pCtx);

            if (! pStmt)
                ERROR(* pCtx, NULL, L"Error parsing statement");

            if (pCtx->is(DOUBLE_PIPE)) {
                ParallelBlock * pNewBlock = pCtx->attach(new ParallelBlock());
                pNewBlock->add(pStmt);
                while (pCtx->consume(DOUBLE_PIPE)) {
                    Statement * pStmt = parseStatement(* pCtx);
                    if (! pStmt)
                        ERROR(* pCtx, NULL, L"Error parsing parallel statement");
                    pNewBlock->add(pStmt);
                }
                pStmt = pNewBlock;
            }

            pBlock->add(pStmt);
            bNeedSeparator = ! pStmt->isBlockLike() && ! pCtx->in(LBRACE, RBRACE);
        }

        if (bNeedSeparator && ! pCtx->consume(SEMICOLON))
            ERROR(* pCtx, NULL, L"Expected \";\", got: %ls", TOK_S(* pCtx));
    }

    ++ (* pCtx);
    _ctx.mergeChildren();

    return pBlock;
}

Multiassignment * Parser::parseMultiAssignment(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    Multiassignment * pMA = ctx.attach(new Multiassignment());

    if (! parseList(ctx, pMA->getLValues(), & Parser::parseAtom, PIPE, PIPE, COMMA))
        ERROR(ctx, NULL, L"Error parsing list of l-values");

    if (! ctx.consume(EQ))
        UNEXPECTED(ctx, "=");

    if (ctx.is(PIPE)) {
        if (! parseList(ctx, pMA->getExpressions(), & Parser::parseExpression, PIPE, PIPE, COMMA))
            ERROR(ctx, NULL, L"Error parsing list of expression");
    } else {
        Expression * pExpr = parseExpression(ctx);
        if (! pExpr)
            ERROR(ctx, NULL, L"Expression expected");
        pMA->getExpressions().add(pExpr);
    }

    _ctx.mergeChildren();

    return pMA;
}

Switch * Parser::parseSwitch(Context & _ctx) {
    Context & ctx = * _ctx.createChild(true);

    if (! ctx.consume(SWITCH))
        UNEXPECTED(ctx, "switch");

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    VariableDeclaration * pDecl = parseVariableDeclaration(ctx, LOCAL_VARIABLE | ALLOW_INITIALIZATION);
    Expression * pExpr = NULL;

    if (! pDecl) {
        pExpr = parseExpression(ctx);
        if (! pExpr)
            ERROR(ctx, NULL, L"Expression or variable declaration expected");
    }

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    Switch * pSwitch = _ctx.attach(new Switch());

    if (pDecl) {
        pSwitch->setParamDecl(pDecl);
        pExpr = new VariableReference(pDecl->getVariable());
        pExpr->setType(pExpr->getType());
//        pExpr->setType((Type *) resolveBaseType(pExpr->getType()));
        pSwitch->setArg(pExpr, true);
    } else
        pSwitch->setArg(pExpr);

    if (! ctx.consume(LBRACE))
        UNEXPECTED(ctx, "{");

    const Type * pBaseType = NULL; //resolveBaseType(pExpr->getType());
    typedef std::map<std::wstring, size_t> map_union_alts_t;
    map_union_alts_t mapUnionAlts;
    bool bUnion = false;

    // TODO: implement switch for unions.

    if (pBaseType && pBaseType->getKind() == Type::UNION) {
        //assert(false);
        /*UnionType * pUnion = (UnionType *) pBaseType;
        for (size_t i = 0; i < pUnion->getAlternatives().size(); ++ i)
            mapUnionAlts[pUnion->getAlternatives().get(i)->getName()] = i;
        bUnion = true;*/
    }

    while (ctx.in(CASE)) {
        Context & ctxCase = * ctx.createChild(true);
        SwitchCase * pCase = ctxCase.attach(new SwitchCase());
        //Type * pParamType = NULL;

        if (bUnion) {
            assert(false);
            /*ctx.consume(Case);

            if (! ctx.is(Identifier))
                ERROR(ctx, NULL, L"Expected identifier, got: %ls", TOK_S(ctx));

            while (ctx.in(Identifier)) {
                const std::wstring & strAlt = ctx.scan();
                map_union_alts_t::iterator iAlt = mapUnionAlts.find(strAlt);

                if (iAlt == mapUnionAlts.end())
                    ERROR(ctx, NULL, L"Unknown alternative identifier: %ls", strAlt.c_str());

                UnionAlternativeExpr * pRef =
                    new UnionAlternativeExpr((UnionType *) pBaseType, iAlt->second);

                pCase->getExpressions().add(pRef, true);

                //if (! pParamType)
                //    pParamType = ((UnionType *) pBaseType)->getAlternatives().get(iAlt->second)->getType();

                if (! ctx.consume(Comma))
                    break;
            }

            if (! ctx.consume(Colon))
                UNEXPECTED(ctx, ":");*/
        } else {
            if (! parseList(ctxCase, pCase->getExpressions(),
                    & Parser::parseExpression, CASE, COLON, COMMA))
                ERROR(ctxCase, NULL, L"Error parsing list of expressions");
        }

//        Context & ctxBody = * ctx.createChild(true);
        Statement * pStmt = parseStatement(ctxCase);

        if (! pStmt)
            ERROR(ctxCase, NULL, L"Statement required");

        pCase->setBody(pStmt);
        pSwitch->add(pCase);
        ctx.mergeChildren();
//        ctx.mergeChildren();
    }

    if (ctx.is(DEFAULT, COLON)) {
        ctx.skip(2);
        Statement * pStmt = parseStatement(ctx);

        if (! pStmt)
            ERROR(ctx, NULL, L"Statement required");

        pSwitch->setDefault(pStmt);
    }

    if (! ctx.consume(RBRACE))
        UNEXPECTED(ctx, "}");

    _ctx.mergeChildren();

    return pSwitch;
}

If * Parser::parseConditional(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(IF))
        UNEXPECTED(ctx, "if");

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    Expression * pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected");

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    Statement * pStmt = parseStatement(ctx);

    if (! pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    If * pIf = ctx.attach(new If());

    pIf->setArg(pExpr);
    pIf->setBody(pStmt);

    if (ctx.consume(ELSE)) {
        pStmt = parseStatement(ctx);

        if (! pStmt)
            ERROR(ctx, NULL, L"Statement expected");

        pIf->setElse(pStmt);
    }

    _ctx.mergeChildren();

    return pIf;
}

Jump * Parser::parseJump(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(HASH))
        UNEXPECTED(ctx, "#");

    if (! ctx.in(LABEL, IDENTIFIER, INTEGER))
        ERROR(ctx, NULL, L"Label identifier expected");

    std::wstring name = ctx.scan();
    Label * pLabel = ctx.getLabel(name);

    if (! pLabel)
        ERROR(ctx, NULL, L"Unknown label %ls", name.c_str());

    _ctx.mergeChildren();

    return _ctx.attach(new Jump(pLabel));
}

//Statement * Parser::parsePragma(Context & _ctx) {
//    return NULL;
//}

Receive * Parser::parseReceive(Context & _ctx) {
    return NULL;
}

Send * Parser::parseSend(Context & _ctx) {
    return NULL;
}

With * Parser::parseWith(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(WITH))
        UNEXPECTED(ctx, "with");

    With * pWith = ctx.attach(new With());

    if (! parseList(ctx, pWith->getArgs(),
            & Parser::parseExpression, LPAREN, RPAREN, COMMA))
        ERROR(ctx, NULL, L"Error parsing list of expressions");

    Statement * pStmt = parseStatement(ctx);

    if (! pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    pWith->setBody(pStmt);

    _ctx.mergeChildren();

    return pWith;
}

For * Parser::parseFor(Context & _ctx) {
    Context & ctx = * _ctx.createChild(true);

    if (! ctx.consume(FOR))
        UNEXPECTED(ctx, "for");

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    For * pFor = _ctx.attach(new For());

    if (! ctx.in(SEMICOLON)) {
        VariableDeclaration * pDecl = parseVariableDeclaration(ctx, LOCAL_VARIABLE | ALLOW_INITIALIZATION);
        if (! pDecl)
            ERROR(ctx, NULL, L"Variable declaration expected");
        pFor->setIterator(pDecl);
    }

    if (! ctx.consume(SEMICOLON))
        UNEXPECTED(ctx, ";");

    if (! ctx.in(SEMICOLON)) {
        Expression * pExpr = parseExpression(ctx);
        if (! pExpr)
            ERROR(ctx, NULL, L"Expression expected");
        pFor->setInvariant(pExpr);
    }

    if (! ctx.consume(SEMICOLON))
        UNEXPECTED(ctx, ";");

    if (! ctx.is(RPAREN)) {
        Statement * pStmt = parseStatement(ctx);
        if (! pStmt)
            ERROR(ctx, NULL, L"Statement expected");
        pFor->setIncrement(pStmt);
    }

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    Statement * pStmt = parseStatement(ctx);

    if (! pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    pFor->setBody(pStmt);

    _ctx.mergeChildren();

    return pFor;
}

While * Parser::parseWhile(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(WHILE))
        UNEXPECTED(ctx, "while");

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    While * pWhile = ctx.attach(new While());
    Expression * pExpr = parseExpression(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Expression expected");

    pWhile->setInvariant(pExpr);

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    Statement * pStmt = parseStatement(ctx);

    if (! pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    pWhile->setBody(pStmt);

    _ctx.mergeChildren();

    return pWhile;
}

Break * Parser::parseBreak(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(BREAK))
        UNEXPECTED(ctx, "break");

    _ctx.mergeChildren();

    return _ctx.attach(new Break());
}

Statement * Parser::parseAssignment(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    Expression * pLHS = parseAtom(ctx);
    Statement * pStmt = NULL;

    if (! pLHS)
        ERROR(ctx, NULL, L"Error parsing expression");

    if (ctx.consume(EQ)) {
        Expression * pRHS = parseExpression(ctx);

        if (! pRHS)
            ERROR(ctx, NULL, L"Error parsing expression");

        pStmt = ctx.attach(new Assignment(pLHS, pRHS));

        // TODO: RHS's type cannot be derived here actually. Tmp solution until
        // proper type inference gets implemented.
        //if (pLHS->getType() && ! pRHS->getType())
        //    pRHS->setType((Type *) resolveBaseType(pLHS->getType()), false);
    } else if (ctx.consume(COMMA)) {
        Multiassignment * pMA = ctx.attach(new Multiassignment());

        pMA->getLValues().add(pLHS);

        if (! parseList(ctx, pMA->getLValues(), & Parser::parseAtom, -1, EQ, COMMA))
            ERROR(ctx, NULL, L"Error parsing list of l-values");

        if (! parseList(ctx, pMA->getExpressions(), & Parser::parseExpression, -1, -1, COMMA))
            ERROR(ctx, NULL, L"Error parsing list of expression");

        pStmt = pMA;
    } else
        ERROR(ctx, NULL, L"Expected \"=\" or \",\", got: %ls", TOK_S(ctx));

    _ctx.mergeChildren();

    return pStmt;
}

Expression * Parser::parseCallResult(Context & _ctx, VariableDeclaration * & _pDecl) {
    Context * pCtx = _ctx.createChild(false);
    Expression * pExpr = NULL;

    // Try variable declaration.
    Type * pType = parseType(* pCtx);

    if (pType && pCtx->is(IDENTIFIER)) {
        // TODO Need to check (was: don't attach to context, caller is responsible for management of this object.)
        _pDecl = pCtx->attach(new VariableDeclaration(true, pCtx->scan()));
        _pDecl->getVariable()->setType(pType);
        _pDecl->getVariable()->setMutable(false);
        pExpr = pCtx->attach(new VariableReference(_pDecl->getVariable()));
        _ctx.addVariable(_pDecl->getVariable());
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

bool Parser::parseCallResults(Context & _ctx, Call & _call, Collection<Expression> & _list) {
    Context & ctx = * _ctx.createChild(false);
    Expression * pExpr = NULL;
    VariableDeclaration * pDecl = NULL;

    if (ctx.consume(UNDERSCORE))
        pExpr = NULL;
    else if (! (pExpr = parseCallResult(ctx, pDecl)))
        ERROR(ctx, false, L"Error parsing output parameter");

    // It's okay to modify call object since if this function fails parseCall() fails too.
    _list.add(pExpr);
    if (pDecl)
        _call.getDeclarations().add(pDecl);

    while (ctx.consume(COMMA)) {
        if (ctx.consume(UNDERSCORE))
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

Call * Parser::parseCall(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);
    std::wstring name = ctx.getValue();
    Predicate * pPred = ctx.getPredicate(name);
    Expression * pExpr = NULL;

    DEBUG(name.c_str());

    if (pPred) {
        ++ ctx;
        pExpr = ctx.attach(new PredicateReference(pPred));
        pExpr->setType(pPred->getType(), false);
    } else
        pExpr = parseAtom(ctx);

    if (! pExpr)
        ERROR(ctx, NULL, L"Predicate expression expected");

    Call * pCall = ctx.attach(new Call());

    pCall->setPredicate(pExpr);

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    if (! parseList(ctx, pCall->getArgs(), & Parser::parseExpression, -1, -1, COMMA))
        ERROR(ctx, false, L"Failed to parse input parameters");

    typedef std::multimap<std::wstring, CallBranch *> call_branch_map_t;
    call_branch_map_t branches;

    while (ctx.consume(COLON)) {
        CallBranch * pBranch = new CallBranch();

        pCall->getBranches().add(pBranch);

        if (! ctx.in(RPAREN, HASH, COLON)) {
            if (! parseCallResults(ctx, * pCall, * pBranch))
                ERROR(ctx, NULL, L"Failed to parse output parameters");
        }

        if (ctx.is(HASH) && ctx.nextIn(IDENTIFIER, LABEL, INTEGER)) {
            std::wstring strLabel = ctx.scan(2, 1);
            Label * pLabel = ctx.getLabel(strLabel);

            if (pLabel)
                pBranch->setHandler(new Jump(pLabel));
            else
                branches.insert(std::make_pair(strLabel, pBranch));
        }
    }

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    while (ctx.consume(CASE)) {
        if (! ctx.in(LABEL, IDENTIFIER, INTEGER) || ! ctx.nextIs(COLON))
            ERROR(ctx, NULL, L"Label identifier expected");

        typedef call_branch_map_t::iterator I;
        std::pair<I, I> bounds = branches.equal_range(ctx.scan());

        ++ ctx;

        if (bounds.first == bounds.second)
            ERROR(ctx, NULL, L"Label identifier expected");

        Statement * pStmt = parseStatement(ctx);

        if (! pStmt)
            ERROR(ctx, NULL, L"Statement required");

        for (I i = bounds.first; i != bounds.second; ++ i)
            i->second->setHandler(pStmt);
    }

    _ctx.mergeChildren();

    return pCall;
}

Statement * Parser::parseStatement(Context & _ctx) {
    switch (_ctx.getToken()) {
        case PIPE:   return parseMultiAssignment(_ctx);
        case LBRACE: return parseBlock(_ctx);
        case SWITCH: return parseSwitch(_ctx);
        case IF:     return parseConditional(_ctx);
        case HASH:   return parseJump(_ctx);

//        case Pragma: return parsePragma(_ctx);

        case RECEIVE: return parseReceive(_ctx);
        case SEND:    return parseSend(_ctx);
        case WITH:    return parseWith(_ctx);

        case FOR:   return parseFor(_ctx);
        case WHILE: return parseWhile(_ctx);
        case BREAK: return parseBreak(_ctx);

        case TYPE: return parseTypeDeclaration(_ctx);

        case PREDICATE:
        CASE_BUILTIN_TYPE: {
            Collection<VariableDeclaration> decls;

            parseParamList(_ctx, decls, &Parser::parseVariableDeclaration,
                    LOCAL_VARIABLE | ALLOW_INITIALIZATION | PART_OF_LIST);

            if (decls.empty())
                return NULL;

            if (decls.size() == 1)
                return decls.get(0);

            // decls.size() > 1
            ParallelBlock *pBlock = new ParallelBlock();

            pBlock->append(decls, true);
            _ctx.mergeChildren();

            return pBlock;
        }
    }

    Statement * pStmt = NULL;

    if (_ctx.in(IDENTIFIER, LABEL, INTEGER) && _ctx.nextIs(COLON)) {
        Context & ctx = * _ctx.createChild(false);
        // A label.
        Label * pLabel = ctx.attach(new Label(ctx.scan(2)));

        if (ctx.is(RBRACE))
            pStmt = ctx.attach(new Statement());
        else
            pStmt = parseStatement(ctx);

        if (! pStmt) return NULL;

        if (pStmt->getLabel()) {
            // We should somehow make two labels to point to the same statement.
            Block * pBlock = ctx.attach(new Block());
            pBlock->add(pStmt);
            pStmt = pBlock;
        }

        pStmt->setLabel(pLabel);
        ctx.addLabel(pLabel);
        _ctx.mergeChildren();
        return pStmt;
    }

    DEBUG(L"Stmt start: %ls", _ctx.getValue().c_str());

    if (_ctx.getType(_ctx.getValue()))
        return parseVariableDeclaration(_ctx, LOCAL_VARIABLE | ALLOW_INITIALIZATION);

    DEBUG(L"Not a type: %ls", _ctx.getValue().c_str());

    Context & ctx = * _ctx.createChild(false);

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

Process * Parser::parseProcess(Context & _ctx) {
    Context & ctx = * _ctx.createChild(true);

    if (! ctx.consume(PROCESS))
        UNEXPECTED(ctx, "process");

    if (! ctx.is(IDENTIFIER))
        ERROR(ctx, NULL, L"Identifier expected");

    Process * pProcess = _ctx.attach(new Process(ctx.scan()));

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    if (! parseParamList(ctx, pProcess->getInParams(), & Parser::parseParam, 0))
        ERROR(ctx, false, L"Failed to parse input parameters");

    branch_map_t branches;

    while (ctx.consume(COLON)) {
        Branch * pBranch = new Branch();

        pProcess->getOutParams().add(pBranch);
        parseParamList(ctx, * pBranch, & Parser::parseParam, OUTPUT_PARAMS);

        for (size_t i = 0; i < pBranch->size(); ++ i)
            pBranch->get(i)->setOutput(true);

        if (ctx.is(HASH) && ctx.nextIn(IDENTIFIER, LABEL, INTEGER)) {
            std::wstring strLabel = ctx.scan(2, 1);
            if (! branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(ctx, false, L"Duplicate branch name \"%ls\"", strLabel.c_str());
            pBranch->setLabel(new Label(strLabel));
        }
    }

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    Block * pBlock = parseBlock(ctx);

    if (! pBlock)
        ERROR(ctx, false, L"Failed parsing process body");

    pProcess->setBlock(pBlock);

    _ctx.mergeChildren();
    _ctx.addProcess(pProcess);

    return pProcess;
}

FormulaDeclaration * Parser::parseFormulaDeclaration(Context & _ctx) {
    if (! _ctx.is(FORMULA, IDENTIFIER))
        return NULL;

    Context * pCtx = _ctx.createChild(false);
    FormulaDeclaration * pDecl = pCtx->attach(new FormulaDeclaration(pCtx->scan(2, 1)));

    if (pCtx->consume(LPAREN) && ! pCtx->consume(RPAREN)) {
        pCtx = pCtx->createChild(true);
        if (! parseParamList(* pCtx, pDecl->getParams(), & Parser::parseVariableName))
            return NULL;
        if (! pCtx->consume(RPAREN))
            UNEXPECTED(* pCtx, ")");
    }

    if (pCtx->consume(EQ)) {
        Expression * pFormula = parseExpression(* pCtx, ALLOW_FORMULAS);

        if (! pFormula)
            return NULL;

        pDecl->setFormula(pFormula);
    }

    _ctx.mergeChildren();
    _ctx.addFormula(pDecl);

    return pDecl;
}

Context * Parser::parsePragma(Context & _ctx) {
    Context & ctx = * _ctx.createChild(false);

    if (! ctx.consume(PRAGMA))
        UNEXPECTED(ctx, "pragma");

    if (! ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    do {
        if (! ctx.is(IDENTIFIER))
            ERROR(ctx, NULL, L"Pragma name expected");

        std::wstring name = ctx.scan();

        if (! ctx.consume(COLON))
            UNEXPECTED(ctx, ":");

        if (name == L"int_bitness") {
            int nBitness = 0;
            if (ctx.is(INTEGER)) {
                nBitness = wcstol(ctx.scan().c_str(), NULL, 10);
                if (nBitness < 1 || nBitness > 64)
                    ERROR(ctx, NULL, L"Integer bitness out of range: %d", nBitness);
            } else if (ctx.is(IDENTIFIER)) {
                std::wstring strBitness = ctx.scan();
                if (strBitness == L"native")
                    nBitness = Number::NATIVE;
                else if (strBitness == L"unbounded")
                    nBitness = Number::GENERIC;
                else
                    ERROR(ctx, NULL, L"Unknown bitness value: %ls", strBitness.c_str());
            }
            ctx.getPragma().setIntBitness(nBitness);
        } else if (name == L"real_bitness") {
            int nBitness = 0;
            if (ctx.is(INTEGER)) {
                nBitness = wcstol(ctx.scan().c_str(), NULL, 10);
                if (nBitness != 32 && nBitness != 64 && nBitness != 128)
                    ERROR(ctx, NULL, L"Real bitness out of range: %d", nBitness);
            } else if (ctx.is(IDENTIFIER)) {
                std::wstring strBitness = ctx.scan();
                if (strBitness == L"native")
                    nBitness = Number::NATIVE;
                else if (strBitness == L"unbounded")
                    nBitness = Number::GENERIC;
                else
                    ERROR(ctx, NULL, L"Unknown bitness value: %ls", strBitness.c_str());
            }
            ctx.getPragma().setRealBitness(nBitness);
        } else if (name == L"overflow") {
            if (ctx.consume(HASH)) {
                if (! ctx.in(LABEL, IDENTIFIER, INTEGER))
                    ERROR(ctx, NULL, L"Label identifier expected");

                std::wstring strLabel = ctx.scan();
                Label * pLabel = ctx.getLabel(strLabel);

                if (! pLabel)
                    ERROR(ctx, NULL, L"Unknown label %ls", strLabel.c_str());

                ctx.getPragma().overflow().set(Overflow::RETURN, pLabel);
            } else if (ctx.is(IDENTIFIER)) {
                std::wstring strOverflow = ctx.scan();
                int nOverflow = Overflow::WRAP;

                if (strOverflow == L"wrap") {
                    nOverflow = Overflow::WRAP;
                } else if (strOverflow == L"saturate") {
                    nOverflow = Overflow::SATURATE;
                } else if (strOverflow == L"strict") {
                    nOverflow = Overflow::STRICT;
                } else
                    ERROR(ctx, NULL, L"Unknown overflow value: %ls", strOverflow.c_str());

                ctx.getPragma().overflow().set(nOverflow);
            }
            ctx.getPragma().set(Pragma::Overflow, true);
        } else
            ERROR(ctx, NULL, L"Unknown pragma %ls", name.c_str());
    } while (ctx.consume(COMMA));

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    return & ctx;
}

void Parser::typecheck(Context &_ctx, Node &_node) {
    if (Options::instance().typeCheck == TC_NONE)
        return;

    tc::Formulas constraints;
    tc::FreshTypes freshTypes;

    tc::collect(constraints, _node, _ctx, freshTypes);
    prettyPrint(constraints, std::wcout);

    if (tc::solve(constraints))
        tc::apply(constraints, freshTypes);
}

bool Parser::parseDeclarations(Context & _ctx, Module & _module) {
    Context * pCtx = _ctx.createChild(false);

    while (!pCtx->in(END_OF_FILE, RBRACE)) {
        switch (pCtx->getToken()) {
            case IDENTIFIER: {
                if (! pCtx->getType(pCtx->getValue())) {
                    Predicate * pPred = parsePredicate(* pCtx);
                    if (! pPred)
                        ERROR(* pCtx, false, L"Failed parsing predicate");
                    _module.getPredicates().add(pPred);
                    typecheck(*pCtx, *pPred);
                     break;
                }
                // no break;
            }
            CASE_BUILTIN_TYPE:
            case MUTABLE: {
                VariableDeclaration * pDecl = parseVariableDeclaration(* pCtx, ALLOW_INITIALIZATION);
                if (! pDecl)
                    ERROR(* pCtx, false, L"Failed parsing variable declaration");
                if (! pCtx->consume(SEMICOLON))
                    ERROR(* pCtx, false, L"Semicolon expected");
                _module.getVariables().add(pDecl);
                typecheck(*pCtx, *pDecl);
                break;
            }
            case TYPE: {
                TypeDeclaration * pDecl = parseTypeDeclaration(* pCtx);
                if (! pDecl)
                    ERROR(* pCtx, false, L"Failed parsing type declaration");
                if (! pCtx->consume(SEMICOLON))
                    ERROR(* pCtx, false, L"Semicolon expected");
                _module.getTypes().add(pDecl);
                 break;
            }
            case PROCESS: {
                Process * pProcess = parseProcess(* pCtx);
                if (! pProcess)
                    ERROR(* pCtx, false, L"Failed parsing process declaration");
                _module.getProcesses().add(pProcess);
                 break;
            }
            case FORMULA: {
                FormulaDeclaration * pFormula = parseFormulaDeclaration(* pCtx);
                if (! pFormula)
                    ERROR(* pCtx, false, L"Failed parsing formula declaration");
                _module.getFormulas().add(pFormula);
                 break;
            }
            case LEMMA: {
                ++*pCtx;
                LemmaDeclaration *pLemma = pCtx->attach(new LemmaDeclaration());
                Expression *pProposition = parseExpression(*pCtx, ALLOW_FORMULAS);

                if (pProposition == NULL)
                    ERROR(*pCtx, false, L"Failed parsing lemma declaration");

                pLemma->setProposition(pProposition);
                _module.getLemmas().add(pLemma);
                 break;
            }
            case PRAGMA: {
                Context * pCtxNew = parsePragma(* pCtx);
                if (! pCtxNew)
                    ERROR(* pCtx, false, L"Failed parsing compiler directive");

                if (pCtxNew->consume(SEMICOLON)) {
                    pCtx = pCtxNew;
                    break;
                } else if (pCtxNew->consume(LBRACE)) {
                    if (! parseDeclarations(* pCtxNew, _module))
                        ERROR(* pCtxNew, false, L"Failed parsing declarations");
                    if (! pCtxNew->consume(RBRACE))
                        ERROR(* pCtxNew, false, L"Closing brace expected");
                    pCtx->mergeChildren();
                    break;
                } else
                    ERROR(* pCtx, false, L"Semicolon or opening brace expected");
            }
            default:
                return false;
        }

        while (pCtx->consume(SEMICOLON))
            ;
    }

    _ctx.mergeChildren();
    return true;
}

bool Parser::parseModule(Context & _ctx, ir::Module * & _pModule) {
    _pModule = new ir::Module();

    parseModuleHeader(_ctx, _pModule);

    while (_ctx.is(IMPORT))
        if (! parseImport(_ctx, _pModule)) {
            _ctx.fmtError(L"Invalid import statement");
            return false;
        }

    if (_ctx.is(END_OF_FILE) || _ctx.loc() == m_tokens.end())
        return true;

    if (!parseDeclarations(_ctx, * _pModule))
        return false;

    if (!_ctx.is(END_OF_FILE) && _ctx.loc() != m_tokens.end())
        UNEXPECTED(_ctx, "End of file");

    return true;
}

bool parse(Tokens & _tokens, ir::Module * & _pModule) {
    Loc loc = _tokens.begin();
    Parser parser(_tokens);
    Context ctx(loc, true);

    if (! parser.parseModule(ctx, _pModule)) {
/*        Context * pCtx = & ctx;

        while (pCtx->getChild())
            pCtx = pCtx->getChild();

//        std::wcerr << L"Parsing failed at line " << pCtx->loc()->getLine();

        std::wcerr << pCtx->getMessages().size() << std::endl;
*/
        ctx.mergeChildren(true);
        std::wcerr << L"Parsing failed at line " << ctx.loc()->getLine() << std::endl;

        for (StatusMessages::const_iterator i = ctx.getMessages().begin();
            i != ctx.getMessages().end(); ++ i)
        {
            const StatusMessage & msg = * i;
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
