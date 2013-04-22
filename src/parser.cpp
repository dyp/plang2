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
#include "options.h"

#include <iostream>
#include <algorithm>
#include <map>

using namespace ir;
using namespace lexer;

#define PARSER_FN(_Node,_Name,...) Auto<_Node> (Parser::*_Name) (Context &_ctx, ## __VA_ARGS__)

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

#define UNEXPECTED_R(_CTX,_TOK,_RETVAL) \
    do { \
        (_CTX).fmtError(L"Expected %ls, got: %ls", fmtQuote(L##_TOK).c_str(), \
                fmtQuote((_CTX).getValue()).c_str()); \
        (_CTX).fail(); \
        return _RETVAL; \
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
    Parser(Tokens &_tokens) : m_tokens(_tokens) { initOps(); }

    ModulePtr parseModule(Context &_ctx, bool _bTopLevel);
    bool parseImport(Context &_ctx, Module &_module);

    TypePtr parseType(Context &_ctx);
    TypePtr parseDerivedTypeParameter(Context &_ctx);
    ArrayTypePtr parseArrayType(Context &_ctx);
    MapTypePtr parseMapType(Context &_ctx);
    RangePtr parseRange(Context &_ctx);
    NamedReferenceTypePtr parseTypeReference(Context &_ctx);
    StructTypePtr parseStructType(Context &_ctx);
    UnionTypePtr parseUnionType(Context &_ctx);
    EnumTypePtr parseEnumType(Context &_ctx);
    SubtypePtr parseSubtype(Context &_ctx);
    PredicateTypePtr parsePredicateType(Context &_ctx);
    ExpressionPtr parseCastOrTypeReference(Context &_ctx, const TypePtr &_pType);

    TypeDeclarationPtr parseTypeDeclaration(Context &_ctx);
    VariableDeclarationPtr parseVariableDeclaration(Context &_ctx, int _nFlags);
    StatementPtr parseVariableDeclarationGroup(Context &_ctx, int _nFlags = 0);
    FormulaDeclarationPtr parseFormulaDeclaration(Context &_ctx);
    LemmaDeclarationPtr parseLemmaDeclaration(Context &_ctx);
    ExpressionPtr parseExpression(Context &_ctx);
    ExpressionPtr parseSubexpression(Context &_ctx, const ExpressionPtr &_lhs, int _minPrec);
    ExpressionPtr parseAtom(Context &_ctx);
    ExpressionPtr parseComponent(Context &_ctx, Expression &_base);
    FormulaPtr parseFormula(Context &_ctx);
    ArrayIterationPtr parseArrayIteration(Context &_ctx);
    ArrayPartExprPtr parseArrayPart(Context &_ctx, Expression &_base);
    FunctionCallPtr parseFunctionCall(Context &_ctx, Expression &_base);
    BinderPtr parseBinder(Context &_ctx, Expression &_base);
    ReplacementPtr parseReplacement(Context &_ctx, Expression &_base);
    LambdaPtr parseLambda(Context &_ctx);
    bool parsePredicateParamsAndBody(Context &_ctx, AnonymousPredicate &_pred);
    PredicatePtr parsePredicate(Context &_ctx);
    ProcessPtr parseProcess(Context &_ctx);

    StatementPtr parseStatement(Context &_ctx);
    BlockPtr parseBlock(Context &_ctx);
    StatementPtr parseAssignment(Context &_ctx);
    MultiassignmentPtr parseMultiAssignment(Context &_ctx);
    SwitchPtr parseSwitch(Context &_ctx);
    IfPtr parseConditional(Context &_ctx);
    JumpPtr parseJump(Context &_ctx);
    ReceivePtr parseReceive(Context &_ctx);
    SendPtr parseSend(Context &_ctx);
    WithPtr parseWith(Context &_ctx);
    ForPtr parseFor(Context &_ctx);
    WhilePtr parseWhile(Context &_ctx);
    BreakPtr parseBreak(Context &_ctx);
    CallPtr parseCall(Context &_ctx);
    ExpressionPtr parseCallResult(Context &_ctx, VariableDeclarationPtr &_pDecl);
    bool parseCallResults(Context &_ctx, Call &_call, Collection<Expression> &_list);

    Context *parsePragma(Context &_ctx);

    bool parseDeclarations(Context &_ctx, Module &_module);

    // Parameter parsing constants.
    enum {
        ALLOW_EMPTY_NAMES = 0x01,
        OUTPUT_PARAMS = 0x02,
        ALLOW_ASTERSK = 0x04,

        // Variable declarations.
        ALLOW_INITIALIZATION = 0x08,
        LOCAL_VARIABLE = 0x10,
        PART_OF_LIST = 0x20,
        IS_MUTABLE = 0x40,
        SINGLE_TYPE = 0x80,
    };

    // Expression parsing constants.
    enum {
        ALLOW_FORMULAS = 0x01,
        RESTRICT_TYPES = 0x02,
    };

    template<class _Param>
    bool parseParamList(Context &_ctx, Collection<_Param> &_params,
            PARSER_FN(_Param,_parser,int), int _nFlags = 0);

    template<class _Node, class _Base>
    bool parseList(Context &_ctx, Collection<_Node, _Base> &_list, PARSER_FN(_Node,_parser),
            int _startMarker, int _endMarker, int _delimiter, bool _bPreserveContextFlags = false);

    bool parseActualParameterList(Context &_ctx, Collection<Expression> &_exprs) {
        return parseList(_ctx, _exprs, &Parser::parseExpression, LPAREN, RPAREN, COMMA, true);
    }

    bool parseArrayIndices(Context &_ctx, Collection<Expression> &_exprs) {
        return parseList(_ctx, _exprs, &Parser::parseExpression, LBRACKET, RBRACKET, COMMA);
    }

    ParamPtr parseParam(Context &_ctx, int _nFlags = 0);
    NamedValuePtr parseVariableName(Context &_ctx, int _nFlags = 0);
    EnumValuePtr parseEnumValue(Context &_ctx);
    NamedValuePtr parseNamedValue(Context &_ctx);
    ElementDefinitionPtr parseArrayElement(Context &_ctx);
    ElementDefinitionPtr parseMapElement(Context &_ctx);
    StructFieldDefinitionPtr parseFieldDefinition(Context &_ctx);
    StructFieldDefinitionPtr parseConstructorField(Context &_ctx);
    UnionConstructorDeclarationPtr parseConstructorDeclaration(Context &_ctx);
    UnionConstructorPtr parseConstructor(Context &_ctx, const UnionTypePtr &_pUnion);

    template<class _T>
    Auto<_T> findByName(const Collection<_T> &_list, const std::wstring &_name);

    template<class _T>
    size_t findByNameIdx(const Collection<_T> &_list, const std::wstring &_name);

    typedef std::map<std::wstring, BranchPtr> branch_map_t;

    template<class _Pred>
    bool parsePreconditions(Context &_ctx, _Pred &_pred, branch_map_t &_branches);

    template<class _Pred>
    bool parsePostconditions(Context &_ctx, _Pred &_pred, branch_map_t &_branches);

    template<class _Pred>
    bool parseMeasure(Context &_ctx, _Pred &_pred);

private:
    Tokens &m_tokens;
    std::vector<operator_t> m_ops;

    void initOps();
    int getPrecedence(int _token, bool _bExpecColon) const;
    int getUnaryOp(int _token) const;
    int getBinaryOp(int _token) const;

    bool isTypeName(Context &_ctx, const std::wstring &_name) const;
    bool fixupAsteriskedParameters(Context &_ctx, Params &_in, Params &_out);

    bool typecheck(Context &_ctx, Node &_node);
};

template<class _Node, class _Base>
bool Parser::parseList(Context &_ctx, Collection<_Node,_Base> &_list, PARSER_FN(_Node,_parser),
        int _startMarker, int _endMarker, int _delimiter, bool _bPreserveContextFlags)
{
    Context &ctx = *_ctx.createChild(false, _bPreserveContextFlags ? _ctx.getFlags() : 0);

    if (_startMarker >= 0 && !ctx.consume(_startMarker))
        return false;

    Collection<_Node> list;
    Auto<_Node> pNode = (this->*_parser)(ctx);

    if (!pNode)
        return false;

    list.add(pNode);

    while (_delimiter < 0 || ctx.consume(_delimiter)) {
        if (!(pNode = (this->*_parser)(ctx)))
            return false;
        list.add(pNode);
    }

    if (_endMarker >= 0 && !ctx.consume(_endMarker))
        return false;

    for (size_t i = 0; i < list.size(); ++i)
        _list.add(list.get(i));

    _ctx.mergeChildren();

    return true;
}

template<class _T>
Auto<_T> Parser::findByName(const Collection<_T> &_list, const std::wstring &_name) {
    const size_t cIdx = findByNameIdx(_list, _name);
    return cIdx == (size_t) -1 ? _list.get(cIdx) : Auto<_T>();
}

template<class _T>
size_t Parser::findByNameIdx(const Collection<_T> &_list, const std::wstring &_name) {
    for (size_t i = 0; i < _list.size(); ++i)
        if (_list.get(i)->getName() == _name)
            return i;

    return (size_t)-1;
}

ArrayPartExprPtr Parser::parseArrayPart(Context &_ctx, Expression &_base) {
    Context &ctx = *_ctx.createChild(false);
    ArrayPartExprPtr pParts = new ArrayPartExpr();

    if (!parseArrayIndices(ctx, pParts->getIndices()))
        return NULL;

    pParts->setObject(&_base);
    _ctx.mergeChildren();

    return pParts;
}

FunctionCallPtr Parser::parseFunctionCall(Context &_ctx, Expression &_base) {
    Context &ctx = *_ctx.createChild(false);
    FunctionCallPtr pCall = new FunctionCall();

    if (!parseActualParameterList(ctx, pCall->getArgs()))
        return NULL;

    pCall->setPredicate(&_base);
    _ctx.mergeChildren();

    return pCall;
}

BinderPtr Parser::parseBinder(Context &_ctx, Expression &_base) {
    Context &ctx = *_ctx.createChild(false);
    BinderPtr pBinder = new Binder();

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    ExpressionPtr pParam;

    if (!ctx.consume(ELLIPSIS)) {
        if (ctx.consume(UNDERSCORE))
            pParam = NULL;
        else if (!(pParam = parseExpression(ctx)))
            ERROR(ctx, NULL, L"Error parsing expression");

        pBinder->getArgs().add(pParam);

        while (ctx.consume(COMMA)) {
            if (ctx.consume(ELLIPSIS))
                break;
            else if (ctx.consume(UNDERSCORE))
                pParam = NULL;
            else if (!(pParam = parseExpression(ctx)))
                ERROR(ctx, NULL, L"Error parsing expression");

            pBinder->getArgs().add(pParam);
        }
    }

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");


    ExpressionPtr pPredicate = &_base;
    if (_base.getKind() == Expression::PREDICATE) {
        ir::Predicates predicates;
        const std::wstring strName = pPredicate.as<PredicateReference>()->getName();

        _ctx.getPredicates(strName, predicates);
        if (predicates.size() != 1)
            ERROR(ctx, NULL, L"Overloaded binders are not implemented");

        pPredicate = new PredicateReference(strName, predicates.get(0));
    }

    pBinder->setPredicate(pPredicate);
    _ctx.mergeChildren();

    return pBinder;
}

ReplacementPtr Parser::parseReplacement(Context &_ctx, Expression &_base) {
    Context &ctx = *_ctx.createChild(false);
    ExpressionPtr pNewValues = parseExpression(ctx);

    if (!pNewValues)
        ERROR(ctx, NULL, L"Error parsing replacement values");

    if (pNewValues->getKind() != Expression::CONSTRUCTOR)
        ERROR(ctx, NULL, L"Constructor expected");

    ReplacementPtr pExpr = new Replacement();

    pExpr->setObject(&_base);
    pExpr->setNewValues(pNewValues.as<Constructor>());
    _ctx.mergeChildren();

    return pExpr;
}

ExpressionPtr Parser::parseComponent(Context &_ctx, Expression &_base) {
    Context &ctx = *_ctx.createChild(false);
    ExpressionPtr pExpr;

    if (ctx.is(DOT) && ctx.nextIn(LBRACKET, LPAREN, MAP_LBRACKET)) {
        ++ctx;
        pExpr = parseReplacement(ctx, _base);
    } else if (ctx.is(DOT)) {
        const std::wstring &fieldName = ctx.scan(2, 1);
        ComponentPtr pExpr = new FieldExpr(fieldName);

        _ctx.mergeChildren();
        pExpr->setObject(&_base);

        return pExpr;
    } else if (ctx.is(LBRACKET)) {
        pExpr = parseArrayPart(ctx, _base);
    } else if (ctx.is(LPAREN)) {
        if (_base.getKind() == Expression::TYPE) {
            pExpr = parseExpression(ctx);

            if (!pExpr)
                return NULL;

            CastExprPtr pCast = new CastExpr();

            pCast->setToType(ExpressionPtr(&_base).as<TypeExpr>());
            pCast->setExpression(pExpr);
            _ctx.mergeChildren();

            return pCast;
        }

        pExpr = parseFunctionCall(ctx, _base);

        if (!pExpr)
            pExpr = parseBinder(ctx, _base);
    }

    if (!pExpr)
        return NULL;

    _ctx.mergeChildren();

    return pExpr;
}

ArrayIterationPtr Parser::parseArrayIteration(Context &_ctx) {
    Context &ctx = *_ctx.createChild(true);

    if (!ctx.consume(FOR))
        UNEXPECTED(ctx, "for");

    ArrayIterationPtr pArray = new ArrayIteration();

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    if (!parseParamList(ctx, pArray->getIterators(), &Parser::parseVariableName))
        ERROR(ctx, NULL, L"Failed parsing list of iterators");

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    if (ctx.is(LBRACE)) {
        Context &ctxParts = *ctx.createChild(false);

        ++ctxParts;

        while (ctxParts.is(CASE)) {
            ArrayPartDefinitionPtr pPart = new ArrayPartDefinition();

            if (!parseList(ctxParts, pPart->getConditions(),
                    &Parser::parseExpression, CASE, COLON, COMMA))
                ERROR(ctxParts, NULL, L"Error parsing list of expressions");

            ExpressionPtr pExpr = parseExpression(ctxParts);

            if (!pExpr)
                ERROR(ctxParts, NULL, L"Expression required");

            pPart->setExpression(pExpr);
            pArray->add(pPart);
        }

        if (ctxParts.is(DEFAULT, COLON)) {
            ctxParts.skip(2);

            ExpressionPtr pExpr = parseExpression(ctxParts);

            if (! pExpr)
                ERROR(ctxParts, NULL, L"Expression required");

            pArray->setDefault(pExpr);
        }

        if (!pArray->empty() || pArray->getDefault()) {
            if (!ctxParts.consume(RBRACE))
                UNEXPECTED(ctxParts, "}");
            ctx.mergeChildren();
        }
    }

    if (pArray->empty() && !pArray->getDefault()) {
        ExpressionPtr pExpr = parseExpression(ctx);

        if (!pExpr)
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

    m_ops[IMPLIES]    = operator_t(Binary::getPrecedence(Binary::IMPLIES), Binary::IMPLIES);
    m_ops[IFF]        = operator_t(Binary::getPrecedence(Binary::IFF), Binary::IFF);
    m_ops[QUESTION]   = operator_t(Ternary::getPrecedence());
//    m_oPS[COLON]      = operator_t(nPrec);
    m_ops[OR]         = operator_t(Binary::getPrecedence(Binary::BOOL_OR), Binary::BOOL_OR);
    m_ops[XOR]        = operator_t(Binary::getPrecedence(Binary::BOOL_XOR), Binary::BOOL_XOR);
    m_ops[AMPERSAND]  = operator_t(Binary::getPrecedence(Binary::BOOL_AND), Binary::BOOL_AND);
    m_ops[AND]        = operator_t(Binary::getPrecedence(Binary::BOOL_AND), Binary::BOOL_AND);
    m_ops[EQ]         = operator_t(Binary::getPrecedence(Binary::EQUALS), Binary::EQUALS);
    m_ops[NE]         = operator_t(Binary::getPrecedence(Binary::NOT_EQUALS), Binary::NOT_EQUALS);
    m_ops[LT]         = operator_t(Binary::getPrecedence(Binary::LESS), Binary::LESS);
    m_ops[LTE]        = operator_t(Binary::getPrecedence(Binary::LESS_OR_EQUALS), Binary::LESS_OR_EQUALS);
    m_ops[GT]         = operator_t(Binary::getPrecedence(Binary::GREATER), Binary::GREATER);
    m_ops[GTE]        = operator_t(Binary::getPrecedence(Binary::GREATER_OR_EQUALS), Binary::GREATER_OR_EQUALS);
    m_ops[IN]         = operator_t(Binary::getPrecedence(Binary::IN), Binary::IN);
    m_ops[SHIFTLEFT]  = operator_t(Binary::getPrecedence(Binary::SHIFT_LEFT), Binary::SHIFT_LEFT);
    m_ops[SHIFTRIGHT] = operator_t(Binary::getPrecedence(Binary::SHIFT_RIGHT), Binary::SHIFT_RIGHT);
    m_ops[PLUS]       = operator_t(Binary::getPrecedence(Binary::ADD), Binary::ADD, Unary::PLUS);
    m_ops[MINUS]      = operator_t(Binary::getPrecedence(Binary::SUBTRACT), Binary::SUBTRACT, Unary::MINUS);
    m_ops[BANG]       = operator_t(Unary::getPrecedence(Unary::BOOL_NEGATE), -1, Unary::BOOL_NEGATE);
    m_ops[TILDE]      = operator_t(Unary::getPrecedence(Unary::BITWISE_NEGATE), -1, Unary::BITWISE_NEGATE);
    m_ops[ASTERISK]   = operator_t(Binary::getPrecedence(Binary::MULTIPLY), Binary::MULTIPLY);
    m_ops[SLASH]      = operator_t(Binary::getPrecedence(Binary::DIVIDE), Binary::DIVIDE);
    m_ops[PERCENT]    = operator_t(Binary::getPrecedence(Binary::REMAINDER), Binary::REMAINDER);
    m_ops[CARET]      = operator_t(Binary::getPrecedence(Binary::POWER), Binary::POWER);
}

ExpressionPtr Parser::parseCastOrTypeReference(Context &_ctx, const TypePtr &_pType) {
    ExpressionPtr pExpr;
    Context &ctx = *_ctx.createChild(false, RESTRICT_TYPES);

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
                pExpr = parseAtom(ctx);
                if (pExpr)
                    pExpr = new CastExpr(pExpr, new TypeExpr(_pType));
        }
    }

    if (!pExpr)
        pExpr = new TypeExpr(_pType);

    _ctx.mergeChildren();

    return pExpr;
}

