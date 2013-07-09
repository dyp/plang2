#include "pp_syntax.h"
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"

#include <sstream>

using namespace ir;

namespace pp {

class CollectIdentifiers : public ir::Visitor {
public:
    CollectIdentifiers(std::map<NamedValuePtr, std::wstring>& _identifiers, std::set<std::wstring>& _usedIdentifiers) :
        m_identifiers(_identifiers), m_usedIdentifiers(_usedIdentifiers)
    {}

    virtual bool visitNamedValue(NamedValue & _val) {
        if (!_val.getName().empty()) {
            m_identifiers.insert(std::pair<NamedValuePtr, std::wstring>(&_val, _val.getName()));
            m_usedIdentifiers.insert(_val.getName());
        }
    }

    virtual bool visitVariableReference(VariableReference & _var) {
        if (_var.getTarget())
            visitNamedValue(*_var.getTarget());
    }

private:
    std::map<NamedValuePtr, std::wstring> &m_identifiers;
    std::set<std::wstring> &m_usedIdentifiers;
};

typedef std::multimap<NodePtr, NodePtr> Graph;
typedef std::pair<const NodePtr, NodePtr> Edge;

class GetDeclarations : public ir::Visitor {
public:
    GetDeclarations(Graph& _decls) :
        m_decls(_decls), m_pKey(NULL)
    {}

#define TRAVERSE_GROUP(_TYPE)                                           \
    virtual bool traverse##_TYPE(_TYPE& _node) {                        \
        if (m_pKey)                                                     \
            m_decls.insert(Edge(m_pKey, &_node));                       \
        const NodePtr pOldKey = m_pKey;                                 \
        m_pKey = &_node;                                                \
        const bool bResult = Visitor::traverse##_TYPE(_node);           \
        m_pKey = pOldKey;                                               \
        return bResult;                                                 \
    }

    TRAVERSE_GROUP(Module)
    TRAVERSE_GROUP(TypeDeclaration)
    TRAVERSE_GROUP(Predicate)
    TRAVERSE_GROUP(FormulaDeclaration)

#undef TRAVERSE_GROUP

#define VISIT_DECLARATION(_TYPE)                                        \
    virtual bool visit##_TYPE(_TYPE& _decl) {                           \
        if (m_pKey != (NodePtr)&_decl                                   \
            && getLoc().role != R_UnionCostructorVarDecl)               \
            m_decls.insert(Edge(m_pKey, &_decl));                       \
        return true;                                                    \
    }

    VISIT_DECLARATION(TypeDeclaration)
    VISIT_DECLARATION(FormulaDeclaration)
    VISIT_DECLARATION(VariableDeclaration)
    VISIT_DECLARATION(Module)
    VISIT_DECLARATION(Predicate)

#undef VISIT_DECLARATION

    void run(const NodePtr& _pNode) {
        m_decls.clear();
        if (_pNode)
            traverseNode(*_pNode);
    }

private:
    NodePtr m_pKey;
    Graph& m_decls;
};

static bool _depthFirstTraversal(const Graph& _graph, const Edge& _edge, const NodePtr& _vertex) {
    if (_vertex == _edge.first)
        return true;

    std::pair<Graph::const_iterator, Graph::const_iterator>
        its = _graph.equal_range(_vertex);
    for (Graph::const_iterator i = its.first; i != its.second; ++i)
        if (_depthFirstTraversal(_graph, _edge, i->second))
            return true;

    return false;
}

static bool _hasLoops(const Graph& _graph, const Edge& _edge) {
    return _depthFirstTraversal(_graph, _edge, _edge.second);
}

class GetDeclDependencies : public ir::Visitor {
public:
    GetDeclDependencies(const Graph& _decls, Graph& _graph) :
        m_decls(_decls), m_graph(_graph), m_pRoot(NULL), m_pSuper(NULL)
    {}

#define TRAVERSE_DECLARATION(_TYPE)                             \
    virtual bool traverse##_TYPE(_TYPE& _node) {                \
        if (m_pSuper || (NodePtr)&_node == m_pRoot)             \
            return Visitor::traverse##_TYPE(_node);             \
        m_pSuper = &_node;                                      \
        const bool bResult = Visitor::traverse##_TYPE(_node);   \
        m_pSuper = NULL;                                        \
        return bResult;                                         \
    }

    TRAVERSE_DECLARATION(TypeDeclaration)
    TRAVERSE_DECLARATION(FormulaDeclaration)
    TRAVERSE_DECLARATION(VariableDeclaration)
    TRAVERSE_DECLARATION(Predicate)

#undef VISIT_DECLARATION

    virtual bool traverseModule(Module& _module) {
        return (&_module == m_pRoot.ptr())
            ? Visitor::traverseModule(_module)
            : false;
    }

    void addEdge(const Edge& _edge) {
        if (!_hasLoops(m_graph, _edge)) {
            m_graph.insert(_edge);
            return;
        }
        //TODO Resolve loops.
    }

    void addToGraph(NodePtr _pSub) {
        if (m_pSuper == _pSub)
            return;
        if (std::find(m_decls.begin(), m_decls.end(), Edge(m_pRoot, m_pSuper)) != m_decls.end()
            && std::find(m_decls.begin(), m_decls.end(), Edge(m_pRoot, _pSub)) != m_decls.end())
            addEdge(Edge(m_pSuper, _pSub));
    }

    virtual bool visitNamedReferenceType(NamedReferenceType& _type) {
        addToGraph(_type.getDeclaration());
        return true;
    }

    virtual bool visitMapElementExpr(MapElementExpr& _node) {
        return false;
    }

    virtual bool visitVariableReference(VariableReference& _var) {
        if (_var.getTarget()->getKind() == NamedValue::LOCAL
            || _var.getTarget()->getKind() == NamedValue::GLOBAL)
            addToGraph(_var.getTarget().as<Variable>()->getDeclaration());
        return true;
    }

    virtual bool visitFormulaCall(FormulaCall& _call) {
        addToGraph(_call.getTarget());
        return true;
    }

    void run(const NodePtr& _pNode) {
        m_pRoot = _pNode;
        m_graph.clear();
        if (_pNode)
            traverseNode(*_pNode);
    }

private:
    NodePtr m_pRoot, m_pSuper;
    Graph& m_graph;
    const Graph& m_decls;
};

void Context::collectIdentifiers(Node &_node) {
    CollectIdentifiers(m_identifiers, m_usedIdentifiers).traverseNode(_node);
}

void Context::addLabel(const std::wstring& _name) {
    m_usedLabels.insert(_name);
}

void Context::addNamedValue(const NamedValuePtr& _pVal) {
    if (!_pVal->getName().empty()) {
        m_identifiers.insert(std::make_pair(_pVal, _pVal->getName()));
        m_usedIdentifiers.insert(_pVal->getName());
    }
}

std::wstring Context::getNewLabelName(const std::wstring& _name) {
    for (size_t i = 1;; ++i) {
        const std::wstring strName = _name + fmtInt(i, L"%d");
        if (m_usedLabels.insert(strName).second)
            return strName;
    }
}