LambdaPtr Parser::parseLambda(Context &_ctx) {
    Context &ctx = *_ctx.createChild(true);

    if (!ctx.consume(PREDICATE))
        UNEXPECTED(ctx, "predicate");

    LambdaPtr pLambda = new Lambda();

    if (!parsePredicateParamsAndBody(ctx, pLambda->getPredicate()))
        return NULL;

    if (!pLambda->getPredicate().getBlock())
        ERROR(ctx, NULL, L"No body defined for anonymous predicate");

    _ctx.mergeChildren();

    return pLambda;
}

FormulaPtr Parser::parseFormula(Context &_ctx) {
    Context &ctx = *_ctx.createChild(true, ALLOW_FORMULAS);

    if (!ctx.in(FORALL, EXISTS))
        ERROR(ctx, NULL, L"Quantifier expected");

    FormulaPtr pFormula = new Formula(ctx.in(FORALL) ? Formula::UNIVERSAL : Formula::EXISTENTIAL);

    ++ctx;

    if (!parseParamList(ctx, pFormula->getBoundVariables(), &Parser::parseVariableName, 0))
        ERROR(ctx, NULL, L"Failed parsing bound variables");

    if (!ctx.consume(DOT))
        UNEXPECTED(ctx, ".");

    // OK to use parseExpression instead of parseSubexpression
    // since quantifiers are lowest priority right-associative operators.
    ExpressionPtr pSub = parseExpression(ctx);

    if (!pSub)
        ERROR(ctx, NULL, L"Failed parsing subformula");

    pFormula->setSubformula(pSub);
    _ctx.mergeChildren();

    return pFormula;
}

ExpressionPtr Parser::parseAtom(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false, _ctx.getFlags());
    ExpressionPtr pExpr;
    const bool bAllowTypes = !(ctx.getFlags() & RESTRICT_TYPES);
    int token = ctx.getToken();

    ctx.setFlags(ctx.getFlags() & ~RESTRICT_TYPES);

    if (ctx.is(LPAREN, RPAREN) || ctx.is(LBRACKET, RBRACKET) || ctx.is(LBRACE, RBRACE) ||
            ctx.is(LIST_LBRACKET, LIST_RBRACKET) || ctx.is(MAP_LBRACKET, MAP_RBRACKET))
    {
        ctx.skip(2);
        pExpr = new Literal();
        token = -1;
    }

    switch (token) {
        case INTEGER: {
            Number num(ctx.scan(), Number::INTEGER);
            pExpr = new Literal(num);
            break;
        }
        case REAL:
        case NOT_A_NUMBER:
        case INF: {
            Number num(ctx.scan(), Number::REAL);
            pExpr = new Literal(num);
            break;
        }
        case TRUE:
        case FALSE:
            pExpr = new Literal(ctx.getToken() == TRUE);
            ++ctx;
            break;
        case CHAR:
            pExpr = new Literal(ctx.scan()[0]);
            break;
        case STRING:
            pExpr = new Literal(ctx.scan());
            break;
        case NIL:
            ++ctx;
            pExpr = new Literal();
            break;
        case LPAREN: {
            Context *pCtx = ctx.createChild(false, ctx.getFlags());
            ++(*pCtx);
            pExpr = parseExpression(*pCtx);
            if (!pExpr || !pCtx->consume(RPAREN)) {
                // Try to parse as a struct literal.
                pCtx = ctx.createChild(false);
                StructConstructorPtr pStruct = new StructConstructor();
                if (!parseList(*pCtx, *pStruct, &Parser::parseFieldDefinition, LPAREN, RPAREN, COMMA))
                    ERROR(*pCtx, NULL, L"Expected \")\" or a struct literal");
                pExpr = pStruct;
            }
            ctx.mergeChildren();
            break;
        }
        case LBRACKET:
            pExpr = new ArrayConstructor();
            if (!parseList(ctx, (ArrayConstructor &)*pExpr, &Parser::parseArrayElement,
                    LBRACKET, RBRACKET, COMMA))
                ERROR(ctx, NULL, L"Failed parsing array constructor");
            break;
        case MAP_LBRACKET:
            pExpr = new MapConstructor();
            if (!parseList(ctx, (MapConstructor &)*pExpr, &Parser::parseMapElement,
                    MAP_LBRACKET, MAP_RBRACKET, COMMA))
                ERROR(ctx, NULL, L"Failed parsing map constructor");
            break;
        case LBRACE:
            pExpr = new SetConstructor();
            if (!parseList(ctx, (SetConstructor &)*pExpr, &Parser::parseExpression,
                    LBRACE, RBRACE, COMMA))
                ERROR(ctx, NULL, L"Failed parsing set constructor");
            break;
        case LIST_LBRACKET:
            pExpr = new ListConstructor();
            if (!parseList(ctx, (ListConstructor &)*pExpr, &Parser::parseExpression,
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
                if (TypePtr pType = parseType(ctx))
                    pExpr = parseCastOrTypeReference(ctx, pType);
                else
                    ERROR(ctx, NULL, L"Type reference expected");
            }
            break;
        case FORALL:
        case EXISTS:
            if (ctx.getFlags() & ALLOW_FORMULAS)
                pExpr = parseFormula(ctx);
            break;
    }

    if (!pExpr && ctx.is(IDENTIFIER)) {
        std::wstring str = ctx.getValue();
        Context *pModuleCtx = &ctx;

        // TODO Module args.
        ModulePtr pModule;
        while (!pExpr && (pModule = pModuleCtx->getModule(str)) && ctx.nextIs(DOT)) {
            ctx.skip(2);

            if (!ctx.is(IDENTIFIER))
                ERROR(ctx, NULL, L"Identifier expected");

            Context *pContext = pModuleCtx->getModuleCtx(str);
            assert(pContext != NULL);
            pModuleCtx = pContext;

            str = ctx.getValue();

        }
        Context &moduleCtx = *pModuleCtx;

        NamedValuePtr pVar;
        bool bLinkedIdentifier = false;

        if (ctx.nextIs(SINGLE_QUOTE)) {
            str += L'\'';
            bLinkedIdentifier = true;
        }

        if ((pVar = moduleCtx.getVariable(str)) && (!bAllowTypes || !isTypeVariable(pVar))) {
            pExpr = new VariableReference(pVar);
            ctx.skip(bLinkedIdentifier ? 2 : 1);
        }

        if (bLinkedIdentifier && !pExpr)
            ERROR(ctx, NULL, L"Parameter with name %ls not found", str.c_str());

        if (!pExpr && _ctx.getConstructor(str)) {
            if (moduleCtx.nextIs(QUESTION) && !moduleCtx.nextLoc()->hasLeadingSpace()) {
                assert(false && "Unimplemented");
                // ParseRecognizer?
            } else
                pExpr = parseConstructor(ctx, NULL);
        }

        PredicatePtr pPred;

        if (!pExpr && (pPred = moduleCtx.getPredicate(str))) {
            pExpr = new PredicateReference(str);
            ++ctx;
        }

        FormulaDeclarationPtr pFormula;

        if (!pExpr && (ctx.getFlags() & ALLOW_FORMULAS) && (pFormula = moduleCtx.getFormula(str))) {
            FormulaCallPtr pCall = new FormulaCall();

            ++ctx;

            if (ctx.is(LPAREN, RPAREN))
                ctx.skip(2);
            else if (!parseActualParameterList(ctx, pCall->getArgs()))
                return NULL;

            pCall->setTarget(pFormula);
            pExpr = pCall;
        }

        TypePtr pRealType;

        if (!pExpr) {
            if (TypeDeclarationPtr pTypeDecl = moduleCtx.getType(str))
                pRealType = pTypeDecl->getType();
        }

        if (!pExpr && pRealType && pRealType->getKind() == Type::UNION && ctx.nextIs(DOT, IDENTIFIER)) {
            // It's ok since we always know the UnionType in UnionType.ConstructorName expression even
            // before type inference.
            ctx.skip(2);
            pExpr = parseConstructor(ctx, pRealType.as<UnionType>());
            if (!pExpr)
                return NULL;
        }

        if (!pExpr && bAllowTypes) {
            if (TypePtr pType = parseType(ctx))
                pExpr = parseCastOrTypeReference(ctx, pType);
        }

        if (!pExpr)
            ERROR(ctx, NULL, L"Unknown identifier: %ls", str.c_str());
    }

    // Other things should be implemented HERE.

    if (pExpr)
        while (ExpressionPtr pCompound = parseComponent(ctx, *pExpr)) {
            pExpr = pCompound;
            pCompound = parseComponent(ctx, *pExpr);
        }

    if (pExpr && bAllowTypes && ctx.consume(DOUBLE_DOT)) {
        // Can be a range.
        ExpressionPtr pMax = parseExpression(ctx);

        if (!pMax)
            return NULL;

        pExpr = new TypeExpr(new Range(pExpr, pMax));
    }

    if (!pExpr)
        ERROR(ctx, NULL, L"Unexpected token while parsing expression: %ls", TOK_S(ctx));

    _ctx.mergeChildren();

    return pExpr;
}

ExpressionPtr Parser::parseSubexpression(Context &_ctx, const ExpressionPtr &_pLhs, int _minPrec)
{
    Context &ctx = *_ctx.createChild(false, _ctx.getFlags());
    bool bParseElse = false;
    ExpressionPtr pLhs = _pLhs;

    while (!pLhs || getPrecedence(ctx.getToken(), bParseElse) >= _minPrec) {
        const int op = ctx.getToken();
        ExpressionPtr pRhs;
        int nPrec = std::max(_minPrec, getPrecedence(op, bParseElse));

        ++ctx;

        const int tokRHS = ctx.getToken();

        if (getUnaryOp(tokRHS) >= 0) {
            pRhs = parseSubexpression(ctx, NULL, nPrec + 1);
        } else {
            pRhs = parseAtom(ctx);
        }

        if (!pRhs)
            return NULL;

        while (getPrecedence(ctx.getToken(), bParseElse) > nPrec) {
            if (ExpressionPtr rhsNew =
                    parseSubexpression(ctx, pRhs, getPrecedence(ctx.getToken(), bParseElse)))
                pRhs = rhsNew;
            else
                return NULL;
        }

        if (bParseElse) {
            if (op != COLON)
                ERROR(ctx, NULL, L"\":\" expected");
            pLhs.as<Ternary>()->setElse(pRhs);
            bParseElse = false;
        } else if (op == QUESTION) {
            if (!pLhs)
                return NULL;
            bParseElse = true;
            pLhs = new Ternary(pLhs, pRhs);
        } else if (!pLhs) {
            const int unaryOp = getUnaryOp(op);

            if (unaryOp < 0)
                ERROR(ctx, NULL, L"Unary operator expected");

            if (tokRHS != LPAREN && // Disable optimization of "-(NUMBER)" expressions for now.
                    pRhs->getKind() == Expression::LITERAL &&
                    pRhs.as<Literal>()->getLiteralKind() == Literal::NUMBER)
            {
                // Ok, handle unary plus/minus here.
                if (unaryOp == Unary::MINUS) {
                    Number num = pRhs.as<Literal>()->getNumber();
                    num.negate();
                    pRhs.as<Literal>()->setNumber(num);
                }
                if (unaryOp == Unary::MINUS || unaryOp == Unary::PLUS) {
                    pLhs = pRhs;
                    continue;
                }
            }

            pLhs = new Unary(unaryOp, pRhs);
            pLhs.as<Unary>()->getOverflow().set(_ctx.getOverflow());
        } else {
            const int binaryOp = getBinaryOp(op);

            if (binaryOp < 0)
                ERROR(ctx, NULL, L"Binary operator expected");

            pLhs = new Binary(binaryOp, pLhs, pRhs);
            pLhs.as<Binary>()->getOverflow().set(_ctx.getOverflow());
        }
    }

    _ctx.mergeChildren();

    return pLhs;
}

ExpressionPtr Parser::parseExpression(Context &_ctx) {
    ExpressionPtr pExpr = parseAtom(_ctx);

    if (!pExpr) {
        Context * pCtx = _ctx.getChild();

        _ctx.setChild(NULL);
        pExpr = parseSubexpression(_ctx, pExpr, 0);

        // Restore context if parsing failed.
        if (!pExpr && pCtx != NULL && !pCtx->getMessages().empty())
            _ctx.setChild(pCtx);
        else
            delete pCtx;

        return pExpr;
    }

    return parseSubexpression(_ctx, pExpr, 0);
}

template<class _Pred>
bool Parser::parsePreconditions(Context &_ctx, _Pred &_pred, branch_map_t &_branches) {
    if (!_ctx.consume(PRE))
        return false;

    Context &ctx = *_ctx.createChild(false, ALLOW_FORMULAS);

    branch_map_t::iterator iBranch = _branches.end();

    if (ctx.in(LABEL, IDENTIFIER, INTEGER) && ctx.nextIs(COLON))
        iBranch = _branches.find(ctx.getValue());

    if (iBranch != _branches.end()) {
        BranchPtr pBranch = iBranch->second;

        ctx.skip(2);

        if (ExpressionPtr pFormula = parseExpression(ctx)) {
            if (pFormula->getKind() == Expression::FORMULA)
                pBranch->setPreCondition(pFormula.as<Formula>());
            else
                pBranch->setPreCondition(new Formula(Formula::NONE, pFormula));
        } else
            ERROR(ctx, false, L"Formula expected");
    } else {
        if (ExpressionPtr pFormula = parseExpression(ctx)) {
            if (pFormula->getKind() == Expression::FORMULA)
                _pred.setPreCondition(pFormula.as<Formula>());
            else
                _pred.setPreCondition(new Formula(Formula::NONE, pFormula));
        } else
            ERROR(ctx, false, L"Formula expected");
    }

    while (ctx.consume(PRE)) {
        BranchPtr pBranch = _branches[ctx.getValue()];

        if (!ctx.in(LABEL, IDENTIFIER, INTEGER) || !ctx.nextIs(COLON) || !pBranch)
            ERROR(ctx, false, L"Branch name expected");

        ctx.skip(2);

        if (ExpressionPtr pFormula = parseExpression(ctx)) {
            if (pFormula->getKind() != Expression::FORMULA)
                pFormula = new Formula(Formula::NONE, pFormula);
            pBranch->setPreCondition(pFormula.as<Formula>());
        } else
            ERROR(ctx, NULL, L"Formula expected");
    }

    _ctx.mergeChildren();

    return true;
}

template<class _Pred>
bool Parser::parsePostconditions(Context &_ctx, _Pred &_pred, branch_map_t &_branches) {
    if (!_ctx.is(POST))
        return false;

    Context &ctx = *_ctx.createChild(false, ALLOW_FORMULAS);

    if (_pred.isHyperFunction()) {
        while (ctx.consume(POST)) {
            BranchPtr pBranch = _branches[ctx.getValue()];

            if (!ctx.in(LABEL, IDENTIFIER, INTEGER) || !ctx.nextIs(COLON) || !pBranch)
                ERROR(ctx, false, L"Branch name expected");

            ctx.skip(2);

            if (ExpressionPtr pFormula = parseExpression(ctx)) {
                if (pFormula->getKind() != Expression::FORMULA)
                    pFormula = new Formula(Formula::NONE, pFormula);
                pBranch->setPostCondition(pFormula.as<Formula>());
            } else
                ERROR(ctx, NULL, L"Formula expected");
        }
    } else if (ctx.consume(POST)) {
        if (ExpressionPtr pFormula = parseExpression(ctx)) {
            if (pFormula->getKind() != Expression::FORMULA)
                pFormula = new Formula(Formula::NONE, pFormula);
            _pred.setPostCondition(pFormula.as<Formula>());
        } else
            ERROR(ctx, false, L"Formula expected");
    }

    _ctx.mergeChildren();

    return true;
}