std::wstring Context::getNamedValueName(NamedValue &_val) {
    std::wstring strIdent = m_identifiers[&_val];

    if (strIdent.empty()) {
        for (m_nLastFoundIdentifier;
            !m_usedIdentifiers.insert(strIdent = intToAlpha(m_nLastFoundIdentifier)).second;
            ++m_nLastFoundIdentifier);
        m_identifiers[&_val] = strIdent;
    }

    return strIdent;
}

void Context::_buildDependencies(NodePtr _pRoot) {
    GetDeclarations(m_decls).run(_pRoot);
    GetDeclDependencies(m_decls, m_deps).run(_pRoot);
}

void Context::_topologicalSort(const NodePtr& _pDecl, std::list<NodePtr>& _sorted) {
    if (std::find(_sorted.begin(), _sorted.end(), _pDecl) != _sorted.end())
        return;
    std::pair<Graph::iterator, Graph::iterator> its = m_deps.equal_range(_pDecl);
    for (Graph::iterator i = its.first; i != its.second; ++i)
        _topologicalSort(i->second, _sorted);
    _sorted.push_back(_pDecl);
}

void Context::sortModule(Module &_module, std::list<NodePtr>& _sorted) {
    _buildDependencies(&_module);

    if (!_module.getPredicates().empty())
        for (size_t i = _module.getPredicates().size() - 1; i > 0; --i)
            m_deps.insert(std::pair<NodePtr, NodePtr>(_module.getPredicates().get(i), _module.getPredicates().get(i-1)));

    std::pair<Graph::iterator, Graph::iterator> its = m_decls.equal_range(&_module);
    for (Graph::iterator i = its.first; i != its.second; ++i)
        _topologicalSort(i->second, _sorted);
}

void Context::clear() {
    m_deps.clear();
    m_decls.clear();
    m_usedLabels.clear();
    m_identifiers.clear();
    m_usedIdentifiers.clear();
    m_nLastFoundIdentifier = 0;
}

// NODE / MODULE
void PrettyPrinterSyntax::printDeclarationGroup(Module &_module) {
    std::list<NodePtr> sorted;
    m_pContext->sortModule(_module, sorted);

    for (std::list<NodePtr>::iterator i = sorted.begin(); i != sorted.end(); ++i) {
        m_os << fmtIndent();
        mergeLines();
        traverseNode(**i);
        separateLines();
        m_os << ((*i)->getNodeKind() == Node::STATEMENT
            && (*i).as<Statement>()->getKind() != Statement::PREDICATE_DECLARATION
            ? L";\n" : L"\n");
    }
}

bool PrettyPrinterSyntax::_traverseDeclarationGroup(DeclarationGroup &_decl) {
    m_os << fmtIndent();
    mergeLines();
    VISITOR_TRAVERSE_COL(LemmaDeclaration, LemmaDecl, _decl.getLemmas());
    VISITOR_TRAVERSE_COL(Message, MessageDecl, _decl.getMessages());
    VISITOR_TRAVERSE_COL(Process, ProcessDecl, _decl.getProcesses());
    separateLines();
    return true;
}

bool PrettyPrinterSyntax::traverseModule(Module &_module) {
    VISITOR_ENTER(Module, _module);

    const bool bNeedsIndent = !_module.getName().empty();

    if (bNeedsIndent) {
        m_os << fmtIndent(L"module ") << _module.getName();
        if (!_module.getParams().empty()) {
            m_os << L"(";
            VISITOR_TRAVERSE_COL(Param, ModuleParam, _module.getParams());
            m_os << L")";
        }
        m_os << L" {\n";
        incTab();
    }

    printDeclarationGroup(_module);
    if (!_traverseDeclarationGroup(_module))
        return false;

    VISITOR_TRAVERSE_COL(Class, ClassDecl, _module.getClasses());

    if (bNeedsIndent) {
        decTab();
        m_os << fmtIndent(L"}\n");
    }

    VISITOR_EXIT();
}

// NODE / LABEL
bool PrettyPrinterSyntax::visitLabel(ir::Label &_label) {
    m_pContext->addLabel(_label.getName());
    m_os << _label.getName() << ": ";
    return true;
}

// NODE / TYPE
static std::wstring fmtBits(int _bits) {
    switch (_bits) {
        case Number::GENERIC: return L"generic"; break;
        case Number::NATIVE:  return L"native";  break;
        default:              return fmtInt(_bits);
    }
}

bool PrettyPrinterSyntax::traverseType(Type &_type) {
    if (&_type != NULL)
        return Visitor::traverseType(_type);

    m_os << L"NULL";
    return true;
}

bool PrettyPrinterSyntax::visitType(Type &_type) {
    if ((m_nFlags & PPC_NO_INCOMPLETE_TYPES) && (_type.getKind() == Type::FRESH || _type.getKind() == Type::GENERIC))
        return true;

    if (_type.getKind() == Type::FRESH) {
        m_os << fmtFreshType((tc::FreshType &)_type);
    } else if (_type.getKind() <= Type::GENERIC) {
        m_os << fmtType(_type.getKind());

        if (_type.getKind() >= Type::NAT && _type.getKind() <= Type::REAL && _type.getBits() != Number::GENERIC) {
            if ((_type.getKind() <= Type::INT && !(m_nFlags & PPC_NO_INT_BITS)) ||
                    ((_type.getKind() == Type::REAL && !(m_nFlags & PPC_NO_REAL_BITS))))
                m_os << L"(" << fmtBits(_type.getBits()) << L")";
        }
    }

    return true;
}

bool PrettyPrinterSyntax::visitTypeType(TypeType &_type) {
    m_os << L"type";
    return false;
}

bool PrettyPrinterSyntax::visitNamedReferenceType(NamedReferenceType &_type) {
    m_os << _type.getName();
    return true;
}

bool PrettyPrinterSyntax::visitSetType(SetType &_type) {
    m_os << (m_bCompact ? L"{" : L"set(");
    traverseType(*_type.getBaseType());
    m_os << (m_bCompact ? L"}" : L")");
    return false;
}

bool PrettyPrinterSyntax::visitListType(ListType &_type) {
    m_os << (m_bCompact ? L"[[" : L"list(");
    traverseType(*_type.getBaseType());
    m_os << (m_bCompact ? L"]]" : L")");
    return false;
}