template<class _Pred>
bool Parser::parseMeasure(Context &_ctx, _Pred &_pred) {
    if (!_ctx.consume(MEASURE))
        return false;

    ExpressionPtr pMeasure = parseExpression(_ctx);

    if (!pMeasure)
        ERROR(_ctx, false, L"Expression expected");

    _pred.setMeasure(pMeasure);

    return true;
}

bool Parser::fixupAsteriskedParameters(Context &_ctx, Params &_in, Params &_out) {
    bool bResult = false;

    for (size_t i = 0; i < _in.size(); ++i) {
        ParamPtr pInParam = _in.get(i);

        if (pInParam->getLinkedParam() != pInParam)
            continue;

        const std::wstring name = pInParam->getName() + L'\'';
        ParamPtr pOutParam = new Param(name);

        _out.add(pOutParam);
        pInParam->setLinkedParam(pOutParam);
        pOutParam->setLinkedParam(pInParam);
        pOutParam->setType(pInParam->getType());
        pOutParam->setOutput(true);
        bResult = true;
        _ctx.addVariable(pOutParam);
    }

    return bResult;
}

bool Parser::parsePredicateParamsAndBody(Context &_ctx, AnonymousPredicate &_pred) {
    if (!_ctx.consume(LPAREN))
        ERROR(_ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(_ctx));

    if (!parseParamList(_ctx, _pred.getInParams(), &Parser::parseParam, ALLOW_ASTERSK | ALLOW_EMPTY_NAMES))
        ERROR(_ctx, false, L"Failed to parse input parameters");

    branch_map_t branches;
    bool bHasAsterisked = false;

    while (_ctx.consume(COLON)) {
        BranchPtr pBranch = new Branch();

        _pred.getOutParams().add(pBranch);

        if (_pred.getOutParams().size() == 1)
            bHasAsterisked = fixupAsteriskedParameters(_ctx, _pred.getInParams(), *pBranch);
        else if (bHasAsterisked)
            ERROR(_ctx, false, L"Hyperfunctions cannot use '*' in parameter list");

        parseParamList(_ctx, *pBranch, &Parser::parseParam, OUTPUT_PARAMS | ALLOW_EMPTY_NAMES);

        for (size_t i = 0; i < pBranch->size(); ++i)
            pBranch->get(i)->setOutput(true);

        if (_ctx.is(HASH) && _ctx.nextIn(IDENTIFIER, LABEL, INTEGER)) {
            ++_ctx;

            std::wstring strLabel = _ctx.getValue();

            if (_ctx.is(INTEGER) && wcstoul(strLabel.c_str(), NULL, 10) != _pred.getOutParams().size())
                ERROR(_ctx, false, L"Numbers of numeric branch labels should correspond to branch order");

            ++_ctx;

            if (!branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(_ctx, false, L"Duplicate branch name \"%ls\"", strLabel.c_str());

            pBranch->setLabel(_ctx.createLabel(strLabel));
        }
    }

    if (_pred.getOutParams().empty()) {
        BranchPtr pBranch = new Branch();

        _pred.getOutParams().add(pBranch);
        fixupAsteriskedParameters(_ctx, _pred.getInParams(), *pBranch);
    }

    // Create labels for unlabeled branches.
    if (_pred.getOutParams().size() > 1) {
        for (size_t i = 0; i < _pred.getOutParams().size(); ++ i) {
            BranchPtr pBranch = _pred.getOutParams().get(i);

            if (!pBranch->getLabel()) {
                pBranch->setLabel(new Label(fmtInt(i + 1)));
                _ctx.addLabel(pBranch->getLabel());
                branches[pBranch->getLabel()->getName()] = pBranch;
            }
        }
    }

    if (!_ctx.consume(RPAREN))
        ERROR(_ctx, false, L"Expected \")\", got: %ls", TOK_S(_ctx));

    if (_ctx.is(PRE))
        if (!parsePreconditions(_ctx, _pred, branches))
            ERROR(_ctx, false, L"Failed parsing preconditions");

    if (_ctx.is(LBRACE)) {
        if (BlockPtr pBlock = parseBlock(_ctx))
            _pred.setBlock(pBlock);
        else
            ERROR(_ctx, false, L"Failed parsing predicate body");
    }

    if (_ctx.is(POST))
        if (!parsePostconditions(_ctx, _pred, branches))
            ERROR(_ctx, false, L"Failed parsing postconditions");

    if (_ctx.is(MEASURE))
        if (!parseMeasure(_ctx, _pred))
            ERROR(_ctx, false, L"Failed parsing measure");

    return true;
}

PredicatePtr Parser::parsePredicate(Context &_ctx) {
    Context *pCtx = _ctx.createChild(false);

    if (!pCtx->is(IDENTIFIER))
        return NULL;

    PredicatePtr pPred = new Predicate(pCtx->scan());

    pCtx->addPredicate(pPred);
    pCtx = pCtx->createChild(true);

    if (!parsePredicateParamsAndBody(*pCtx, *pPred))
        return NULL;

    if (!pPred->getBlock() && !pCtx->consume(SEMICOLON))
        ERROR(*pCtx, NULL, L"Expected block or a semicolon");

    _ctx.mergeChildren();

    return pPred;
}

VariableDeclarationPtr Parser::parseVariableDeclaration(Context &_ctx, int _nFlags) {
    Context &ctx = *_ctx.createChild(false);
    bool bMutable = (_nFlags & IS_MUTABLE);
    TypePtr pType;

    if ((_nFlags & PART_OF_LIST) == 0) {
        bMutable |= ctx.consume(MUTABLE);
        pType = parseType(ctx);

        if (!pType)
            return NULL;
    }

    if (!ctx.is(IDENTIFIER))
        ERROR(ctx, NULL, L"Expected identifier, got: %ls", TOK_S(ctx));

    VariableDeclarationPtr pDecl = new VariableDeclaration(_nFlags & LOCAL_VARIABLE, ctx.scan());

    if ((_nFlags & PART_OF_LIST) == 0)
        pDecl->getVariable()->setType(pType);

    pDecl->getVariable()->setMutable(bMutable);

    if ((_nFlags & ALLOW_INITIALIZATION) && ctx.consume(EQ)) {
        if (ExpressionPtr pExpr = parseExpression(ctx))
            pDecl->setValue(pExpr);
        else
            return NULL;
    }

    _ctx.mergeChildren();
    _ctx.addVariable(pDecl->getVariable());

    return pDecl;
}

bool Parser::parseImport(Context &_ctx, Module &_module) {
    if (_ctx.is(IMPORT, IDENTIFIER, SEMICOLON)) {
        _module.getImports().push_back(_ctx.scan(3, 1));
        return true;
    }

    return false;
}

TypePtr Parser::parseDerivedTypeParameter(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(LPAREN))
        ERROR(ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(ctx));

    TypePtr pType = parseType(ctx);

    if (!pType)
        return NULL;

    if (!ctx.consume(RPAREN))
        ERROR(ctx, NULL, L"Expected \")\", got: %ls", TOK_S(ctx));

    _ctx.mergeChildren();

    return pType;
}

ArrayTypePtr Parser::parseArrayType(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(ARRAY))
        ERROR(ctx, NULL, L"Expected \"array\", got: %ls", TOK_S(ctx));

    if (!ctx.consume(LPAREN))
        ERROR(ctx, NULL, L"Expected \"(\", got: %ls", TOK_S(ctx));

    TypePtr pBaseType = parseType(ctx);
    if (!pBaseType)
        return NULL;

    ArrayTypePtr
        pArray = new ArrayType(pBaseType),
        pCurrentArray = pArray;

    while (1) {
        if (!ctx.consume(COMMA))
            ERROR(ctx, NULL, L"Expected \",\", got: %ls", TOK_S(ctx));

        TypePtr pDimensionType = parseType(ctx);
        if (!pDimensionType)
            return NULL;

        pCurrentArray->setDimensionType(pDimensionType);
        if (ctx.consume(RPAREN))
            break;

        pCurrentArray->setBaseType(new ArrayType());
        pCurrentArray = pCurrentArray->getBaseType().as<ArrayType>();
    }

    pCurrentArray->setBaseType(pBaseType);

    _ctx.mergeChildren();

    return pArray;
}

MapTypePtr Parser::parseMapType(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(MAP))
        UNEXPECTED(ctx, "map");

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    TypePtr pIndexType = parseType(ctx);

    if (!pIndexType)
        return NULL;

    if (!ctx.consume(COMMA))
        UNEXPECTED(ctx, ",");

    TypePtr pBaseType = parseType(ctx);

    if (!pBaseType)
        return NULL;

    if (! ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    _ctx.mergeChildren();

    return new MapType(pIndexType, pBaseType);
}

bool Parser::isTypeName(Context &_ctx, const std::wstring &_name) const {
    TypeDeclarationPtr pDecl = _ctx.getType(_name);

    if (pDecl)
        return true;

    if (NamedValuePtr pVar = _ctx.getVariable(_name))
        return isTypeVariable(pVar);

    return false;
}

NamedReferenceTypePtr Parser::parseTypeReference(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.is(IDENTIFIER))
        UNEXPECTED(ctx, "identifier");

    const std::wstring &str = ctx.scan();
    TypeDeclarationPtr pDecl = ctx.getType(str);

    if (!pDecl)
        ERROR(ctx, NULL, L"Unknown type identifier: %ls", str.c_str());

//    if (! pDecl) {
//        const NamedValue * pVar = ctx.getVariable(str);
//
//        if (isTypeVariable(pVar))
//            pType = new NamedReferenceType(pVar);
//        else
//            ERROR(ctx, NULL, L"Unknown type identifier: %ls", str.c_str());
//    } else

    NamedReferenceTypePtr pType = new NamedReferenceType(pDecl);

    if (pDecl && pDecl->getType() && pDecl->getType()->hasParameters() && ctx.is(LPAREN)) {
        if (!parseActualParameterList(ctx, pType.as<NamedReferenceType>()->getArgs()))
            ERROR(ctx, NULL, L"Garbage in argument list");
    }

    _ctx.mergeChildren();

    return pType;
}

RangePtr Parser::parseRange(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false, RESTRICT_TYPES);
    ExpressionPtr pMin = parseSubexpression(ctx, parseAtom(ctx), 0);

    if (!pMin)
        return NULL;

    if (!ctx.consume(DOUBLE_DOT))
        UNEXPECTED(ctx, "..");

    ExpressionPtr pMax = parseExpression(ctx);

    if (!pMax)
        return NULL;

    _ctx.mergeChildren();

    return new Range(pMin, pMax);
}

StructTypePtr Parser::parseStructType(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(STRUCT))
        UNEXPECTED(ctx, "struct");

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    StructTypePtr pType = new StructType();
    NamedValues fields;

    if (!parseParamList(ctx, fields, &Parser::parseVariableName, ALLOW_EMPTY_NAMES))
        return NULL;

    if (!fields.empty() && !fields.get(0)->getName().empty())
        pType->getNamesOrd().append(fields);
    else
        pType->getTypesOrd().append(fields);

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    _ctx.mergeChildren();

    return pType;
}

UnionTypePtr Parser::parseUnionType(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(UNION))
        UNEXPECTED(ctx, "union");

    UnionTypePtr pType = new UnionType();

    if (!parseList(ctx, pType->getConstructors(), &Parser::parseConstructorDeclaration,
            LPAREN, RPAREN, COMMA))
        return NULL;

    for (size_t i = 0; i < pType->getConstructors().size(); ++i) {
        pType->getConstructors().get(i)->setOrdinal(i);
        pType->getConstructors().get(i)->setUnion(pType);
    }

    _ctx.mergeChildren();

    return pType;
}

EnumTypePtr Parser::parseEnumType(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(ENUM))
        UNEXPECTED(ctx, "enum");

    EnumTypePtr pType = new EnumType();

    if (!parseList(ctx, pType->getValues(), &Parser::parseEnumValue, LPAREN, RPAREN, COMMA))
        return NULL;

    for (size_t i = 0; i < pType->getValues().size(); ++ i) {
        pType->getValues().get(i)->setType(pType);
        pType->getValues().get(i)->setOrdinal(i);
    }

    _ctx.mergeChildren();

    return pType;
}

SubtypePtr Parser::parseSubtype(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(SUBTYPE))
        UNEXPECTED(ctx, "subtype");

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    NamedValuePtr pVar = parseNamedValue(ctx);

    if (!pVar)
        return NULL;

    if (!ctx.consume(COLON))
        UNEXPECTED(ctx, ":");

    ExpressionPtr pExpr = parseExpression(ctx);

    if (!pExpr)
        return NULL;

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    _ctx.mergeChildren();

    return new Subtype(pVar, pExpr);
}

PredicateTypePtr Parser::parsePredicateType(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(PREDICATE))
        UNEXPECTED(ctx, "predicate");

    PredicateTypePtr pType = new PredicateType();

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    if (!parseParamList(ctx, pType->getInParams(), &Parser::parseParam, ALLOW_EMPTY_NAMES))
        ERROR(ctx, NULL, L"Failed to parse input parameters");

    branch_map_t branches;

    while (ctx.consume(COLON)) {
        BranchPtr pBranch = new Branch();

        pType->getOutParams().add(pBranch);
        parseParamList(ctx, *pBranch, &Parser::parseParam, OUTPUT_PARAMS | ALLOW_EMPTY_NAMES);

        for (size_t i = 0; i < pBranch->size(); ++i)
            pBranch->get(i)->setOutput(true);

        if (ctx.is(HASH) &&ctx.nextIn(IDENTIFIER, LABEL, INTEGER)) {
            std::wstring strLabel = ctx.scan(2, 1);

            if (!branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(ctx, NULL, L"Duplicate branch name \"%ls\"", strLabel.c_str());

            pBranch->setLabel(new Label(strLabel));
        }
    }

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    if (ctx.is(PRE))
        if (!parsePreconditions(ctx, *pType, branches))
            ERROR(ctx, NULL, L"Failed parsing preconditions");

    if (ctx.is(POST))
        if (!parsePostconditions(ctx, *pType, branches))
            ERROR(ctx, NULL, L"Failed parsing postconditions");

    _ctx.mergeChildren();

    return pType;
}

TypePtr Parser::parseType(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);
    TypePtr pType;

    bool bBuiltinType = true;

    switch (ctx.getToken()) {
        case NAT_TYPE:
            pType = new Type(Type::NAT);
            pType->setBits(ctx.getIntBits());
            ++ctx;
            break;
        case INT_TYPE:
            pType = new Type(Type::INT);
            pType->setBits(ctx.getIntBits());
            ++ctx;
            break;
        case REAL_TYPE:
            pType = new Type(Type::REAL);
            pType->setBits(ctx.getRealBits());
            ++ctx;
            break;
        case BOOL_TYPE:   pType = new Type(Type::BOOL); ++ctx; break;
        case CHAR_TYPE:   pType = new Type(Type::CHAR); ++ctx; break;
        case STRING_TYPE: pType = new Type(Type::STRING); ++ctx; break;
        case TYPE:        pType = new TypeType(new TypeDeclaration()); ++ctx; break;
        case VAR:         pType = new Type(Type::GENERIC); ++ctx; break;

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
            if (!(pType = parseDerivedTypeParameter(++ctx)))
                return NULL;
            pType = new SeqType(pType);
            break;
        case SET:
            if (!(pType = parseDerivedTypeParameter(++ctx)))
                return NULL;
            pType = new SetType(pType);
            break;
        case LIST:
            if (!(pType = parseDerivedTypeParameter(++ ctx)))
                return NULL;
            pType = new ListType(pType);
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

    if (bBuiltinType && !pType)
        ERROR(ctx, NULL, L"Error parsing type reference");

    if (!pType && ctx.is(IDENTIFIER))
        pType = parseTypeReference(ctx);

    if (!pType)
        pType = parseRange(ctx);

    if (!pType)
        ERROR(ctx, NULL, L"Unexpected token while parsing type reference: %ls", TOK_S(ctx));

    if (ctx.consume(ASTERISK))
        pType = new OptionalType(pType);

    _ctx.mergeChildren();

    return pType;
}

template<class _Param>
bool Parser::parseParamList(Context &_ctx, Collection<_Param> &_params,
        PARSER_FN(_Param,_parser,int), int _nFlags)
{
    if (_ctx.in(RPAREN, COLON, DOT))
        return true;

    Context &ctx = *_ctx.createChild(false);
    TypePtr pType;
    Collection<_Param> params;

    do {
        const bool bNeedType = (!pType
            || !ctx.is(IDENTIFIER)
            || !(ctx.nextIn(COMMA, RPAREN, COLON, DOT) ||  ctx.nextIn(SEMICOLON, EQ))
            || ((_nFlags & ALLOW_EMPTY_NAMES) && isTypeName(ctx, ctx.getValue())))
            && !(pType && (_nFlags & SINGLE_TYPE));

        if (bNeedType) {
            if (ctx.consume(MUTABLE))
                _nFlags |= IS_MUTABLE;
            pType = parseType(ctx);
            if (!pType)
                ERROR(ctx, false, L"Type required");
        }

        Auto<_Param> pParam;

        if (!ctx.is(IDENTIFIER)) {
            if (!(_nFlags & ALLOW_EMPTY_NAMES))
                ERROR(ctx, false, L"Identifier required");
            pParam = new _Param();
        } else
            pParam = (this->*_parser)(ctx, _nFlags);

        if (!pParam)
            ERROR(ctx, false, L"Variable or parameter definition required");

        if (pType->getKind() == Type::TYPE) {
            TypeDeclarationPtr pDecl = pType.as<TypeType>()->getDeclaration();

            pDecl->setName(pParam->getName());
            _ctx.addType(pDecl);
        }

        pParam->setType(pType);
        params.add(pParam);
    } while (ctx.consume(COMMA));

    _params.append(params);
    _ctx.mergeChildren();

    return true;
}

StatementPtr Parser::parseVariableDeclarationGroup(Context &_ctx, int _nFlags) {
    Collection<VariableDeclaration> decls;
    parseParamList(_ctx, decls, &Parser::parseVariableDeclaration, ALLOW_INITIALIZATION | PART_OF_LIST | SINGLE_TYPE | _nFlags);

    if (decls.empty())
        return NULL;

    if (decls.size() == 1)
        return decls.get(0);

    // decls.size() > 1
    VariableDeclarationGroupPtr pGroup = new VariableDeclarationGroup();

    pGroup->append(decls);
    _ctx.mergeChildren();

    return pGroup;
}

ParamPtr Parser::parseParam(Context &_ctx, int _nFlags) {
    if (!_ctx.is(IDENTIFIER))
        return NULL;

    Context &ctx = *_ctx.createChild(false);
    std::wstring name = ctx.scan();
    ParamPtr pParam;

    if (ctx.consume(SINGLE_QUOTE)) {
        if (!(_nFlags & OUTPUT_PARAMS))
            ERROR(ctx, NULL, L"Only output parameters can be declared as joined");

        NamedValuePtr pVar = ctx.getVariable(name, true);

        if (!pVar)
            ERROR(ctx, NULL, L"Parameter '%ls' is not defined.", name.c_str());

        if (pVar->getKind() != NamedValue::PREDICATE_PARAMETER || pVar.as<Param>()->isOutput())
            ERROR(ctx, NULL, L"Identifier '%ls' does not name a predicate input parameter.", name.c_str());

        name += L'\'';
        pParam = new Param(name);
        pParam->setLinkedParam(pVar.as<Param>());
        pVar.as<Param>()->setLinkedParam(pParam);
    } else
        pParam = new Param(name);

    if (ctx.consume(ASTERISK)) {
        if (!(_nFlags & ALLOW_ASTERSK))
            ERROR(ctx, NULL, L"Only input predicate parameters can automatically declare joined output parameters");
        pParam->setLinkedParam(pParam); // Just a mark, should be processed later.
    }

    pParam->setOutput(_nFlags & OUTPUT_PARAMS);
    ctx.addVariable(pParam);
    _ctx.mergeChildren();

    return pParam;
}

NamedValuePtr Parser::parseNamedValue(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);
    TypePtr pType = parseType(ctx);

    if (!pType)
        ERROR(ctx, NULL, L"Type required");

    if (!ctx.is(IDENTIFIER))
        ERROR(ctx, NULL, L"Identifier required");

    NamedValuePtr pVar = new NamedValue(ctx.scan(), pType);

    _ctx.mergeChildren();
    _ctx.addVariable(pVar);

    return pVar;
}

ElementDefinitionPtr Parser::parseArrayElement(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);
    ElementDefinitionPtr pElem = new ElementDefinition();
    ExpressionPtr pExpr = parseExpression(ctx);

    if (!pExpr)
        ERROR(ctx, NULL, L"Expression expected.");

    if (ctx.consume(COLON)) {
        pElem->setIndex(pExpr);
        pExpr = parseExpression(ctx);
    }

    if (!pExpr)
        ERROR(ctx, NULL, L"Expression expected.");

    pElem->setValue(pExpr);
    _ctx.mergeChildren();

    return pElem;
}

ElementDefinitionPtr Parser::parseMapElement(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);
    ElementDefinitionPtr pElem = new ElementDefinition();
    ExpressionPtr pExpr = parseExpression(ctx);

    if (!pExpr)
        ERROR(ctx, NULL, L"Index expression expected.");

    if (!ctx.consume(COLON))
        UNEXPECTED(ctx, ":");

    pElem->setIndex(pExpr);
    pExpr = parseExpression(ctx);

    if (!pExpr)
        ERROR(ctx, NULL, L"Value expression expected.");

    pElem->setValue(pExpr);
    _ctx.mergeChildren();

    return pElem;
}