bool PrettyPrinterSyntax::traverseMapType(MapType &_type) {
    VISITOR_ENTER(MapType, _type);
    m_os << (m_bCompact ? L"{" : L"map(");
    VISITOR_TRAVERSE(Type, MapIndexType, _type.getIndexType(), _type, MapType, setIndexType);
    m_os << (m_bCompact ? L" : " : L", ");
    VISITOR_TRAVERSE(Type, MapBaseType, _type.getBaseType(), _type, DerivedType, setBaseType);
    m_os << (m_bCompact ? L"}" : L")");
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseSubtype(Subtype &_type) {
    VISITOR_ENTER(Subtype, _type);
    m_os << (m_bCompact ? L"{" : L"subtype(");
    VISITOR_TRAVERSE(NamedValue, SubtypeParam, _type.getParam(), _type, Subtype, setParam);
    m_os << (m_bCompact ? L" | " : L": ");
    VISITOR_TRAVERSE(Expression, SubtypeCond, _type.getExpression(), _type, Subtype, setExpression);
    m_os << (m_bCompact ? L"}" : L")");
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseRange(Range &_type) {
    VISITOR_ENTER(Range, _type);
    VISITOR_TRAVERSE(Expression, RangeMin, _type.getMin(), _type, Range, setMin);
    m_os << L"..";
    VISITOR_TRAVERSE(Expression, RangeMax, _type.getMax(), _type, Range, setMax);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::visitArrayType(ArrayType &_type) {
    const TypePtr& pBaseType = _type.getRootType();

    if (!m_bCompact)
        m_os << L"array(";
    traverseType(*pBaseType);

    if (!m_bCompact)
        m_os << ", ";

    std::list<TypePtr> dims;
    _type.getDimensions(dims);

    for (std::list<TypePtr>::iterator i = dims.begin(); i != dims.end(); ++i) {
        if (m_bCompact)
            m_os << L"[";

        TypePtr pDim = *i;
        if (pDim->getKind() == Type::SUBTYPE) {
            RangePtr pRange = pDim.as<Subtype>()->asRange();
            if (pRange)
                pDim = pRange;
        }
        traverseType(*pDim);

        if (m_bCompact)
            m_os << L"]";

        const bool bIsLast = (*i == dims.back());
        if (!bIsLast && !m_bCompact)
            m_os << L", ";
    }

    if (!m_bCompact)
        m_os << L")";

    return false;
}

bool PrettyPrinterSyntax::traverseEnumType(EnumType &_type) {
    VISITOR_ENTER(EnumType, _type);

    m_os << L"enum(";
    for (size_t i = 0; i < _type.getValues().size(); ++i) {
        m_os << _type.getValues().get(i)->getName();
        const bool bIsLast = (_type.getValues().size() - 1 == i);
        if (!bIsLast)
            m_os << L", ";
    }
    m_os << L")";

    VISITOR_EXIT();
}

void PrettyPrinterSyntax::printStructNamedValues(const NamedValues& _nvs, std::set<std::wstring>& _usedNames, bool& _bIsFirst, bool _bNeedsIndent) {
    for (size_t i = 0; i < _nvs.size(); ++i) {
        if (!_bIsFirst)
            m_os << L", ";
        if (_bNeedsIndent)
            m_os << L"\n" << fmtIndent();
        traverseNamedValue(*_nvs.get(i));
        _usedNames.insert(_nvs.get(i)->getName());
        _bIsFirst = false;
    }
}

bool PrettyPrinterSyntax::needsIndent() {
    switch (getRole()) {
        case R_FormulaCallArgs:
        case R_FunctionCallArgs:
        case R_PredicateInParam:
        case R_PredicateOutParam:
        case R_PredicateCallArgs:
        case R_PredicateCallBranchResults:
            return false;
    }
    return !m_bCompact && !m_bSingleLine;
}

#define INDENT()                                \
    const bool bNeedsIndent = needsIndent();    \
    const bool oldSingleLine = m_bSingleLine;   \
    if (bNeedsIndent) {                         \
        incTab();                               \
        m_bSingleLine = true;                   \
    }

#define UNINDENT()                              \
    if (bNeedsIndent) {                         \
        decTab();                               \
        m_os << L"\n" << fmtIndent();           \
        m_bSingleLine = oldSingleLine;          \
    }

std::wstring getNewFieldName(std::set<std::wstring>& _usedNames) {
    size_t i = 1;
    std::wstring strName;

    do {
        strName = intToAlpha(i++);
    } while (!_usedNames.insert(strName).second);

    return strName;
}

bool PrettyPrinterSyntax::traverseStructType(StructType &_type) {
    VISITOR_ENTER(StructType, _type);

    m_os << (!m_bCompact ? L"struct(" : L"(");

    INDENT();

    bool bIsFirst = true;
    std::set<std::wstring> usedNames;
    printStructNamedValues(_type.getNamesOrd(), usedNames, bIsFirst, bNeedsIndent);

    if (!_type.getNamesSet().empty()) {
        // Ensure sorting for debug purposes (don't reorder source collection though).
        std::map<std::wstring, NamedValuePtr> sortedFieldsMap;
        NamedValues sortedFields;

        for (size_t i = 0; i < _type.getNamesSet().size(); ++i)
            sortedFieldsMap[_type.getNamesSet().get(i)->getName()] = _type.getNamesSet().get(i);

        for (std::map<std::wstring, NamedValuePtr>::iterator i = sortedFieldsMap.begin();
                i != sortedFieldsMap.end(); ++i)
            sortedFields.add(i->second);

        printStructNamedValues(sortedFields, usedNames, bIsFirst, bNeedsIndent);
        if (m_bCompact) {
            m_os << L"; ";
            bIsFirst = true;
        }
    }

    NamedValues typeFields;
    for (size_t i = 0; i < _type.getTypesOrd().size(); ++i) {
        if (_type.getTypesOrd().get(i)->getName().empty())
            typeFields.add(new NamedValue(getNewFieldName(usedNames), _type.getTypesOrd().get(i)->getType()));
        else
            typeFields.add(_type.getTypesOrd().get(i));
    }
    printStructNamedValues(typeFields, usedNames, bIsFirst, bNeedsIndent);

    UNINDENT();
    m_os << L")";

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseUnionConstructorDeclaration(UnionConstructorDeclaration &_cons) {
    VISITOR_ENTER(UnionConstructorDeclaration, _cons);
    m_os << _cons.getName() << L"(";
    VISITOR_TRAVERSE_COL(NamedValue, UnionConsField, _cons.getFields());
    m_os << ")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseUnionType(UnionType &_type) {
    VISITOR_ENTER(UnionType, _type);
    m_os << L"union(";

    INDENT();

    for (size_t i = 0; i < _type.getConstructors().size(); ++i) {
        if (bNeedsIndent)
            m_os << L"\n" << fmtIndent();

        traverseUnionConstructorDeclaration(*_type.getConstructors().get(i));

        const bool bIsLast = (_type.getConstructors().size() - 1 == i);
        if (!bIsLast)
            m_os << ", ";
    }

    UNINDENT();
    m_os << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traversePredicateType(PredicateType &_type) {
    VISITOR_ENTER(PredicateType, _type);
    m_os << L"predicate(";
    VISITOR_TRAVERSE_COL(Param, PredicateTypeInParam, _type.getInParams());

    for (size_t i = 0; i < _type.getOutParams().size(); ++i) {
        m_os << " : ";
        for (size_t j = 0; j < _type.getOutParams().get(i)->size(); ++j) {
            if (j != 0)
                m_os << L", ";
            traverseType(*_type.getOutParams().get(i)->get(j)->getType());
            m_os << L" " << _type.getOutParams().get(i)->get(j)->getName();
        }
        if (_type.getOutParams().get(i)->getLabel() && !_type.getOutParams().get(i)->getLabel()->getName().empty())
            m_os << L" #" << _type.getOutParams().get(i)->getLabel()->getName();
    }

    m_os << L")";

    if (m_bCompact)
        VISITOR_EXIT();

    if (_type.getPreCondition()) {
        m_os << L" pre ";
        VISITOR_TRAVERSE(Formula, PredicatePreCondition, _type.getPreCondition(), _type, PredicateType, setPreCondition);
    }

    Branches& branches = _type.getOutParams();
    for (size_t i = 0; i < branches.size(); ++i) {
        if (!branches.get(i) || !branches.get(i)->getPreCondition() || !branches.get(i)->getLabel())
            continue;
        Branch& br = *branches.get(i);
        m_os << L" pre " << br.getLabel()->getName() << L": ";
        VISITOR_TRAVERSE(Formula, PredicatePreCondition, br.getPreCondition(), br, Branch, setPreCondition);
    }
    for (size_t i = 0; i < branches.size(); ++i) {
        if (!branches.get(i) || !branches.get(i)->getPostCondition() || !branches.get(i)->getLabel())
            continue;
        Branch& br = *branches.get(i);
        m_os << L" post " << br.getLabel()->getName() << L": ";
        VISITOR_TRAVERSE(Formula, PredicatePostCondition, br.getPostCondition(), br, Branch, setPostCondition);
    }

    if (_type.getPostCondition()) {
        m_os << L" post ";
        VISITOR_TRAVERSE(Formula, PredicatePostCondition, _type.getPostCondition(), _type, PredicateType, setPostCondition);
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseParameterizedType(ParameterizedType &_type) {
    VISITOR_ENTER(ParameterizedType, _type);
    VISITOR_TRAVERSE(Type, ParameterizedTypeBase, _type.getActualType(), _type, ParameterizedType, setActualType);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::visitSeqType(SeqType& _type) {
    assert(false);
    return false;
}

bool PrettyPrinterSyntax::visitOptionalType(OptionalType& _type) {
    assert(false);
    return false;
}

// NODE / STATEMENT
bool PrettyPrinterSyntax::traverseStatement(Statement &_stmt) {
    if (&_stmt == NULL)
        return false;
    if (!m_bMergeLines
        && _stmt.getKind() != Statement::PARALLEL_BLOCK
        && _stmt.getKind() != Statement::VARIABLE_DECLARATION_GROUP)
        m_os << fmtIndent();
    separateLines();
    return Visitor::traverseStatement(_stmt);
}

bool PrettyPrinterSyntax::traverseJump(Jump &_stmt) {
    VISITOR_ENTER(Jump, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
    m_os << L"#" << _stmt.getDestination()->getName();
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseBlock(Block &_stmt) {
    VISITOR_ENTER(Block, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    m_os << L"{\n";
    incTab();

    for (size_t i = 0; i < _stmt.size(); ++i) {
        const bool bIsLast = i != _stmt.size() - 1;
        traverseStatement(*_stmt.get(i));
        m_os << (bIsLast && !_stmt.get(i)->isBlockLike() ? L";\n" : L"\n");
    }

    decTab();
    m_os << fmtIndent(L"}");

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseParallelBlock(ParallelBlock &_stmt) {
    VISITOR_ENTER(ParallelBlock, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    for (size_t i = 0; i < _stmt.size(); ++i) {
        if (i > 0) {
            m_os << fmtIndent(L"|| ");
            mergeLines();
        }
        traverseStatement(*_stmt.get(i));
        if (i != _stmt.size() - 1)
            m_os << L"\n";
        if (i == 0 && _stmt.size() != 1)
            incTab();
    }

    if (_stmt.size() != 1)
        decTab();

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::_traverseAnonymousPredicate(AnonymousPredicate &_decl) {
    m_os << L"(";
    VISITOR_TRAVERSE_COL(Param, PredicateInParam, _decl.getInParams());
    m_os << L" : ";

    bool bHavePreConditions = false, bHavePostConditions = false;

    for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
        Branch &br = *_decl.getOutParams().get(i);
        bHavePreConditions = bHavePreConditions || br.getPreCondition();
        bHavePostConditions = bHavePostConditions || br.getPostCondition();
        VISITOR_TRAVERSE_COL(Param, PredicateOutParam, br);
        if (br.getLabel() && !br.getLabel()->getName().empty())
            m_os << L" #" << br.getLabel()->getName();
        if (i < _decl.getOutParams().size() - 1)
            m_os << L" : ";
    }

    bHavePreConditions = bHavePreConditions || _decl.getPreCondition();
    bHavePostConditions = bHavePostConditions || _decl.getPostCondition();

    m_os << L") ";

    if (bHavePreConditions) {
        m_os << L"\n";
        if (_decl.getPreCondition()) {
            m_os << fmtIndent(L"pre ");
            VISITOR_TRAVERSE(Formula, PredicatePreCondition, _decl.getPreCondition(), _decl, AnonymousPredicate, setPreCondition);
            m_os << L"\n";
        }
        for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
            Branch &br = *_decl.getOutParams().get(i);
            if (br.getPreCondition()) {
                m_os << fmtIndent(L"pre ") << br.getLabel()->getName() << L": ";
                VISITOR_TRAVERSE(Formula, PredicateBranchPreCondition, br.getPreCondition(), br, Branch, setPreCondition);
                m_os << L"\n";
            }
        }
    }
    else
        mergeLines();

    traverseStatement(*_decl.getBlock());

    if (bHavePostConditions) {
        if (_decl.getPostCondition()) {
            m_os << L"\n" << fmtIndent(L"post ");
            VISITOR_TRAVERSE(Formula, PredicatePostCondition, _decl.getPostCondition(), _decl, AnonymousPredicate, setPostCondition);
        }
        for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
            Branch &br = *_decl.getOutParams().get(i);
            if (br.getPostCondition()) {
                m_os << L"\n" << fmtIndent(L"post ") << br.getLabel()->getName() << L": ";
                VISITOR_TRAVERSE(Formula, PredicateBranchPostCondition, br.getPostCondition(), br, Branch, setPostCondition);
            }
        }
    }

    if (_decl.getMeasure()) {
        m_os << L"\n" << fmtIndent(L"measure ");
        VISITOR_TRAVERSE(Expression, PredicateMeasure, _decl.getMeasure(), _decl, AnonymousPredicate, setMeasure);
    }

    if (_decl.getMeasure() || bHavePostConditions)
        m_os << L";";

    return true;
}

bool PrettyPrinterSyntax::traversePredicate(Predicate &_stmt) {
    VISITOR_ENTER(Predicate, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    m_os << _stmt.getName();

    if (!_traverseAnonymousPredicate(_stmt))
        return false;

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseAssignment(ir::Assignment &_stmt) {
    VISITOR_ENTER(Assignment, _stmt);

    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
    VISITOR_TRAVERSE(Expression, LValue, _stmt.getLValue(), _stmt, Assignment, setLValue);
    m_os << " = ";
    VISITOR_TRAVERSE(Expression, RValue, _stmt.getExpression(), _stmt, Assignment, setExpression);

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseMultiassignment(Multiassignment &_stmt) {
    VISITOR_ENTER(Multiassignment, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    VISITOR_TRAVERSE_COL(Expression, LValue, _stmt.getLValues());
    m_os << " = ";
    VISITOR_TRAVERSE_COL(Expression, RValue, _stmt.getExpressions());

    VISITOR_EXIT();
}

VariableDeclarationPtr getVarDecl(const Call& _call, const ExpressionPtr& _pArg) {
    if (!_pArg || _pArg->getKind() != Expression::VAR)
        return NULL;
    for (size_t i = 0; i < _call.getDeclarations().size(); ++i) {
        VariableDeclarationPtr pVar = _call.getDeclarations().get(i);
        if (pVar->getVariable() == _pArg.as<VariableReference>()->getTarget())
            return pVar;
    }
    return NULL;
}

bool PrettyPrinterSyntax::traverseCall(Call &_stmt) {
    VISITOR_ENTER(Call, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
    VISITOR_TRAVERSE(Expression, PredicateCallee, _stmt.getPredicate(), _stmt, Call, setPredicate);

    m_os << L"(";
    VISITOR_TRAVERSE_COL(Expression, PredicateCallArgs, _stmt.getArgs());

    if (_stmt.getBranches().size() == 0) {
        m_os << L")";
        VISITOR_EXIT();
    }

    m_os << L": ";

    if (_stmt.getBranches().size() == 1) {
        traverseCollection(*_stmt.getBranches().get(0));
        m_os << L")";
        VISITOR_EXIT();
    }

    const std::wstring strGeneralName = _stmt.getPredicate()->getKind() == Expression::PREDICATE
        ? _stmt.getPredicate().as<PredicateReference>()->getName()
        : L"";

    std::vector<std::wstring> names;
    for (size_t i = 0; i < _stmt.getBranches().size(); ++i)
        names.push_back(m_pContext->getNewLabelName(strGeneralName));

    for (size_t i = 0; i < _stmt.getBranches().size(); ++i) {
        CallBranch &br = *_stmt.getBranches().get(i);

        bool bIsFirst = true;
        for (size_t j = 0; j < br.size(); ++j) {
            ExpressionPtr pArg = br.get(j);
            if (!pArg)
                continue;

            if (!bIsFirst)
                m_os << L", ";
            bIsFirst = false;

            if (VariableDeclarationPtr pVar = getVarDecl(_stmt, pArg))
                traverseVariableDeclaration(*pVar);
            else
                traverseExpression(*pArg);
        }

        if (_stmt.getBranches().size() > 1)
            m_os << L" #" << names[i];
        if (i != _stmt.getBranches().size() - 1)
            m_os << L" : ";
    }

    m_os << L")";

    bool bHasHandlers = false;
    for (size_t i = 0; i < _stmt.getBranches().size(); ++i)
        if (_stmt.getBranches().get(i)->getHandler()) {
            bHasHandlers = true;
            break;
        }


    for (size_t i = 0; i < _stmt.getBranches().size(); ++i) {
        CallBranch &br = *_stmt.getBranches().get(i);
        if (!br.getHandler())
            continue;

        incTab();

        m_os << "\n" << fmtIndent() << L"case " << names[i] << ": ";
        mergeLines();
        traverseStatement(*br.getHandler());
        decTab();
    }

    VISITOR_EXIT();
}

void PrettyPrinterSyntax::printMergedStatement(const StatementPtr _pStmt) {
    if (_pStmt->getKind() == Statement::BLOCK) {
        mergeLines();
        traverseStatement(*_pStmt);
    } else {
        incTab();
        m_os << L"\n";
        traverseStatement(*_pStmt);
        decTab();
    }
}

void PrettyPrinterSyntax::feedLine(const Statement& _stmt) {
    if (_stmt.getLabel())
        m_os << L"\n" << fmtIndent();
}

bool PrettyPrinterSyntax::traverseIf(If &_stmt) {
    VISITOR_ENTER(If, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    feedLine(_stmt);

    m_os << L"if (";
    VISITOR_TRAVERSE(Expression, IfArg, _stmt.getArg(), _stmt, If, setArg);
    m_os << L") ";

    if (_stmt.getBody())
        printMergedStatement(_stmt.getBody());

    if (_stmt.getElse()) {
        m_os << L"\n" << fmtIndent(L"else ");
        printMergedStatement(_stmt.getElse());
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseSwitch(Switch &_stmt) {
    VISITOR_ENTER(Switch, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    feedLine(_stmt);

    m_os << L"switch (";
    if (_stmt.getParamDecl()) {
        mergeLines();
        VISITOR_TRAVERSE(VariableDeclaration, SwitchParamDecl, _stmt.getParamDecl(), _stmt, Switch, setParamDecl);
    }
    else
        VISITOR_TRAVERSE(Expression, SwitchArg, _stmt.getArg(), _stmt, Switch, setArg);
    m_os << L") {\n";
    incTab();

    for (size_t i=0; i < _stmt.size(); ++i) {
        if (!_stmt.get(i))
            continue;

        m_os << fmtIndent(L"case ");
        traverseCollection(_stmt.get(i)->getExpressions());
        m_os << L" : ";
        mergeLines();

        if (_stmt.get(i)->getBody())
            traverseStatement(*_stmt.get(i)->getBody());

        m_os << "\n";
    }

    if (_stmt.getDefault()) {
        m_os << fmtIndent(L"default: ");
        mergeLines();
        VISITOR_TRAVERSE(Statement, SwitchDefault, _stmt.getDefault(), _stmt, Switch, setDefault);
        m_os << L"\n";
    }

    decTab();
    m_os << fmtIndent(L"}");
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFor(For &_stmt) {
    VISITOR_ENTER(For, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    feedLine(_stmt);

    m_os << L"for (";
    VISITOR_TRAVERSE(VariableDeclaration, ForIterator, _stmt.getIterator(), _stmt, For, setIterator);
    m_os << L"; ";
    VISITOR_TRAVERSE(Expression, ForInvariant, _stmt.getInvariant(), _stmt, For, setInvariant);
    m_os << L"; ";
    mergeLines();
    VISITOR_TRAVERSE(Statement, ForIncrement, _stmt.getIncrement(), _stmt, For, setIncrement);
    m_os << L") ";

    if (_stmt.getBody())
        printMergedStatement(_stmt.getBody());

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseWhile(While &_stmt) {
    VISITOR_ENTER(While, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    feedLine(_stmt);

    m_os << L"while (";
    VISITOR_TRAVERSE(Expression, WhileInvariant, _stmt.getInvariant(), _stmt, While, setInvariant);
    m_os << L") ";

    if (_stmt.getBody())
        printMergedStatement(_stmt.getBody());

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseBreak(Break &_stmt) {
    VISITOR_ENTER(Break, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
    m_os << L"break";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseWith(With &_stmt) {
    VISITOR_ENTER(With, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    feedLine(_stmt);

    m_os << L"with (";
    VISITOR_TRAVERSE_COL(Expression, WithArg, _stmt.getArgs());
    m_os << L") ";

    if (_stmt.getBody())
        printMergedStatement(_stmt.getBody());

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseTypeDeclaration(TypeDeclaration &_stmt) {
    VISITOR_ENTER(TypeDeclaration, _stmt);

    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
    m_os << L"type " << _stmt.getName();

    if (!_stmt.getType())
        VISITOR_EXIT();

    if (_stmt.getType()->getKind() == Type::PARAMETERIZED) {
        m_os << L"(";
        traverseCollection(_stmt.getType().as<ParameterizedType>()->getParams());
        m_os << L")";
    }

    m_os << L" = ";
    VISITOR_TRAVERSE(Type, TypeDeclBody, _stmt.getType(), _stmt, TypeDeclaration, setType);

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseVariable(Variable &_val) {
    VISITOR_ENTER(Variable, _val);
    if (_val.isMutable())
        m_os << L"mutable ";
    VISITOR_TRAVERSE(Type, VariableType, _val.getType(), _val, NamedValue, setType);
    m_os << L" " << _val.getName();
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseVariableDeclaration(VariableDeclaration &_stmt) {
    VISITOR_ENTER(VariableDeclaration, _stmt);
    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    if (_stmt.getVariable()->isMutable())
        m_os << "mutable ";

    VISITOR_TRAVERSE(Variable, VarDeclVar, _stmt.getVariable(), _stmt, VariableDeclaration, setVariable);

    if (_stmt.getValue()) {
        m_os << L" = ";
        VISITOR_TRAVERSE(Expression, VarDeclInit, _stmt.getValue(), _stmt, VariableDeclaration, setValue);
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseVariableDeclarationGroup(VariableDeclarationGroup &_stmt) {
    VISITOR_ENTER(Block, _stmt);

    if (_stmt.empty())
        VISITOR_EXIT();

    m_os << fmtIndent();
    traverseType(*_stmt.get(0)->getVariable()->getType());
    m_os << L" ";

    for (size_t i = 0; i < _stmt.size(); ++i) {
        const bool bIsLast = (_stmt.size() - 1 == i);
        VariableDeclaration var = *_stmt.get(i);

        m_os << var.getName();
        if (var.getValue()) {
            m_os << L" = ";
            traverseExpression(*var.getValue());
        }
        if (!bIsLast)
            m_os << L", ";
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFormulaDeclaration(FormulaDeclaration &_node) {
    VISITOR_ENTER(FormulaDeclaration, _node);
    VISITOR_TRAVERSE(Label, StmtLabel, _node.getLabel(), _node, Statement, setLabel);

    m_os << "formula " << _node.getName();

    if (!_node.getParams().empty() || _node.getResultType()) {
        m_os << "(";
        VISITOR_TRAVERSE_COL(NamedValue, FormulaDeclParams, _node.getParams());

        if (_node.getResultType())
            m_os << " : ";

        VISITOR_TRAVERSE(Type, FormulaDeclResultType, _node.getResultType(), _node, FormulaDeclaration, setResultType);
        m_os << ")";
    }

    m_os << " = ";
    VISITOR_TRAVERSE(Expression, FormulaDeclBody, _node.getFormula(), _node, FormulaDeclaration, setFormula);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseLemmaDeclaration(LemmaDeclaration &_stmt) {
    VISITOR_ENTER(LemmaDeclaration, _stmt);

    VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

    m_os << "lemma ";

    switch (_stmt.getStatus()) {
        case LemmaDeclaration::VALID:
            m_os << "valid ";
            break;
        case LemmaDeclaration::INVALID:
            m_os << "invalid ";
            break;
        case LemmaDeclaration::UNKNOWN:
            m_os << "unknown ";
            break;
    }

    VISITOR_TRAVERSE(Expression, LemmaDeclBody, _stmt.getProposition(), _stmt, LemmaDeclaration, setProposition);
    m_os << ";\n";

    VISITOR_EXIT();
}

// NODE / NAMED_VALUE
bool PrettyPrinterSyntax::visitNamedValue(NamedValue &_val) {
    if (!m_path.empty()) {
        if (getLoc().bPartOfCollection && getLoc().cPosInCollection != 0)
            m_os << L", ";
        else if (getLoc().role == R_PredicateTypeOutParam && getLoc().cPosInCollection == 0)
            m_os << L" : ";
    }

    if (getRole() != R_EnumValueDecl)
        traverseType(*_val.getType());

    if (getLoc().type == N_Param)
        return false;
    if (getRole() != R_EnumValueDecl)
        m_os << L" ";

    m_os << m_pContext->getNamedValueName(_val);

    return false;
}

// NODE / EXPRESSION
void PrettyPrinterSyntax::printLiteralKind(ir::Literal &_node) {
    switch (_node.getLiteralKind()) {
        case ir::Literal::UNIT:
            m_os << L"0";
            break;
        case ir::Literal::NUMBER:
            m_os << _node.getNumber().toString();
            break;
        case ir::Literal::BOOL:
            m_os << (_node.getBool() ? L"true" : L"false");
            break;
        case ir::Literal::CHAR:
            m_os <<  L"\'" << _node.getChar() << L"\'";
            break;
        case ir::Literal::STRING:
            m_os <<  L"\"" << _node.getString().c_str() << L"\"";
            break;
    }
}

void PrettyPrinterSyntax::printUnaryOperator(ir::Unary &_node) {
    switch (_node.getOperator()) {
        case ir::Unary::PLUS:
            m_os << L"+";
            break;
        case ir::Unary::MINUS:
            m_os << L"-";
            break;
        case ir::Unary::BOOL_NEGATE:
            m_os << L"!";
            break;
        case ir::Unary::BITWISE_NEGATE:
            m_os << L"~";
            break;
    }
}

void PrettyPrinterSyntax::printBinaryOperator(ir::Binary &_node) {
    switch (_node.getOperator()) {
        case ir::Binary::ADD:
            m_os << L"+";
            break;
        case ir::Binary::SUBTRACT:
            m_os << L"-";
            break;
        case ir::Binary::MULTIPLY:
            m_os << L"*";
            break;
        case ir::Binary::DIVIDE:
            m_os << L"/";
            break;
        case ir::Binary::REMAINDER:
            m_os << L"%";
            break;
        case ir::Binary::SHIFT_LEFT:
            m_os << L"<<";
            break;
        case ir::Binary::SHIFT_RIGHT:
            m_os << L">>";
            break;
        case ir::Binary::POWER:
            m_os << L"^";
            break;
        case ir::Binary::BOOL_AND:
            m_os << L"&";
            break;
        case ir::Binary::BOOL_OR:
            m_os << L"or";
            break;
        case ir::Binary::BOOL_XOR:
            m_os << L"^";
            break;
        case ir::Binary::EQUALS:
            m_os << L"=";
            break;
        case ir::Binary::NOT_EQUALS:
            m_os << L"!=";
            break;
        case ir::Binary::LESS:
            m_os << L"<";
            break;
        case ir::Binary::LESS_OR_EQUALS:
            m_os << L"<=";
            break;
        case ir::Binary::GREATER:
            m_os << L">";
            break;
        case ir::Binary::GREATER_OR_EQUALS:
            m_os << L">=";
            break;
        case ir::Binary::IMPLIES:
            m_os << L"=>";
            break;
        case ir::Binary::IFF:
            m_os << L"<=>";
            break;
    }
}

void PrettyPrinterSyntax::printQuantifier(int _quantifier) {
    switch (_quantifier) {
        case ir::Formula::NONE:
            m_os << "none";
            break;
        case ir::Formula::UNIVERSAL:
            m_os << "forall";
            break;
        case ir::Formula::EXISTENTIAL:
            m_os << "exists";
            break;
    }
}

void PrettyPrinterSyntax::printComma() {
    if (m_path.empty())
        return;
    if (getLoc().bPartOfCollection && getLoc().cPosInCollection > 0)
        m_os << ", ";
}

ir::ExpressionPtr PrettyPrinterSyntax::getChild() {
    std::list<Visitor::Loc>::iterator i = ::prev(m_path.end());
    if (i->pNode->getNodeKind() != ir::Node::EXPRESSION)
        return NULL;
    return i->pNode;
}

ir::ExpressionPtr PrettyPrinterSyntax::getParent() {
    std::list<Visitor::Loc>::iterator i = ::prev(m_path.end());
    if (i->pNode->getNodeKind() != ir::Node::EXPRESSION)
        return NULL;
    if (i == m_path.begin())
        return NULL;
    else
        --i;
    if (i->pNode->getNodeKind() != ir::Node::EXPRESSION)
        return NULL;
    return i->pNode;
}

int PrettyPrinterSyntax::getParentKind() {
    const ir::ExpressionPtr pParent = getParent();
    if (pParent)
        return pParent->getKind();
    else
        return -1;
}

int PrettyPrinterSyntax::getChildKind() {
    const ir::ExpressionPtr pChild = getChild();
    if (pChild)
        return pChild->getKind();
    else
        return -1;
}

bool PrettyPrinterSyntax::needsParen() {

    const int nChildKind = getChildKind();
    if (nChildKind == -1)
        return false;

    switch (nChildKind) {
        case ir::Expression::FORMULA_CALL:
        case ir::Expression::FUNCTION_CALL:
        case ir::Expression::LITERAL:
        case ir::Expression::VAR:
            return false;
        default:
            break;
    }

    const int nParentKind = getParentKind();
    if (nParentKind == -1)
        return false;

    if (nParentKind == nChildKind) {
        if (nParentKind == ir::Expression::BINARY) {
            const ir::BinaryPtr pChild = getChild().as<Binary>();
            const ir::BinaryPtr pParent = getParent().as<Binary>();
            return (Binary::getPrecedence(pChild->getOperator()) < Binary::getPrecedence(pParent->getOperator()));
        }
        return true;
    }

    switch (nParentKind) {
        case ir::Expression::UNARY:
        case ir::Expression::BINARY:
        case ir::Expression::TERNARY:
            break;

        default:
            return false;
    }

    return true;

}

bool PrettyPrinterSyntax::traverseExpression(ir::Expression &_node) {

    printComma();

    bool bParen;
    if (m_path.empty())
        bParen = false;
    else
        bParen = needsParen();

    if (bParen)
        m_os << "(";
    const bool result = Visitor::traverseExpression(_node);
    if (bParen)
        m_os << ")";

    return result;

}

bool PrettyPrinterSyntax::visitLiteral(ir::Literal &_node) {
    printLiteralKind(_node);
    return true;
}

bool PrettyPrinterSyntax::visitVariableReference(ir::VariableReference &_node) {
    m_os << (_node.getName().empty() ? m_pContext->getNamedValueName(*_node.getTarget()) : _node.getName());
    return false;
}

bool PrettyPrinterSyntax::visitPredicateReference(ir::PredicateReference &_node) {
    m_os << _node.getName();
    return true;
}

bool PrettyPrinterSyntax::visitLambda(ir::Lambda &_node) {
    m_os << L"predicate";
    return true;
}

bool PrettyPrinterSyntax::traverseBinder(Binder &_expr) {
    VISITOR_ENTER(Binder, _expr);
    VISITOR_TRAVERSE(Expression, BinderCallee, _expr.getPredicate(), _expr, Binder, setPredicate);

    m_os << L"(";
    for (size_t i = 0; i < _expr.getArgs().size(); ++i) {
        if (_expr.getArgs().get(i))
            traverseExpression(*_expr.getArgs().get(i));
        else
            m_os << L"_";
        const bool bIsLast = (_expr.getArgs().size() - 1 == i);
        if (!bIsLast)
            m_os << L", ";
    }
    m_os << L")";

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::visitUnary(ir::Unary &_node) {
    printUnaryOperator(_node);
    return true;
}

bool PrettyPrinterSyntax::traverseBinary(ir::Binary &_node) {
    VISITOR_ENTER(Binary, _node);
    VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getLeftSide(), _node, Binary, setLeftSide);

    m_os << " ";
    printBinaryOperator(_node);
    m_os << " ";

    VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getRightSide(), _node, Binary, setRightSide);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseTernary(ir::Ternary &_node) {
    VISITOR_ENTER(Ternary, _node);

    VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getIf(), _node, Ternary, setIf);
    m_os << " ? ";
    VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getThen(), _node, Ternary, setThen);
    m_os << " : ";
    VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getElse(), _node, Ternary, setElse);

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFormula(ir::Formula &_node) {
    VISITOR_ENTER(Formula, _node);

    const int nQuantifier = _node.getQuantifier();

    if (nQuantifier != ir::Formula::NONE) {
        printQuantifier(nQuantifier);
        m_os << " ";
        VISITOR_TRAVERSE_COL(NamedValue, FormulaBoundVariable, _node.getBoundVariables());
        m_os << ". ";
    }

    VISITOR_TRAVERSE(Expression, Subformula, _node.getSubformula(), _node, Formula, setSubformula);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseReplacement(Replacement &_expr) {
    VISITOR_ENTER(Replacement, _expr);
    VISITOR_TRAVERSE(Expression, ReplacementObject, _expr.getObject(), _expr, Component, setObject);
    m_os << ".";
    VISITOR_TRAVERSE(Constructor, ReplacementValue, _expr.getNewValues(), _expr, Replacement, setNewValues);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFunctionCall(ir::FunctionCall &_expr) {
    VISITOR_ENTER(FunctionCall, _expr);
    VISITOR_TRAVERSE(Expression, FunctionCallee, _expr.getPredicate(), _expr, FunctionCall, setPredicate);
    m_os << "(";
    VISITOR_TRAVERSE_COL(Expression, FunctionCallArgs, _expr.getArgs());
    m_os << ")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFormulaCall(ir::FormulaCall &_node) {
    m_os << _node.getName() << L"(";
    const bool bResult = Visitor::traverseFormulaCall(_node);
    m_os << ")";
    return bResult;
}

bool PrettyPrinterSyntax::traverseStructFieldDefinition(StructFieldDefinition &_cons) {
    VISITOR_ENTER(StructFieldDefinition, _cons);

    if (getLoc().cPosInCollection > 0)
        m_os << L", ";
    if (!_cons.getName().empty())
        m_os << _cons.getName() << L": ";

    VISITOR_TRAVERSE(Expression, StructFieldValue, _cons.getValue(), _cons, StructFieldDefinition, setValue);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseStructConstructor(StructConstructor &_expr) {
    VISITOR_ENTER(StructConstructor, _expr);
    m_os << L"(";
    VISITOR_TRAVERSE_COL(StructFieldDefinition, StructFieldDef, _expr);
    m_os << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseUnionConstructor(UnionConstructor &_expr) {
    VISITOR_ENTER(UnionConstructor, _expr);
    m_os << _expr.getName() << L"(";
    VISITOR_TRAVERSE_COL(StructFieldDefinition, UnionCostructorParam, _expr);
    m_os << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseElementDefinition(ElementDefinition &_cons) {
    VISITOR_ENTER(ElementDefinition, _cons);
    if (getLoc().cPosInCollection > 0)
        m_os << L", ";

    VISITOR_TRAVERSE(Expression, ElementIndex, _cons.getIndex(), _cons, ElementDefinition, setIndex);
    if (_cons.getIndex())
        m_os << L": ";

    VISITOR_TRAVERSE(Expression, ElementValue, _cons.getValue(), _cons, ElementDefinition, setValue);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseArrayConstructor(ArrayConstructor &_expr) {
    VISITOR_ENTER(ArrayConstructor, _expr);
    m_os << L"[ ";
    VISITOR_TRAVERSE_COL(ElementDefinition, ArrayElementDef, _expr);
    m_os << L" ]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseMapConstructor(MapConstructor &_expr) {
    VISITOR_ENTER(MapConstructor, _expr);
    m_os << L"[{";
    VISITOR_TRAVERSE_COL(ElementDefinition, MapElementDef, _expr);
    m_os << L"}]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseSetConstructor(SetConstructor &_expr) {
    VISITOR_ENTER(SetConstructor, _expr);
    m_os << L"{";
    VISITOR_TRAVERSE_COL(Expression, SetElementDef, _expr);
    m_os << L"}";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseListConstructor(ListConstructor &_expr) {
    VISITOR_ENTER(ListConstructor, _expr);
    m_os << L"[[ ";
    VISITOR_TRAVERSE_COL(Expression, ListElementDef, _expr);
    m_os << L" ]]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseArrayPartDefinition(ArrayPartDefinition &_cons) {
    VISITOR_ENTER(ArrayPartDefinition, _cons);
    m_os << L"case ";
    VISITOR_TRAVERSE_COL(Expression, ArrayPartCond, _cons.getConditions());
    m_os << L": ";
    VISITOR_TRAVERSE(Expression, ArrayPartValue, _cons.getExpression(), _cons, ArrayPartDefinition, setExpression);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseArrayIteration(ArrayIteration &_expr) {
    VISITOR_ENTER(ArrayIteration, _expr);

    m_os << L"for(";
    VISITOR_TRAVERSE_COL(NamedValue, ArrayIterator, _expr.getIterators());
    m_os << L") { ";

    INDENT();

    for (size_t i = 0; i < _expr.size(); ++i) {
        if (bNeedsIndent)
            m_os << L"\n" << fmtIndent();
        traverseArrayPartDefinition(*_expr.get(i));
        m_os << L" ";
    }

    if (_expr.getDefault()) {
        if (bNeedsIndent)
            m_os << L"\n" << fmtIndent();
        m_os << L"default: ";
        VISITOR_TRAVERSE(Expression, ArrayIterationDefault, _expr.getDefault(), _expr, ArrayIteration, setDefault);
        m_os << L" ";
    }

    UNINDENT();

    m_os << L"}";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseArrayPartExpr(ArrayPartExpr &_expr) {
    VISITOR_ENTER(ArrayPartExpr, _expr);
    VISITOR_TRAVERSE(Expression, ArrayPartObject, _expr.getObject(), _expr, Component, setObject);
    m_os << L"[ ";
    VISITOR_TRAVERSE_COL(Expression, ArrayPartIndex, _expr.getIndices());
    m_os << L" ]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFieldExpr(FieldExpr &_expr) {
    VISITOR_ENTER(FieldExpr, _expr);
    VISITOR_TRAVERSE(Expression, FieldObject, _expr.getObject(), _expr, Component, setObject);
    m_os << L"." << _expr.getFieldName();
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseMapElementExpr(MapElementExpr &_expr) {
    VISITOR_ENTER(MapElementExpr, _expr);
    VISITOR_TRAVERSE(Expression, MapElementObject, _expr.getObject(), _expr, Component, setObject);
    m_os << L"[ ";
    VISITOR_TRAVERSE(Expression, MapElementIndex, _expr.getIndex(), _expr, MapElementExpr, setIndex);
    m_os << L" ]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseListElementExpr(ListElementExpr &_expr) {
    VISITOR_ENTER(ListElementExpr, _expr);
    VISITOR_TRAVERSE(Expression, ListElementObject, _expr.getObject(), _expr, Component, setObject);
    m_os << L"[ ";
    VISITOR_TRAVERSE(Expression, ListElementIndex, _expr.getIndex(), _expr, ListElementExpr, setIndex);
    m_os << L" ]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::visitConstructor(Constructor& _expr) {
    if (getRole() == R_CastParam)
        m_os << L" ";
    return true;
}

#undef INDENT
#undef UNINDENT

void PrettyPrinterSyntax::run() {
    if (m_pNode) {
        m_pContext->collectIdentifiers(*m_pNode);
        traverseNode(*m_pNode);
    }
    else if (m_bCompact)
        m_os << L"NULL";
}

void PrettyPrinterSyntax::print(Node &_node) {
    if (&_node == NULL)
        m_os << "NULL";
    else {
        m_pContext->collectIdentifiers(_node);
        traverseNode(_node);
    }
}

size_t PrettyPrinterSyntax::getDepth() const {
    return m_szDepth;
}

std::wstring PrettyPrinterSyntax::fmtIndent(const std::wstring &_s) {
    std::wstring res;

    for (size_t i = 0; i < getDepth(); ++i)
        res += L" ";

    return res + _s;
}

void PrettyPrinterSyntax::incTab() {
    m_szDepth += 4;
}

void PrettyPrinterSyntax::decTab() {
    if (m_szDepth > 4)
        m_szDepth -= 4;
    else
        m_szDepth = 0;
}

void PrettyPrinterSyntax::mergeLines() {
    m_bMergeLines = true;
}
void PrettyPrinterSyntax::separateLines() {
    m_bMergeLines = false;
}

void prettyPrintSyntax(ir::Node &_node, std::wostream & _os, const ContextPtr& _pContext) {
    PrettyPrinterSyntax(_node, _os, _pContext).run();
}

void prettyPrintSyntax(ir::Node &_node, size_t nDepth, std::wostream & _os) {
    PrettyPrinterSyntax(_node, _os, nDepth).run();
}

void prettyPrintCompact(Node &_node, std::wostream &_os, int _nFlags, const ContextPtr& _pContext) {
    std::wstringstream wstringstream;
    PrettyPrinterSyntax(wstringstream, true, _nFlags, _pContext).print(_node);
    _os << removeRedundantSymbols(wstringstream.str(), L"\r\n ");
}

}

void prettyPrintCompact(Node &_node, std::wostream &_os, int _nFlags) {
    std::wstringstream wstringstream;
    pp::PrettyPrinterSyntax(wstringstream, true, _nFlags).print(_node);
    _os << removeRedundantSymbols(wstringstream.str(), L"\r\n ");
}