StructFieldDefinitionPtr Parser::parseFieldDefinition(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);
    StructFieldDefinitionPtr pField = new StructFieldDefinition();

    if (ctx.is(IDENTIFIER, COLON))
        pField->setName(ctx.getValue());

    ExpressionPtr pExpr = parseExpression(ctx);

    if (!pExpr) {
        if (pField->getName().empty())
            ERROR(ctx, NULL, L"Field name expected.");
        ++ctx;
    }

    if (ctx.consume(COLON)) {
        if (pField->getName().empty())
            ERROR(ctx, NULL, L"Field name expected.");
        pExpr = parseExpression(ctx);
    }

    if (!pExpr)
        ERROR(ctx, NULL, L"Expression expected.");

    pField->setValue(pExpr);
    _ctx.mergeChildren();

    return pField;
}

StructFieldDefinitionPtr Parser::parseConstructorField(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false, RESTRICT_TYPES);
    StructFieldDefinitionPtr pField = new StructFieldDefinition();
    std::wstring strIdent;

    if (ctx.is(IDENTIFIER))
        strIdent = ctx.getValue();

    if (ExpressionPtr pExpr = parseExpression(ctx)) {
        pField->setValue(pExpr);
    } else {
        VariableDeclarationPtr pDecl = parseVariableDeclaration(ctx, LOCAL_VARIABLE);

        if (!pDecl && strIdent.empty())
            ERROR(ctx, NULL, L"Expression, declaration or identifier expected.");

        if (!pDecl) {
            // Unresolved identifier treated as variable declaration.
            pDecl = new VariableDeclaration(true, ctx.scan());
            pDecl->getVariable()->setType(new Type(Type::GENERIC));
        }

        if (ctx.getCurrentConstructor())
            ctx.getCurrentConstructor()->getDeclarations().add(pDecl);

        pField->setValue(new VariableReference(pDecl->getVariable()));
    }

    _ctx.mergeChildren();

    return pField;
}

UnionConstructorPtr Parser::parseConstructor(Context &_ctx, const UnionTypePtr &_pUnion) {
    if (!_ctx.is(IDENTIFIER))
        return NULL;

    Context &ctx = *_ctx.createChild(false);
    const std::wstring &strName = ctx.scan();
    UnionConstructorPtr pCons = new UnionConstructor(strName);

    if ((_pUnion && _pUnion->getConstructors().findByNameIdx(strName) == (size_t)-1) ||
            (!_pUnion && !_ctx.getConstructor(strName)))
        ERROR(ctx, NULL, L"Unknown union constructor reference: %ls", strName.c_str());

    if (ctx.is(LPAREN)) {
        ctx.setCurrentConstructor(pCons);

        if (!parseList(ctx, *pCons, &Parser::parseConstructorField, LPAREN, RPAREN, COMMA))
            ERROR(ctx, NULL, L"Union constructor parameters expected");
    }

    if (_pUnion) {
        UnionConstructorDeclarationPtr pDecl =
                _pUnion->getConstructors().get(_pUnion->getConstructors().findByNameIdx(strName));

        if (pDecl->getFields().size() != pCons->size())
            ERROR(ctx, NULL, L"Constructor %ls requires %u arguments, %u given.",
                    strName.c_str(), pDecl->getFields().size(), pCons->size());
    }

    _ctx.mergeChildren();

    return pCons;
}

UnionConstructorDeclarationPtr Parser::parseConstructorDeclaration(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.is(IDENTIFIER))
        ERROR(ctx, NULL, L"Constructor name expected.");

    UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(ctx.scan());

    if (ctx.consume(LPAREN)) {
        if (!parseParamList(ctx, pCons->getFields(), &Parser::parseVariableName))
            return NULL;

        if (!ctx.consume(RPAREN))
            UNEXPECTED(ctx, ")");
    }

    _ctx.mergeChildren();
    _ctx.addConstructor(pCons);

    return pCons;
}


EnumValuePtr Parser::parseEnumValue(Context &_ctx) {
    if (!_ctx.is(IDENTIFIER))
        return NULL;

    EnumValuePtr pVar = new EnumValue(_ctx.scan());

    _ctx.addVariable(pVar);

    return pVar;
}

NamedValuePtr Parser::parseVariableName(Context &_ctx, int /* _nFlags */) {
    if (!_ctx.is(IDENTIFIER))
        return NULL;

    NamedValuePtr pVar = new NamedValue(_ctx.scan());

    _ctx.addVariable(pVar);

    return pVar;
}

TypeDeclarationPtr Parser::parseTypeDeclaration(Context &_ctx) {
    if (!_ctx.is(TYPE, IDENTIFIER))
        return NULL;

    Context *pCtx = _ctx.createChild(false);
    TypeDeclarationPtr pDecl = new TypeDeclaration(pCtx->scan(2, 1));
    ParameterizedTypePtr pParamType;

    if (pCtx->consume(LPAREN)) {
        pCtx = pCtx->createChild(true);
        pParamType = new ParameterizedType();
        pDecl->setType(pParamType);
        if (!parseParamList(*pCtx, pParamType->getParams(), &Parser::parseVariableName))
            return NULL;
        if (!pCtx->consume(RPAREN))
            ERROR(*pCtx, NULL, L"Expected \")\", got: %ls", TOK_S(*pCtx));
    }

    _ctx.addType(pDecl); // So that recursive definitions would work.

    if (pCtx->consume(EQ)) {
        TypePtr pType = parseType(* pCtx);

        if (!pType)
            return NULL;

        if (pParamType)
            pParamType->setActualType(pType);
        else
            pDecl->setType(pType);
    }

    _ctx.mergeChildren();

    return pDecl;
}

BlockPtr Parser::parseBlock(Context &_ctx) {
    Context *pCtx = _ctx.createChild(false);

    if (!pCtx->consume(LBRACE))
        return NULL;

    BlockPtr pBlock = new Block();

    pCtx = pCtx->createChild(true);

    while (!pCtx->is(RBRACE)) {
        bool bNeedSeparator = false;

        if (pCtx->is(PRAGMA)) {
            Context *pCtxNew = parsePragma(*pCtx);

            if (!pCtxNew)
                ERROR(*pCtx, NULL, L"Failed parsing compiler directive");

            if (pCtxNew->is(LBRACE)) {
                BlockPtr pNewBlock = parseBlock(*pCtxNew);

                if (!pNewBlock)
                    ERROR(* pCtxNew, NULL, L"Failed parsing block");

                pBlock->add(pNewBlock);
                pCtx->mergeChildren();
            } else {
                pCtx = pCtxNew;
                bNeedSeparator = true;
            }
        } else {
            StatementPtr pStmt = parseStatement(*pCtx);

            if (!pStmt)
                ERROR(*pCtx, NULL, L"Error parsing statement");

            if (pCtx->is(DOUBLE_PIPE)) {
                ParallelBlockPtr pNewBlock = new ParallelBlock();

                pNewBlock->add(pStmt);

                while (pCtx->consume(DOUBLE_PIPE)) {
                    if (StatementPtr pStmt = parseStatement(* pCtx))
                        pNewBlock->add(pStmt);
                    else
                        ERROR(*pCtx, NULL, L"Error parsing parallel statement");
                }

                pStmt = pNewBlock;
            }

            pBlock->add(pStmt);
            bNeedSeparator = !pStmt->isBlockLike() && !pCtx->in(LBRACE, RBRACE);
        }

        if (bNeedSeparator && !pCtx->consume(SEMICOLON))
            ERROR(* pCtx, NULL, L"Expected \";\", got: %ls", TOK_S(* pCtx));
    }

    ++(*pCtx);
    _ctx.mergeChildren();

    return pBlock;
}

MultiassignmentPtr Parser::parseMultiAssignment(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);
    MultiassignmentPtr pMA = new Multiassignment();

    if (!parseList(ctx, pMA->getLValues(), &Parser::parseAtom, PIPE, PIPE, COMMA))
        ERROR(ctx, NULL, L"Error parsing list of l-values");

    if (!ctx.consume(EQ))
        UNEXPECTED(ctx, "=");

    if (ctx.is(PIPE)) {
        if (!parseList(ctx, pMA->getExpressions(), &Parser::parseExpression, PIPE, PIPE, COMMA))
            ERROR(ctx, NULL, L"Error parsing list of expression");
    } else if (ExpressionPtr pExpr = parseExpression(ctx)) {
        pMA->getExpressions().add(pExpr);
    } else
        ERROR(ctx, NULL, L"Expression expected");

    _ctx.mergeChildren();

    return pMA;
}

SwitchPtr Parser::parseSwitch(Context &_ctx) {
    Context &ctx = *_ctx.createChild(true);

    if (!ctx.consume(SWITCH))
        UNEXPECTED(ctx, "switch");

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    VariableDeclarationPtr pDecl = parseVariableDeclaration(ctx, LOCAL_VARIABLE | ALLOW_INITIALIZATION);
    ExpressionPtr pExpr;

    if (!pDecl) {
        pExpr = parseExpression(ctx);

        if (!pExpr)
            ERROR(ctx, NULL, L"Expression or variable declaration expected");
    }

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    SwitchPtr pSwitch = new Switch();

    if (pDecl) {
        pSwitch->setParamDecl(pDecl);
        pExpr = new VariableReference(pDecl->getVariable());
        pExpr->setType(pExpr->getType());
        pSwitch->setArg(pExpr);
    } else
        pSwitch->setArg(pExpr);

    if (!ctx.consume(LBRACE))
        UNEXPECTED(ctx, "{");

    while (ctx.in(CASE)) {
        Context &ctxCase = *ctx.createChild(true);
        SwitchCasePtr pCase = new SwitchCase();

        if (!parseList(ctxCase, pCase->getExpressions(), &Parser::parseExpression, CASE, COLON, COMMA))
            ERROR(ctxCase, NULL, L"Error parsing list of expressions");

        if (StatementPtr pStmt = parseStatement(ctxCase))
            pCase->setBody(pStmt);
        else
            ERROR(ctxCase, NULL, L"Statement required");

        pSwitch->add(pCase);
        ctx.mergeChildren();
    }

    if (ctx.is(DEFAULT, COLON)) {
        ctx.skip(2);

        if (StatementPtr pStmt = parseStatement(ctx))
            pSwitch->setDefault(pStmt);
        else
            ERROR(ctx, NULL, L"Statement required");
    }

    if (!ctx.consume(RBRACE))
        UNEXPECTED(ctx, "}");

    _ctx.mergeChildren();

    return pSwitch;
}

IfPtr Parser::parseConditional(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(IF))
        UNEXPECTED(ctx, "if");

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    ExpressionPtr pExpr = parseExpression(ctx);

    if (!pExpr)
        ERROR(ctx, NULL, L"Expression expected");

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    StatementPtr pStmt = parseStatement(ctx);

    if (!pStmt)
        ERROR(ctx, NULL, L"Statement expected");

    IfPtr pIf = new If();

    pIf->setArg(pExpr);
    pIf->setBody(pStmt);

    if (ctx.consume(ELSE)) {
        pStmt = parseStatement(ctx);

        if (!pStmt)
            ERROR(ctx, NULL, L"Statement expected");

        pIf->setElse(pStmt);
    }

    _ctx.mergeChildren();

    return pIf;
}

JumpPtr Parser::parseJump(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(HASH))
        UNEXPECTED(ctx, "#");

    if (!ctx.in(LABEL, IDENTIFIER, INTEGER))
        ERROR(ctx, NULL, L"Label identifier expected");

    std::wstring name = ctx.scan();
    LabelPtr pLabel = ctx.createLabel(name);

    _ctx.mergeChildren();

    return new Jump(pLabel);
}

ReceivePtr Parser::parseReceive(Context &_ctx) {
    assert(false && "Unimplemented");
    return NULL;
}

SendPtr Parser::parseSend(Context &_ctx) {
    assert(false && "Unimplemented");
    return NULL;
}

WithPtr Parser::parseWith(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(WITH))
        UNEXPECTED(ctx, "with");

    WithPtr pWith = new With();

    if (!parseList(ctx, pWith->getArgs(), &Parser::parseExpression, LPAREN, RPAREN, COMMA))
        ERROR(ctx, NULL, L"Error parsing list of expressions");

    if (StatementPtr pStmt = parseStatement(ctx))
        pWith->setBody(pStmt);
    else
        ERROR(ctx, NULL, L"Statement expected");

    _ctx.mergeChildren();

    return pWith;
}

ForPtr Parser::parseFor(Context &_ctx) {
    Context &ctx = *_ctx.createChild(true);

    if (!ctx.consume(FOR))
        UNEXPECTED(ctx, "for");

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    ForPtr pFor = new For();

    if (!ctx.in(SEMICOLON)) {
        if (VariableDeclarationPtr pDecl = parseVariableDeclaration(ctx, LOCAL_VARIABLE | ALLOW_INITIALIZATION))
            pFor->setIterator(pDecl);
        else
            ERROR(ctx, NULL, L"Variable declaration expected");
    }

    if (!ctx.consume(SEMICOLON))
        UNEXPECTED(ctx, ";");

    if (!ctx.in(SEMICOLON)) {
        if (ExpressionPtr pExpr = parseExpression(ctx))
            pFor->setInvariant(pExpr);
        else
            ERROR(ctx, NULL, L"Expression expected");
    }

    if (!ctx.consume(SEMICOLON))
        UNEXPECTED(ctx, ";");

    if (!ctx.is(RPAREN)) {
        if (StatementPtr pStmt = parseStatement(ctx))
            pFor->setIncrement(pStmt);
        else
            ERROR(ctx, NULL, L"Statement expected");
    }

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    if (StatementPtr pStmt = parseStatement(ctx))
        pFor->setBody(pStmt);
    else
        ERROR(ctx, NULL, L"Statement expected");

    _ctx.mergeChildren();

    return pFor;
}

WhilePtr Parser::parseWhile(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(WHILE))
        UNEXPECTED(ctx, "while");

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    WhilePtr pWhile = new While();

    if (ExpressionPtr pExpr = parseExpression(ctx))
        pWhile->setInvariant(pExpr);
    else
        ERROR(ctx, NULL, L"Expression expected");

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    if (StatementPtr pStmt = parseStatement(ctx))
        pWhile->setBody(pStmt);
    else
        ERROR(ctx, NULL, L"Statement expected");

    _ctx.mergeChildren();

    return pWhile;
}

BreakPtr Parser::parseBreak(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(BREAK))
        UNEXPECTED(ctx, "break");

    _ctx.mergeChildren();

    return new Break();
}

StatementPtr Parser::parseAssignment(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);
    ExpressionPtr pLHS = parseAtom(ctx);
    StatementPtr pStmt;

    if (!pLHS)
        ERROR(ctx, NULL, L"Error parsing expression");

    if (ctx.consume(EQ)) {
        if (ExpressionPtr pRHS = parseExpression(ctx))
            pStmt = new Assignment(pLHS, pRHS);
        else
            ERROR(ctx, NULL, L"Error parsing expression");
    } else if (ctx.consume(COMMA)) {
        MultiassignmentPtr pMA = new Multiassignment();

        pMA->getLValues().add(pLHS);

        if (!parseList(ctx, pMA->getLValues(), &Parser::parseAtom, -1, EQ, COMMA))
            ERROR(ctx, NULL, L"Error parsing list of l-values");

        if (!parseList(ctx, pMA->getExpressions(), &Parser::parseExpression, -1, -1, COMMA))
            ERROR(ctx, NULL, L"Error parsing list of expression");

        pStmt = pMA;
    } else
        ERROR(ctx, NULL, L"Expected \"=\" or \",\", got: %ls", TOK_S(ctx));

    _ctx.mergeChildren();

    return pStmt;
}

ExpressionPtr Parser::parseCallResult(Context &_ctx, VariableDeclarationPtr &_pDecl) {
    Context *pCtx = _ctx.createChild(false);
    ExpressionPtr pExpr;

    // Try variable declaration.
    TypePtr pType = parseType(*pCtx);

    if (pType && pCtx->is(IDENTIFIER)) {
        _pDecl = new VariableDeclaration(true, pCtx->scan());
        _pDecl->getVariable()->setType(pType);
        _pDecl->getVariable()->setMutable(false);
        pExpr = new VariableReference(_pDecl->getVariable());
        _ctx.addVariable(_pDecl->getVariable());
    }

    if (!pExpr) {
        _pDecl = NULL;
        pCtx = _ctx.createChild(false);
        pExpr = parseExpression(*pCtx);

        if (!pExpr)
            ERROR(*pCtx, NULL, L"Error parsing output parameter");
    }

    _ctx.mergeChildren();

    return pExpr;
}

bool Parser::parseCallResults(Context &_ctx, Call &_call, Collection<Expression> &_list) {
    Context &ctx = *_ctx.createChild(false);
    ExpressionPtr pExpr;
    VariableDeclarationPtr pDecl;

    if (!ctx.consume(UNDERSCORE)) {
        pExpr = parseCallResult(ctx, pDecl);

        if (!pExpr)
            ERROR(ctx, false, L"Error parsing output parameter");
    }

    // It's okay to modify call object since if this function fails parseCall() fails too.
    _list.add(pExpr);

    if (pDecl)
        _call.getDeclarations().add(pDecl);

    while (ctx.consume(COMMA)) {
        if (!ctx.consume(UNDERSCORE)) {
            pExpr = parseCallResult(ctx, pDecl);

            if (!pExpr)
                ERROR(ctx, false, L"Error parsing output parameter");
        }

        _list.add(pExpr);

        if (pDecl)
            _call.getDeclarations().add(pDecl);
    }

    _ctx.mergeChildren();

    return true;
}

CallPtr Parser::parseCall(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);
    std::wstring name = ctx.getValue();
    ExpressionPtr pExpr;

    if (PredicatePtr pPred = ctx.getPredicate(name)) {
        ++ctx;
        pExpr = new PredicateReference(pPred);
        pExpr->setType(pPred->getType());
    } else {
        pExpr = parseAtom(ctx);

        if (!pExpr)
            ERROR(ctx, NULL, L"Predicate expression expected");
    }

    CallPtr pCall = new Call();

    pCall->setPredicate(pExpr);

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    typedef std::multimap<std::wstring, CallBranchPtr> call_branch_map_t;
    call_branch_map_t branches;

    if (!ctx.is(RPAREN)) {
        if (!ctx.is(COLON) && !parseList(ctx, pCall->getArgs(), &Parser::parseExpression, -1, -1, COMMA))
            ERROR(ctx, NULL, L"Failed to parse input parameters");

        while (ctx.consume(COLON)) {
            CallBranchPtr pBranch = new CallBranch();

            pCall->getBranches().add(pBranch);

            if (!ctx.in(RPAREN, HASH, COLON)) {
                if (!parseCallResults(ctx, *pCall, *pBranch))
                    ERROR(ctx, NULL, L"Failed to parse output parameters");
            }

            if (ctx.is(HASH) && ctx.nextIn(IDENTIFIER, LABEL, INTEGER)) {
                const std::wstring strLabel = ctx.scan(2, 1);

                if (LabelPtr pLabel = ctx.getLabel(strLabel))
                    pBranch->setHandler(new Jump(pLabel));
                else
                    branches.insert(std::make_pair(strLabel, pBranch));
            }
        }
    }

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    if (!branches.empty())
        while (ctx.consume(CASE)) {
            if (!ctx.in(LABEL, IDENTIFIER, INTEGER) || !ctx.nextIs(COLON))
                ERROR(ctx, NULL, L"Label identifier expected");

            typedef call_branch_map_t::iterator I;
            std::pair<I, I> bounds = branches.equal_range(ctx.scan());

            ++ctx;

            if (bounds.first == bounds.second)
                ERROR(ctx, NULL, L"Label identifier expected");

            if (StatementPtr pStmt = parseStatement(ctx))
                for (I i = bounds.first; i != bounds.second; ++ i)
                    i->second->setHandler(pStmt);
            else
                ERROR(ctx, NULL, L"Statement required");
        }

    if (pCall->getBranches().empty())
        pCall->getBranches().add(new Branch());

    _ctx.mergeChildren();

    return pCall;
}

StatementPtr Parser::parseStatement(Context &_ctx) {
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
        case MUTABLE:
        CASE_BUILTIN_TYPE:
            return parseVariableDeclarationGroup(_ctx, LOCAL_VARIABLE);
    }

    StatementPtr pStmt;

    if (_ctx.in(IDENTIFIER, LABEL, INTEGER) && _ctx.nextIs(COLON)) {
        Context &ctx = *_ctx.createChild(false);
        LabelPtr pLabel = ctx.createLabel(ctx.scan(2));

        if (ctx.is(RBRACE))
            pStmt = new Statement();
        else
            pStmt = parseStatement(ctx);

        if (!pStmt)
            return NULL;

        if (pStmt->getLabel()) {
            // We should somehow make two labels to point to the same statement.
            BlockPtr pBlock = new Block();
            pBlock->add(pStmt);
            pStmt = pBlock;
        }

        pStmt->setLabel(pLabel);
        _ctx.mergeChildren();

        return pStmt;
    }

    if (_ctx.getType(_ctx.getValue()))
        return parseVariableDeclarationGroup(_ctx, LOCAL_VARIABLE);

    Context &ctx = *_ctx.createChild(false);

    // Maybe call?
    if (!pStmt)
        pStmt = parseCall(ctx);

    // Maybe assignment?
    if (!pStmt)
        pStmt = parseAssignment(ctx);

    // Maybe nested predicate?
    if (!pStmt)
        pStmt = parsePredicate(ctx);

    if (!pStmt)
        return NULL;

    _ctx.mergeChildren();

    return pStmt;
}

ProcessPtr Parser::parseProcess(Context &_ctx) {
    Context &ctx = *_ctx.createChild(true);

    if (!ctx.consume(PROCESS))
        UNEXPECTED(ctx, "process");

    if (!ctx.is(IDENTIFIER))
        ERROR(ctx, NULL, L"Identifier expected");

    ProcessPtr pProcess = new Process(ctx.scan());

    if (!ctx.consume(LPAREN))
        UNEXPECTED(ctx, "(");

    if (!parseParamList(ctx, pProcess->getInParams(), &Parser::parseParam, 0))
        ERROR(ctx, NULL, L"Failed to parse input parameters");

    branch_map_t branches;

    while (ctx.consume(COLON)) {
        BranchPtr pBranch = new Branch();

        pProcess->getOutParams().add(pBranch);
        parseParamList(ctx, *pBranch, &Parser::parseParam, OUTPUT_PARAMS);

        for (size_t i = 0; i < pBranch->size(); ++i)
            pBranch->get(i)->setOutput(true);

        if (ctx.is(HASH) && ctx.nextIn(IDENTIFIER, LABEL, INTEGER)) {
            const std::wstring strLabel = ctx.scan(2, 1);

            if (!branches.insert(std::make_pair(strLabel, pBranch)).second)
                ERROR(ctx, NULL, L"Duplicate branch name \"%ls\"", strLabel.c_str());

            pBranch->setLabel(new Label(strLabel));
        }
    }

    if (!ctx.consume(RPAREN))
        UNEXPECTED(ctx, ")");

    if (BlockPtr pBlock = parseBlock(ctx))
        pProcess->setBlock(pBlock);
    else
        ERROR(ctx, NULL, L"Failed parsing process body");

    _ctx.mergeChildren();
    _ctx.addProcess(pProcess);

    return pProcess;
}

FormulaDeclarationPtr Parser::parseFormulaDeclaration(Context &_ctx) {
    if (!_ctx.is(FORMULA, IDENTIFIER))
        return NULL;

    Context *pCtx = _ctx.createChild(false, ALLOW_FORMULAS);
    FormulaDeclarationPtr pDecl = new FormulaDeclaration(pCtx->scan(2, 1));

    pCtx->addFormula(pDecl);
    pCtx = pCtx->createChild(true, ALLOW_FORMULAS);

    if (!pCtx->consume(LPAREN))
        UNEXPECTED(*pCtx, "(");

    if (!pCtx->consume(RPAREN)) {
        pCtx = pCtx->createChild(true, ALLOW_FORMULAS);

        if (!parseParamList(*pCtx, pDecl->getParams(), &Parser::parseVariableName))
            return NULL;

        if (pCtx->consume(COLON)) {
            if (TypePtr pType = parseType(*pCtx))
                pDecl->setResultType(pType);
            else
                ERROR(*pCtx, NULL, L"Failed parsing formula result type");
        }

        if (!pCtx->consume(RPAREN))
            UNEXPECTED(*pCtx, ")");
    }

    if (pCtx->consume(EQ)) {
        if (ExpressionPtr pFormula = parseExpression(*pCtx))
            pDecl->setFormula(pFormula);
        else
            return NULL;
    }

    _ctx.mergeChildren();
    _ctx.addFormula(pDecl);

    return pDecl;
}

LemmaDeclarationPtr Parser::parseLemmaDeclaration(Context &_ctx) {
    if (!_ctx.is(LEMMA))
        return NULL;

    Context &ctx = *_ctx.createChild(false, ALLOW_FORMULAS);
    LemmaDeclarationPtr pLemma = new LemmaDeclaration();

    ++ctx;

    if (ExpressionPtr pProposition = parseExpression(ctx))
        pLemma->setProposition(pProposition);
    else
        ERROR(ctx, NULL, L"Failed parsing lemma proposition");

    _ctx.mergeChildren();

    return pLemma;
}

Context *Parser::parsePragma(Context &_ctx) {
    Context &ctx = *_ctx.createChild(false);

    if (!ctx.consume(PRAGMA))
        UNEXPECTED_R(ctx, "pragma", NULL);

    if (!ctx.consume(LPAREN))
        UNEXPECTED_R(ctx, "(", NULL);

    do {
        if (!ctx.is(IDENTIFIER))
            ERROR(ctx, NULL, L"Pragma name expected");

        const std::wstring name = ctx.scan();

        if (!ctx.consume(COLON))
            UNEXPECTED_R(ctx, ":", NULL);

        if (name == L"int_bitness") {
            int nBitness = 0;

            if (ctx.is(INTEGER)) {
                nBitness = wcstol(ctx.scan().c_str(), NULL, 10);

                if (nBitness < 1 || nBitness > 64)
                    ERROR(ctx, NULL, L"Integer bitness out of range: %d", nBitness);
            } else if (ctx.is(IDENTIFIER)) {
                const std::wstring strBitness = ctx.scan();

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

                if (nBitness != 32 &&nBitness != 64 &&nBitness != 128)
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

                const std::wstring strLabel = ctx.scan();

                if (LabelPtr pLabel = ctx.getLabel(strLabel))
                    ctx.getPragma().overflow().set(Overflow::RETURN, pLabel);
                else
                    ERROR(ctx, NULL, L"Unknown label %ls", strLabel.c_str());
            } else if (ctx.is(IDENTIFIER)) {
                const std::wstring strOverflow = ctx.scan();
                int nOverflow = Overflow::WRAP;

                if (strOverflow == L"wrap")
                    nOverflow = Overflow::WRAP;
                else if (strOverflow == L"saturate")
                    nOverflow = Overflow::SATURATE;
                else if (strOverflow == L"strict")
                    nOverflow = Overflow::STRICT;
                else
                    ERROR(ctx, NULL, L"Unknown overflow value: %ls", strOverflow.c_str());

                ctx.getPragma().overflow().set(nOverflow);
            }

            ctx.getPragma().set(Pragma::Overflow, true);
        } else
            ERROR(ctx, NULL, L"Unknown pragma %ls", name.c_str());
    } while (ctx.consume(COMMA));

    if (!ctx.consume(RPAREN))
        UNEXPECTED_R(ctx, ")", NULL);

    return &ctx;
}

bool Parser::typecheck(Context &_ctx, Node &_node) {
    if (Options::instance().typeCheck == TC_NONE)
        return true;

    tc::Formulas constraints, substs;

    try {
        tc::collect(constraints, _node, _ctx);
    }
    catch (const std::exception &_e) {
        ERROR(_ctx, false, L"Type error: %s", _e.what());
    }

    if (tc::solve(constraints, substs)) {
        tc::apply(substs, _node);
        tc::linkPredicates(_ctx, _node);
    }
    else if (Options::instance().typeCheck != TC_SOFT)
        ERROR(_ctx, false, L"Type error");

    return true;
}

bool Parser::parseDeclarations(Context &_ctx, Module &_module) {
    Context *pCtx = _ctx.createChild(false);

    while (!pCtx->in(END_OF_FILE, RBRACE)) {
        switch (pCtx->getToken()) {
            case MODULE:
                if (ModulePtr pModule = parseModule(*pCtx, false))
                    _module.getModules().add(pModule);
                else
                    ERROR(*pCtx, false, L"Failed parsing submodule");
                break;
            case IDENTIFIER:
                if (! pCtx->getType(pCtx->getValue())) {
                    if (PredicatePtr pPred = parsePredicate(*pCtx)) {
                        _module.getPredicates().add(pPred);
                        if (!typecheck(*pCtx, *pPred))
                            return false;
                    } else
                        ERROR(*pCtx, false, L"Failed parsing predicate");
                    break;
                }
                // no break;
            CASE_BUILTIN_TYPE:
            case MUTABLE: {
                Collection<VariableDeclaration> decls;
                parseParamList(*pCtx, decls, &Parser::parseVariableDeclaration, ALLOW_INITIALIZATION | PART_OF_LIST | SINGLE_TYPE);
                if (decls.empty())
                    ERROR(* pCtx, false, L"Failed parsing variable declaration");
                _module.getVariables().append(decls);
                if (!typecheck(*pCtx, decls))
                        return false;
            }
            break;
            case TYPE:
                if (TypeDeclarationPtr pDecl = parseTypeDeclaration(*pCtx)) {
                    if (!pCtx->consume(SEMICOLON))
                        ERROR(*pCtx, false, L"Semicolon expected");
                    _module.getTypes().add(pDecl);
                    if (!typecheck(*pCtx, *pDecl))
                            return false;
                } else
                    ERROR(*pCtx, false, L"Failed parsing type declaration");
                break;
            case PROCESS:
                if (ProcessPtr pProcess = parseProcess(*pCtx))
                    _module.getProcesses().add(pProcess);
                else
                    ERROR(*pCtx, false, L"Failed parsing process declaration");
                break;
            case FORMULA:
                if (FormulaDeclarationPtr pFormula = parseFormulaDeclaration(*pCtx)) {
                    _module.getFormulas().add(pFormula);
                    if (!typecheck(*pCtx, *pFormula))
                        return false;
                }
                else
                    ERROR(* pCtx, false, L"Failed parsing formula declaration");
                break;
            case LEMMA:
                if (LemmaDeclarationPtr pLemma = parseLemmaDeclaration(*pCtx)) {
                    _module.getLemmas().add(pLemma);
                    if (!typecheck(*pCtx, *pLemma))
                        return false;
                }
                else
                    ERROR(*pCtx, false, L"Failed parsing lemma declaration");
                break;
            case PRAGMA:
                if (Context *pCtxNew = parsePragma(*pCtx)) {
                    if (pCtxNew->consume(SEMICOLON)) {
                        pCtx = pCtxNew;
                        break;
                    } else if (pCtxNew->consume(LBRACE)) {
                        if (!parseDeclarations(*pCtxNew, _module))
                            ERROR(*pCtxNew, false, L"Failed parsing declarations");

                        if (!pCtxNew->consume(RBRACE))
                            ERROR(*pCtxNew, false, L"Closing brace expected");

                        pCtx->mergeChildren();
                        break;
                    } else
                        ERROR(*pCtx, false, L"Semicolon or opening brace expected");
                } else
                    ERROR(*pCtx, false, L"Failed parsing compiler directive");
            default:
                ERROR(*pCtx, false, L"Declaration expected");
        }

        while (pCtx->consume(SEMICOLON))
            ;
    }

    _ctx.mergeChildren();
    return true;
}

ModulePtr Parser::parseModule(Context &_ctx, bool _bTopLevel) {
    Context &ctx = *_ctx.createChild(true);
    int endToken = END_OF_FILE;
    ModulePtr pModule;

    if (ctx.consume(MODULE)) {
        if (!ctx.is(IDENTIFIER))
            ERROR(ctx, NULL, L"Identifier expected");

        pModule = new Module(ctx.scan());

        if (ctx.consume(LPAREN)) {
            if (!parseParamList(ctx, pModule->getParams(), &Parser::parseVariableName))
                return NULL;

            if (!ctx.consume(RPAREN))
                UNEXPECTED(ctx, ")");
        }

        if (ctx.consume(LBRACE))
            endToken = RBRACE;
        else if (!_bTopLevel)
            ERROR(ctx, NULL, L"Expected \"{\", got: %ls", TOK_S(ctx));
        else if (!ctx.consume(SEMICOLON))
            ERROR(ctx, NULL, L"Expected \";\", got: %ls", TOK_S(ctx));
    } else
        pModule = new Module();

    while (ctx.is(IMPORT))
        if (!parseImport(ctx, *pModule)) {
            ctx.fmtError(L"Invalid import statement");
            return NULL;
        }

    if (!ctx.consume(endToken)) {
        if (!parseDeclarations(ctx, *pModule))
            return NULL;

        if (!ctx.consume(endToken))
            UNEXPECTED(_ctx, "End of module");
    }

    _ctx.addModule(pModule);
    _ctx.addModuleCtx(pModule, &ctx);
    _ctx.setChild(NULL);

    return pModule;
}

ModulePtr parse(Tokens &_tokens) {
    Loc loc = _tokens.begin();
    Parser parser(_tokens);
    Context ctx(loc, true);

    if (ModulePtr pModule = parser.parseModule(ctx, true)) {
        ctx.mergeChildren(true);
        DEBUG(L"Done.");

        return pModule;
    }

    ctx.mergeChildren(true);
    std::wcerr << L"Parsing failed at line " << ctx.loc()->getLine() << std::endl;

    for (StatusMessages::const_iterator i = ctx.getMessages().begin();
        i != ctx.getMessages().end(); ++ i)
    {
        const StatusMessage &msg = *i;
        std::wcerr << msg;
    }

    return NULL;
}
