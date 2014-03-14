#include "pp_syntax.h"
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"
#include "pp_syntax.h"

#include <sstream>

using namespace ir;

namespace pp {

typedef std::multimap<NodePtr, NodePtr> Graph;
typedef std::pair<const NodePtr, NodePtr> Edge;

class CollectPaths : public Visitor {
public:
    CollectPaths(std::map<NodePtr, std::list<ModulePtr>>& _paths) :
        m_paths(_paths)
    {}

    void makePath(const NodePtr& _pNode) {
        std::list<ModulePtr> path;
        for(auto& i: m_path) {
            if (i.pNode && i.pNode->getNodeKind() == Node::MODULE)
                path.push_back(NodePtr(i.pNode).as<Module>());
        }
        m_paths.insert({_pNode, path});
    }

#define DECLARATION(_TYPE, _METHOD)              \
    virtual bool visit##_TYPE(_TYPE & _node) {   \
        makePath(_METHOD);                       \
        return true;                             \
    }

    DECLARATION(TypeDeclaration, &_node)
    DECLARATION(FormulaDeclaration, &_node)
    DECLARATION(VariableDeclaration, _node.getVariable())

private:
    std::map<NodePtr, std::list<ModulePtr>>& m_paths;
};

class GetDeclarations : public Visitor {
public:
    GetDeclarations(Graph& _decls) : m_decls(_decls) {}

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

    virtual bool traverseTypeType(TypeType& _node) {
        return true;
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

class GetDeclDependencies : public Visitor {
public:
    GetDeclDependencies(const Graph& _decls, Graph& _graph) :
        m_graph(_graph), m_decls(_decls)
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

void Context::collectPaths(Node &_node) {
    CollectPaths(m_paths).traverseNode(_node);
}

void Context::getPath(const ir::NodePtr& _pNode, std::list<ir::ModulePtr>& _container) {
    auto i = m_paths.find(_pNode);
    if (i == m_paths.end())
        return;
    _container = i->second;
}

void Context::_buildDependencies(NodePtr _pRoot) {
    GetDeclarations(m_decls).run(_pRoot);
    GetDeclDependencies(m_decls, m_deps).run(_pRoot);
}

void Context::_topologicalSort(const NodePtr& _pDecl, Nodes& _sorted) {
    if (std::find(_sorted.begin(), _sorted.end(), _pDecl) != _sorted.end())
        return;
    std::pair<Graph::iterator, Graph::iterator> its = m_deps.equal_range(_pDecl);
    for (Graph::iterator i = its.first; i != its.second; ++i)
        _topologicalSort(i->second, _sorted);
    _sorted.add(_pDecl);
}

void Context::sortModule(Module &_module, Nodes& _sorted) {
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
    m_names.clear();
}

bool PrettyPrinterSyntax::traverseModule(Module &_module) {
    VISITOR_ENTER(Module, _module);

    const ModulePtr pOldCurrModule = m_pCurrentModule;
    m_pCurrentModule = &_module;

    if (!_module.getName().empty()) {
        m_os << L"module " << indent << _module.getName();

        if (!_module.getParams().empty()) {
            m_os << L"(" << setInline(true);
            VISITOR_TRAVERSE_COL(Param, ModuleParam, _module.getParams());
            m_os << setInline(false) << L")";
        }

        m_os << L" {";
    }

    Nodes sortedDecls;

    m_pContext->sortModule(_module, sortedDecls);

    sortedDecls.append(_module.getLemmas());
    sortedDecls.append(_module.getMessages());
    sortedDecls.append(_module.getProcesses());
    sortedDecls.append(_module.getClasses());

    for (size_t c = 0; c < sortedDecls.size(); ++c) {
        if (c > 0) {
            if (sortedDecls.get(c - 1)->getNodeKind() == Node::STATEMENT &&
                    !sortedDecls.get(c - 1).as<Statement>()->isBlockLike())
                m_os << L";";
        }

        m_os << L"\n";
        VISITOR_TRAVERSE_ITEM_NS(Node, Decl, sortedDecls, c);
    }

    if (!_module.getName().empty())
        m_os << unindent << L"\n}";

    m_pCurrentModule = pOldCurrModule;
    VISITOR_EXIT();
}

void PrettyPrinterSyntax::printPath(const NodePtr& _pNode) {
    std::list<ModulePtr> container;
    m_pContext->getPath(_pNode, container);

    bool bCanPrint = false;
    for (auto& i: container) {
        if (bCanPrint)
            m_os << i->getName() << L".";
        if (i == m_pCurrentModule)
            bCanPrint = true;
    }
}

// NODE / LABEL
bool PrettyPrinterSyntax::visitLabel(Label &_label) {
    m_pContext->nameGenerator().addLabel(&_label);

    if (m_os.getLevel() > 0)
        m_os << unindent << m_pContext->nameGenerator().getLabelName(_label) << ":\n" << indent;
    else
        m_os << m_pContext->nameGenerator().getLabelName(_label) << ": ";

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

bool PrettyPrinterSyntax::visitType(Type &_type) {
    if ((m_nFlags & PPC_NO_INCOMPLETE_TYPES) &&
            (_type.getKind() == Type::FRESH || _type.getKind() == Type::GENERIC))
        switch (getRole()) {
            case R_VariableType:
            case R_NamedValueType:
            case R_ParamType:
            case R_FormulaDeclResultType:
                m_os << L"var";
                // no break;
            default:
                return true;
        }

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

bool PrettyPrinterSyntax::traverseNamedReferenceType(NamedReferenceType &_type) {
    VISITOR_ENTER(NamedReferenceType, _type);
    printPath(_type.getDeclaration());
    m_os << _type.getName();

    if (!_type.getArgs().empty()) {
        m_os << L"(" << setInline(true);
        VISITOR_TRAVERSE_COL(Expression, NamedTypeArg, _type.getArgs());
        m_os << setInline(false) << L")";
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::visitSetType(SetType &_type) {
    m_os << (m_bCompact ? L"{" : L"set(") << setInline(true);
    VISITOR_TRAVERSE_NS(Type, SetBaseType, _type.getBaseType());
    m_os << setInline(false) << (m_bCompact ? L"}" : L")");
    return false;
}

bool PrettyPrinterSyntax::visitListType(ListType &_type) {
    m_os << (m_bCompact ? L"[[" : L"list(") << setInline(true);
    VISITOR_TRAVERSE_NS(Type, ListBaseType, _type.getBaseType());
    m_os << setInline(false) << (m_bCompact ? L"]]" : L")");
    return false;
}

bool PrettyPrinterSyntax::visitRefType(RefType &_type) {
    m_os << L"ref(" << setInline(true);
    VISITOR_TRAVERSE_NS(Type, RefBaseType, _type.getBaseType());
    m_os << setInline(false) << L")";
    return false;
}

bool PrettyPrinterSyntax::traverseMapType(MapType &_type) {
    VISITOR_ENTER(MapType, _type);
    m_os << (m_bCompact ? L"{" : L"map(") << setInline(true);
    VISITOR_TRAVERSE_NS(Type, MapIndexType, _type.getIndexType());
    m_os << (m_bCompact ? L" : " : L", ");
    VISITOR_TRAVERSE_NS(Type, MapBaseType, _type.getBaseType());
    m_os << setInline(false) << (m_bCompact ? L"}" : L")");
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseSubtype(Subtype &_type) {
    if (!m_bCompact) {
        if (RangePtr pRange = _type.asRange()) {
            VISITOR_TRAVERSE_NS(Range, Type, pRange);
            return true;
        }
    }

    VISITOR_ENTER(Subtype, _type);
    m_os << (m_bCompact ? L"{" : L"subtype(") << indent;
    VISITOR_TRAVERSE_NS(NamedValue, SubtypeParam, _type.getParam());
    m_os << (m_bCompact ? L" | " : L": ");
    VISITOR_TRAVERSE_NS(Expression, SubtypeCond, _type.getExpression());
    m_os << unindent << (m_bCompact ? L"}" : L")");
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseRange(Range &_type) {
    VISITOR_ENTER(Range, _type);
    m_os << setInline(true);
    VISITOR_TRAVERSE_NS(Expression, RangeMin, _type.getMin());
    m_os << L"..";
    VISITOR_TRAVERSE_NS(Expression, RangeMax, _type.getMax());
    m_os << setInline(false);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::visitArrayType(ArrayType &_type) {
    const TypePtr& pBaseType = _type.getRootType();

    m_os << setInline(true);

    if (!m_bCompact)
        m_os << L"array(";

    VISITOR_TRAVERSE_NS(Type, ArrayBaseType, pBaseType);

    if (!m_bCompact)
        m_os << L", ";

    Collection<Type> dims;
    _type.getDimensions(dims);

    for (size_t c = 0; c < dims.size(); ++c) {
        if (m_bCompact)
            m_os << L"[";
        else if (c > 0)
            m_os << L", ";

        TypePtr pDim = dims.get(c);
        if (pDim->getKind() == Type::SUBTYPE) {
            RangePtr pRange = pDim.as<Subtype>()->asRange();
            if (pRange)
                pDim = pRange;
        }

        VISITOR_TRAVERSE_NS(Type, ArrayDimType, pDim);

        if (m_bCompact)
            m_os << L"]";
    }

    if (!m_bCompact)
        m_os << L")";

    m_os << setInline(false);

    return false;
}

bool PrettyPrinterSyntax::traverseEnumType(EnumType &_type) {
    VISITOR_ENTER(EnumType, _type);
    m_os << L"enum(" << indent;

    for (size_t i = 0; i < _type.getValues().size(); ++i) {
        if (i > 0)
            m_os << L",\n";
        else if (!m_os.isInline())
            m_os << L"\n";

        VISITOR_TRAVERSE_ITEM_NS(EnumValue, EnumValueDecl, _type.getValues(), i);
    }

    m_os << unindent << (_type.getValues().empty() || m_os.isInline() ? L")" : L"\n)");
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::_traverseStructType(StructType &_type) {
    VISITOR_ENTER(StructType, _type);

    // Order of fields: ordered names, unordered names, ordered types.
    if (!_type.getNamesOrd().empty()) {
        if (!m_os.isInline())
            m_os << L"\n";
        VISITOR_TRAVERSE_COL(NamedValue, StructFieldDeclNameOrd, _type.getNamesOrd());
    }

    if (m_bCompact && !(_type.getNamesSet().empty() && _type.getTypesOrd().empty()))
        m_os << L";";

    if (!_type.getNamesSet().empty()) {
        // Ensure sorting for debug purposes (don't reorder source collection though).
        std::map<std::wstring, NamedValuePtr> sortedFieldsMap;
        NamedValues sortedFields;

        for (size_t i = 0; i < _type.getNamesSet().size(); ++i)
            sortedFieldsMap[_type.getNamesSet().get(i)->getName()] = _type.getNamesSet().get(i);

        for (std::map<std::wstring, NamedValuePtr>::iterator i = sortedFieldsMap.begin();
                i != sortedFieldsMap.end(); ++i)
            sortedFields.add(i->second);

        if (m_bCompact)
            m_os << L"\n";
        else if (!_type.getNamesSet().empty())
            m_os << L",\n";

        VISITOR_TRAVERSE_COL(NamedValue, StructFieldDeclNameSet, sortedFields);
    }

    if (m_bCompact && !_type.getTypesOrd().empty())
        m_os << L";";

    if (!_type.getTypesOrd().empty()) {
        if (m_bCompact)
            m_os << L"\n";
        else if (!_type.getNamesOrd().empty() || !_type.getNamesSet().empty())
            m_os << L",\n";

        VISITOR_TRAVERSE_COL(NamedValue, StructFieldDeclTypeOrd, _type.getTypesOrd());
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseStructType(StructType &_type) {
    m_os << (m_bCompact ? L"(" : L"struct(") << indent;
    const bool bResult = _traverseStructType(_type);
    m_os << unindent << (_type.empty() || m_os.isInline() ? L")" : L"\n)");
    return bResult;
}

bool PrettyPrinterSyntax::traverseUnionConstructorDeclaration(UnionConstructorDeclaration &_cons) {
    VISITOR_ENTER(UnionConstructorDeclaration, _cons);

    if (getLoc().cPosInCollection > 0)
        m_os << L",\n";
    else if (!m_os.isInline())
        m_os << L"\n";

    m_os << _cons.getName() << L"(" << setInline(true);

    if (const StructTypePtr pStruct = _cons.getStructFields()) {
        VISITOR_TRAVERSE_INLINE_NS(StructType, UnionConsFields, pStruct);
        if (!_traverseStructType(*pStruct))
            return false;
    } else
        VISITOR_TRAVERSE_NS(Type, UnionConsFields, _cons.getFields());

    m_os << setInline(false) << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseUnionType(UnionType &_type) {
    VISITOR_ENTER(UnionType, _type);
    m_os << L"union(" << indent;
    VISITOR_TRAVERSE_COL(UnionConstructorDeclaration, UnionConstructorDecl, _type.getConstructors());
    m_os << unindent << (_type.getConstructors().empty() || m_os.isInline() ? L")" : L"\n)");
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traversePredicateType(PredicateType &_type) {
    VISITOR_ENTER(PredicateType, _type);
    m_os << L"predicate(" << setInline(true);
    VISITOR_TRAVERSE_COL(Param, PredicateTypeInParam, _type.getInParams());

    for (size_t i = 0; i < _type.getOutParams().size(); ++i) {
        Branch &br = *_type.getOutParams().get(i);

        m_os << L" : ";
        VISITOR_TRAVERSE_COL(Param, PredicateTypeOutParam, br);

        if (br.getLabel() && !br.getLabel()->getName().empty())
            m_os << L" #" << br.getLabel()->getName();
    }

    m_os << setInline(false) << L")";

    if (m_bCompact)
        VISITOR_EXIT();

    if (_type.getPreCondition()) {
        m_os << indent << L"\npre " << indent;
        VISITOR_TRAVERSE_NS(Formula, PredicatePreCondition, _type.getPreCondition());
        m_os << unindent << unindent;
    }

    Branches& branches = _type.getOutParams();

    for (size_t i = 0; i < branches.size(); ++i) {
        if (!branches.get(i) || !branches.get(i)->getPreCondition() || !branches.get(i)->getLabel())
            continue;

        Branch& br = *branches.get(i);
        m_os << indent << L"\npre " << br.getLabel()->getName() << L": " << indent;
        VISITOR_TRAVERSE_NS(Formula, PredicatePreCondition, br.getPreCondition());
        m_os << unindent << unindent;
    }

    for (size_t i = 0; i < branches.size(); ++i) {
        if (!branches.get(i) || !branches.get(i)->getPostCondition() || !branches.get(i)->getLabel())
            continue;

        Branch& br = *branches.get(i);
        m_os << indent << L"\npost " << br.getLabel()->getName() << L": " << indent;
        VISITOR_TRAVERSE_NS(Formula, PredicatePostCondition, br.getPostCondition());
        m_os << unindent << unindent;
    }

    if (_type.getPostCondition()) {
        m_os << indent << L"\npost " << indent;
        VISITOR_TRAVERSE_NS(Formula, PredicatePostCondition, _type.getPostCondition());
        m_os << unindent << unindent;
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseParameterizedType(ParameterizedType &_type) {
    VISITOR_ENTER(ParameterizedType, _type);
    VISITOR_TRAVERSE_NS(Type, ParameterizedTypeBase, _type.getActualType());
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

bool PrettyPrinterSyntax::traverseJump(Jump &_stmt) {
    VISITOR_ENTER(Jump, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    m_os << L"#" << m_pContext->nameGenerator().getLabelName(*_stmt.getDestination());
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseBlock(Block &_stmt) {
    VISITOR_ENTER(Block, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());

    m_os << L"{" << indent;

    for (size_t i = 0; i < _stmt.size(); ++i) {
        if (i > 0 && !_stmt.get(i - 1)->isBlockLike())
            m_os << L";";

        m_os << L"\n";
        VISITOR_TRAVERSE_ITEM_NS(Statement, Stmt, _stmt, i);
    }

    m_os << unindent << L"\n}";

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseParallelBlock(ParallelBlock &_stmt) {
    VISITOR_ENTER(ParallelBlock, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());

    if (_stmt.empty())
        VISITOR_EXIT();

    VISITOR_TRAVERSE_ITEM_NS(Statement, Stmt, _stmt, 0);
    m_os << indent;

    for (size_t i = 1; i < _stmt.size(); ++i) {
        m_os << L" ||\n";
        VISITOR_TRAVERSE_ITEM_NS(Statement, Stmt, _stmt, i);
    }

    m_os << unindent;
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::_traverseAnonymousPredicate(AnonymousPredicate &_decl) {
    m_os << L"(" << setInline();
    VISITOR_TRAVERSE_COL(Param, PredicateInParam, _decl.getInParams());

    bool bHavePreConditions = false, bHavePostConditions = false;

    for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
        Branch &br = *_decl.getOutParams().get(i);

        m_os << L" : ";

        bHavePreConditions |= br.getPreCondition();
        bHavePostConditions |= br.getPostCondition();
        VISITOR_TRAVERSE_COL(Param, PredicateOutParam, br);

        if (br.getLabel() && !br.getLabel()->getName().empty())
            m_os << (br.empty() ? L"#" : L" #") << br.getLabel()->getName();
    }

    bHavePreConditions |= _decl.getPreCondition();
    bHavePostConditions |= _decl.getPostCondition();

    m_os << setInline(false) << L")";

    if (!_decl.getBlock())
        m_os << indent;

    if (bHavePreConditions) {
        if (_decl.getPreCondition()) {
            m_os << L"\npre " << indent;
            VISITOR_TRAVERSE_NS(Formula, PredicatePreCondition, _decl.getPreCondition());
            m_os << unindent;
        }

        for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
            Branch &br = *_decl.getOutParams().get(i);

            if (br.getPreCondition()) {
                m_os << L"\npre " << br.getLabel()->getName() << L": " << indent;
                VISITOR_TRAVERSE_NS(Formula, PredicateBranchPreCondition, br.getPreCondition());
                m_os << unindent;
            }
        }
    }

    if (_decl.getBlock())
        m_os << (bHavePreConditions ? L"\n" : L" ");

    VISITOR_TRAVERSE_NS(Block, PredicateBody, _decl.getBlock());

    if (bHavePostConditions) {
        if (_decl.getPostCondition()) {
            m_os << L"\npost " << indent;
            VISITOR_TRAVERSE_NS(Formula, PredicatePostCondition, _decl.getPostCondition());
            m_os << unindent;
        }

        for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
            Branch &br = *_decl.getOutParams().get(i);

            if (br.getPostCondition()) {
                m_os << L"\npost " << br.getLabel()->getName() << L": " << indent;
                VISITOR_TRAVERSE_NS(Formula, PredicateBranchPostCondition, br.getPostCondition());
                m_os << unindent;
            }
        }
    }

    if (_decl.getMeasure()) {
        m_os << L"\nmeasure " << indent;
        VISITOR_TRAVERSE_NS(Expression, PredicateMeasure, _decl.getMeasure());
        m_os << unindent;
    }

    if (_decl.getMeasure() || bHavePostConditions)
        m_os << L";";

    if (!_decl.getBlock())
        m_os << unindent;

    return true;
}

bool PrettyPrinterSyntax::traversePredicate(Predicate &_stmt) {
    VISITOR_ENTER(Predicate, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());

    m_os << _stmt.getName();

    if (!_traverseAnonymousPredicate(_stmt))
        return false;

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseAssignment(Assignment &_stmt) {
    VISITOR_ENTER(Assignment, _stmt);

    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    VISITOR_TRAVERSE_NS(Expression, LValue, _stmt.getLValue());
    m_os << " = " << indent;
    VISITOR_TRAVERSE_NS(Expression, RValue, _stmt.getExpression());
    m_os << unindent;

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseMultiassignment(Multiassignment &_stmt) {
    VISITOR_ENTER(Multiassignment, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());

    VISITOR_TRAVERSE_COL(Expression, LValue, _stmt.getLValues());
    m_os << " = " << indent;
    VISITOR_TRAVERSE_COL(Expression, RValue, _stmt.getExpressions());
    m_os << unindent;

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
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    VISITOR_TRAVERSE_NS(Expression, PredicateCallee, _stmt.getPredicate());

    m_os << L"(" << indent;
    VISITOR_TRAVERSE_COL(Expression, PredicateCallArgs, _stmt.getArgs());

    std::map<CallBranchPtr, std::wstring> branchLabels;
    const std::wstring strGeneralName =
         _stmt.getPredicate()->getKind() == Expression::PREDICATE ?
             _stmt.getPredicate().as<PredicateReference>()->getName() : L"";
    bool bEmptyArgSet = _stmt.getArgs().empty();

    for (CallBranchPtr pBranch : _stmt.getBranches()) {
        m_os << (bEmptyArgSet ? L":" : L" :");
        bEmptyArgSet = true;

        if (!pBranch)
            continue;

        bEmptyArgSet = pBranch->empty() && !pBranch->getHandler();

        for (size_t c = 0; c < pBranch->size(); ++c) {
            ExpressionPtr pExpr = pBranch->get(c);

            m_os << (c > 0 ? L", " : L" ");

            if (VariableDeclarationPtr pVar = getVarDecl(_stmt, pExpr))
                VISITOR_TRAVERSE_NS(VariableDeclaration, PredicateVarDecl, pVar);
            else
                VISITOR_TRAVERSE_NS(Expression, PredicateCallBranchResults, pExpr);
        }

        if (pBranch->getHandler()) {
            const std::wstring strLabel = m_pContext->nameGenerator().getNewLabelName(strGeneralName);
            branchLabels[pBranch] = strLabel;
            m_os << L" #" << strLabel;
        }

    }

    m_os << unindent << L")";

    for (size_t c = 0; c < _stmt.getBranches().size(); ++c) {
        CallBranchPtr pBranch = _stmt.getBranches().get(c);

        if (!pBranch || !pBranch->getHandler())
            continue;

        m_os << indent << L"\n" << L"case " << branchLabels[pBranch] << ":\n" << indent;
        VISITOR_TRAVERSE_NS(Statement, PredicateCallBranchHandler, pBranch->getHandler());
        m_os << unindent << unindent;
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseIf(If &_stmt) {
    VISITOR_ENTER(If, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    m_os << L"if (" << indent;
    VISITOR_TRAVERSE_NS(Expression, IfArg, _stmt.getArg());
    m_os << unindent << L")";

    bool bBlock = _stmt.getBody() &&
            _stmt.getBody()->getKind() == Statement::BLOCK;

    if (bBlock)
        m_os << " ";
    else
        m_os << "\n" << indent;

    VISITOR_TRAVERSE_NS(Statement, IfBody, _stmt.getBody());

    if (!bBlock)
        m_os << unindent;

    if (_stmt.getElse()) {
        m_os << (bBlock ? " " : "\n") << "else";
        bBlock = _stmt.getElse()->getKind() == Statement::BLOCK ||
                _stmt.getElse()->getKind() == Statement::IF;

        if (bBlock)
            m_os << " ";
        else
            m_os << "\n" << indent;

        VISITOR_TRAVERSE_NS(Statement, IfElse, _stmt.getElse());

        if (!bBlock)
            m_os << unindent;
    }

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseSwitchCase(SwitchCase &_case) {
    VISITOR_ENTER(SwitchCase, _case);
    m_os << L"\ncase " << indent;
    VISITOR_TRAVERSE_COL(Expression, SwitchCaseValue, _case.getExpressions());
    m_os << L":\n";
    VISITOR_TRAVERSE_NS(Statement, SwitchCaseBody, _case.getBody());
    m_os << unindent;
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseSwitch(Switch &_stmt) {
    VISITOR_ENTER(Switch, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    m_os << L"switch (" << indent;

    if (_stmt.getParamDecl())
        VISITOR_TRAVERSE_NS(VariableDeclaration, SwitchParamDecl, _stmt.getParamDecl());
    else
        VISITOR_TRAVERSE_NS(Expression, SwitchArg, _stmt.getArg());

    m_os << L") {";
    VISITOR_TRAVERSE_COL(SwitchCase, SwitchCase, _stmt);

    if (_stmt.getDefault()) {
        if (!_stmt.empty())
            m_os << L"\n";

        m_os << L"default:\n" << indent;
        VISITOR_TRAVERSE_NS(Statement, SwitchDefault, _stmt.getDefault());
        m_os << unindent;
    }

    m_os << unindent << L"\n}";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFor(For &_stmt) {
    VISITOR_ENTER(For, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    m_os << L"for (" << indent;
    VISITOR_TRAVERSE_NS(VariableDeclaration, ForIterator, _stmt.getIterator());
    m_os << L"; ";
    VISITOR_TRAVERSE_NS(Expression, ForInvariant, _stmt.getInvariant());
    m_os << L"; ";
    VISITOR_TRAVERSE_NS(Statement, ForIncrement, _stmt.getIncrement());
    m_os << L")\n";
    VISITOR_TRAVERSE_NS(Statement, ForBody, _stmt.getBody());
    m_os << unindent;
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseWhile(While &_stmt) {
    VISITOR_ENTER(While, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    m_os << L"while (" << indent;
    VISITOR_TRAVERSE_NS(Expression, WhileInvariant, _stmt.getInvariant());
    m_os << L")\n";
    VISITOR_TRAVERSE_NS(Statement, WhileBody, _stmt.getBody());
    m_os << unindent;
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseBreak(Break &_stmt) {
    VISITOR_ENTER(Break, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    m_os << L"break";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseWith(With &_stmt) {
    VISITOR_ENTER(With, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());

    m_os << L"with (" << indent;
    VISITOR_TRAVERSE_COL(Expression, WithArg, _stmt.getArgs());
    m_os << L")\n";
    VISITOR_TRAVERSE_NS(Statement, WithBody, _stmt.getBody());
    m_os << unindent;
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseTypeDeclaration(TypeDeclaration &_stmt) {
    VISITOR_ENTER(TypeDeclaration, _stmt);

    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    m_os << L"type " << _stmt.getName();

    if (!_stmt.getType())
        VISITOR_EXIT();

    if (_stmt.getType()->getKind() == Type::PARAMETERIZED) {
        m_os << L"(" << setInline(true);
        VISITOR_TRAVERSE_COL(NamedValue, ParameterizedTypeParam,
                _stmt.getType().as<ParameterizedType>()->getParams());
        m_os<< setInline(false) << L")";
    }

    m_os << L" = " << indent;
    VISITOR_TRAVERSE_NS(Type, TypeDeclBody, _stmt.getType());
    m_os << unindent;

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseVariableDeclaration(VariableDeclaration &_stmt) {
    VISITOR_ENTER(VariableDeclaration, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());

    if (getRole() == R_VarDeclGroupElement && getLoc().cPosInCollection > 0) {
        VISITOR_TRAVERSE_INLINE(Variable, VarDeclVar, _stmt.getVariable(),
                _stmt, VariableDeclaration, setVariable);
        VISITOR_ENTER(Variable, *_stmt.getVariable());
        m_os << ", " << m_pContext->nameGenerator().getNamedValueName(
                *_stmt.getVariable());
        VISITOR_EXIT_INLINE();
    } else
        VISITOR_TRAVERSE_NS(Variable, VarDeclVar, _stmt.getVariable());

    if (getRole() == R_VarDeclGroupElement && getLoc().cPosInCollection == 0)
        m_os << indent;

    if (_stmt.getValue()) {
        m_os << L" = " << indent;
        VISITOR_TRAVERSE_NS(Expression, VarDeclInit, _stmt.getValue());
        m_os << unindent;
    }

    if (getRole() == R_VarDeclGroupElement && getLoc().bLastInCollection)
        m_os << unindent;

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseVariableDeclarationGroup(VariableDeclarationGroup &_stmt) {
    VISITOR_ENTER(VariableDeclarationGroup, _stmt);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());
    VISITOR_TRAVERSE_COL(VariableDeclaration, VarDeclGroupElement, _stmt);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFormulaDeclaration(FormulaDeclaration &_node) {
    VISITOR_ENTER(FormulaDeclaration, _node);
    VISITOR_TRAVERSE_NS(Label, StmtLabel, _node.getLabel());

    m_os << L"formula " << _node.getName();

    if (!_node.getParams().empty() || _node.getResultType()) {
        m_os << L"(" << setInline(true);
        VISITOR_TRAVERSE_COL(NamedValue, FormulaDeclParams, _node.getParams());

        if (_node.getResultType())
            m_os << L" : ";

        VISITOR_TRAVERSE_NS(Type, FormulaDeclResultType, _node.getResultType());
        m_os << setInline(false) << L")";
    }

    m_os << L" = " << indent;
    VISITOR_TRAVERSE_NS(Expression, FormulaDeclBody, _node.getFormula());
    m_os << unindent;

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseLemmaDeclaration(LemmaDeclaration &_stmt) {
    VISITOR_ENTER(LemmaDeclaration, _stmt);

    VISITOR_TRAVERSE_NS(Label, StmtLabel, _stmt.getLabel());

    m_os << L"lemma ";

    switch (_stmt.getStatus()) {
        case LemmaDeclaration::VALID:
            m_os << L"valid ";
            break;
        case LemmaDeclaration::INVALID:
            m_os << L"invalid ";
            break;
        case LemmaDeclaration::UNKNOWN:
            m_os << L"unknown ";
            break;
    }

    m_os << indent;
    VISITOR_TRAVERSE_NS(Expression, LemmaDeclBody, _stmt.getProposition());
    m_os << unindent;

    VISITOR_EXIT();
}

// NODE / NAMED_VALUE
bool PrettyPrinterSyntax::traverseNamedValue(NamedValue &_val) {
    if (!m_path.empty() && getLoc().bPartOfCollection && getLoc().cPosInCollection != 0) {
        if (getRole() == R_StructFieldDeclNameOrd ||
                getRole() == R_StructFieldDeclNameSet ||
                getRole() == R_StructFieldDeclTypeOrd)
            m_os << L",\n";
        else
            m_os << L", ";
    }

    if (_val.getKind() != NamedValue::GENERIC)
        return PrettyPrinterBase::traverseNamedValue(_val);


    VISITOR_ENTER(NamedValue, _val);
    VISITOR_TRAVERSE_NS(Type, NamedValueType, _val.getType());

    if (getRole() != R_StructFieldDeclTypeOrd)
        m_os << L" " << m_pContext->nameGenerator().getNamedValueName(_val);

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseEnumValue(EnumValue &_val) {
    VISITOR_ENTER(EnumValue, _val);
    m_os << m_pContext->nameGenerator().getNamedValueName(_val);
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseParam(Param &_val) {
    VISITOR_ENTER(Param, _val);
    VISITOR_TRAVERSE_NS(Type, ParamType, _val.getType());

    if (_val.isUsed())
        m_os << L" " << m_pContext->nameGenerator().getNamedValueName(_val);

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseVariable(Variable &_val) {
    VISITOR_ENTER(Variable, _val);

    if (_val.isMutable())
        m_os << "mutable ";

    m_os << setInline(true);
    VISITOR_TRAVERSE_NS(Type, VariableType, _val.getType());
    m_os << L" " << m_pContext->nameGenerator().getNamedValueName(_val);
    m_os << setInline(false);
    VISITOR_EXIT();
}

// NODE / EXPRESSION
void PrettyPrinterSyntax::printLiteralKind(Literal &_node) {
    switch (_node.getLiteralKind()) {
        case Literal::UNIT:
            m_os << L"0";
            break;
        case Literal::NUMBER:
            m_os << _node.getNumber().toString();
            break;
        case Literal::BOOL:
            m_os << (_node.getBool() ? L"true" : L"false");
            break;
        case Literal::CHAR:
            m_os <<  L"\'" << setVerbatim(true) << _node.getChar() << setVerbatim(false) << L"\'";
            break;
        case Literal::STRING:
            m_os <<  L"\"" << setVerbatim(true) << _node.getString().c_str() << setVerbatim(false) << L"\"";
            break;
    }
}

void PrettyPrinterSyntax::printUnaryOperator(Unary &_node) {
    switch (_node.getOperator()) {
        case Unary::PLUS:
            m_os << L"+";
            break;
        case Unary::MINUS:
            m_os << L"-";
            break;
        case Unary::BOOL_NEGATE:
            m_os << L"!";
            break;
        case Unary::BITWISE_NEGATE:
            m_os << L"~";
            break;
    }
}

void PrettyPrinterSyntax::printBinaryOperator(Binary &_node) {
    switch (_node.getOperator()) {
        case Binary::ADD:
            m_os << L" + ";
            break;
        case Binary::SUBTRACT:
            m_os << L" - ";
            break;
        case Binary::MULTIPLY:
            m_os << L"*";
            break;
        case Binary::DIVIDE:
            m_os << L"/";
            break;
        case Binary::REMAINDER:
            m_os << L"%";
            break;
        case Binary::SHIFT_LEFT:
            m_os << L" << ";
            break;
        case Binary::SHIFT_RIGHT:
            m_os << L" >> ";
            break;
        case Binary::POWER:
            m_os << L"^";
            break;
        case Binary::BOOL_AND:
            m_os << L" & ";
            break;
        case Binary::BOOL_OR:
            m_os << L" or ";
            break;
        case Binary::BOOL_XOR:
            m_os << L" ^ ";
            break;
        case Binary::EQUALS:
            m_os << L" = ";
            break;
        case Binary::NOT_EQUALS:
            m_os << L" != ";
            break;
        case Binary::LESS:
            m_os << L" < ";
            break;
        case Binary::LESS_OR_EQUALS:
            m_os << L" <= ";
            break;
        case Binary::GREATER:
            m_os << L" > ";
            break;
        case Binary::GREATER_OR_EQUALS:
            m_os << L" >= ";
            break;
        case Binary::IMPLIES:
            m_os << L" => ";
            break;
        case Binary::IFF:
            m_os << L" <=> ";
            break;
    }
}

void PrettyPrinterSyntax::printQuantifier(int _quantifier) {
    switch (_quantifier) {
        case Formula::NONE:
            m_os << "none";
            break;
        case Formula::UNIVERSAL:
            m_os << "forall";
            break;
        case Formula::EXISTENTIAL:
            m_os << "exists";
            break;
    }
}

bool PrettyPrinterSyntax::needsParen() {
    if (m_path.empty() || !getLoc().pNode ||
            getLoc().pNode->getNodeKind() != Node::EXPRESSION)
        return false;

    const ExpressionPtr pNode = static_cast<Expression *>(getLoc().pNode);
    const int nKind = pNode->getKind();

    switch (nKind) {
        case Expression::FORMULA:
            if (pNode.as<Formula>()->getQuantifier() == Formula::NONE)
                return false;
            break;

        case Expression::LAMBDA:
            if (!pNode.as<Lambda>()->getPredicate().getPostCondition() &&
                    !pNode.as<Lambda>()->getPredicate().getMeasure())
                return false;
            break;

        case Expression::COMPONENT:
            if (pNode.as<Component>()->getComponentKind() != Component::REPLACEMENT)
                return false;
            break;

        case Expression::CAST:
        case Expression::UNARY:
        case Expression::BINARY:
        case Expression::TERNARY:
            break;

        default:
            return false;
    }

    switch (getRole()) {
        case R_RangeMin:
        case R_RangeMax:
        case R_UnarySubexpression:
        case R_ArrayPartObject:
        case R_FieldObject:
        case R_MapElementObject:
        case R_ListElementObject:
        case R_ReplacementObject:
        case R_ReplacementValue:
        case R_FunctionCallee:
        case R_BinderCallee:
        case R_LValue:
        case R_PredicateCallee:
            return true;

        case R_BinarySubexpression:
            if (nKind == Expression::UNARY)
                return false;
            if (nKind != Expression::BINARY)
                return true;
            return (Binary::getPrecedence(pNode.as<Binary>()->getOperator()) <
                    Binary::getPrecedence(static_cast<Binary *>(getParent())->getOperator()));

        case R_TernarySubexpression:
            return nKind == Expression::TERNARY;

        default:
            return false;
    }
}

bool PrettyPrinterSyntax::traverseExpression(Expression &_node) {
    if (getLoc().bPartOfCollection && getLoc().cPosInCollection > 0)
        m_os << ", ";

    const bool bParen = needsParen();

    if (bParen)
        m_os << "(" << indent;

    const bool bResult = Visitor::traverseExpression(_node);

    if (bParen)
        m_os << unindent << ")";

    return bResult;

}

bool PrettyPrinterSyntax::visitLiteral(Literal &_node) {
    printLiteralKind(_node);
    return true;
}

bool PrettyPrinterSyntax::visitVariableReference(VariableReference &_node) {
    printPath(_node.getTarget());
    m_os << (_node.getName().empty() ?
        m_pContext->nameGenerator().getNamedValueName(*_node.getTarget()) : _node.getName());
    return false;
}

bool PrettyPrinterSyntax::visitPredicateReference(PredicateReference &_node) {
    m_os << _node.getName();
    return true;
}

bool PrettyPrinterSyntax::visitLambda(Lambda &_node) {
    m_os << L"predicate";
    return true;
}

bool PrettyPrinterSyntax::traverseBinder(Binder &_expr) {
    VISITOR_ENTER(Binder, _expr);
    VISITOR_TRAVERSE_NS(Expression, BinderCallee, _expr.getPredicate());

    m_os << L"(" << indent;

    for (size_t i = 0; i < _expr.getArgs().size(); ++i)
        if (_expr.getArgs().get(i))
            VISITOR_TRAVERSE_ITEM_NS(Expression, BinderArgs, _expr.getArgs(), i);
        else
            m_os << (i > 0 ? L", _" : L"_");

    m_os << unindent << L")";

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::visitUnary(Unary &_node) {
    printUnaryOperator(_node);
    return true;
}

bool PrettyPrinterSyntax::traverseBinary(Binary &_node) {
    VISITOR_ENTER(Binary, _node);
    VISITOR_TRAVERSE_NS(Expression, BinarySubexpression, _node.getLeftSide());
    printBinaryOperator(_node);
    m_os << indent;
    VISITOR_TRAVERSE_NS(Expression, BinarySubexpression, _node.getRightSide());
    m_os << unindent;
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseTernary(Ternary &_node) {
    VISITOR_ENTER(Ternary, _node);

    VISITOR_TRAVERSE_NS(Expression, TernarySubexpression, _node.getIf());
    m_os << " ? " << indent;
    VISITOR_TRAVERSE_NS(Expression, TernarySubexpression, _node.getThen());
    m_os << " : ";
    VISITOR_TRAVERSE_NS(Expression, TernarySubexpression, _node.getElse());
    m_os << unindent;

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFormula(Formula &_node) {
    VISITOR_ENTER(Formula, _node);

    const int nQuantifier = _node.getQuantifier();

    if (nQuantifier != Formula::NONE) {
        printQuantifier(nQuantifier);
        m_os << " ";
        VISITOR_TRAVERSE_COL(NamedValue, FormulaBoundVariable, _node.getBoundVariables());
        m_os << ". " << indent;
    }

    VISITOR_TRAVERSE_NS(Expression, Subformula, _node.getSubformula());

    if (nQuantifier != Formula::NONE)
        m_os << unindent;

    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseReplacement(Replacement &_expr) {
    VISITOR_ENTER(Replacement, _expr);
    VISITOR_TRAVERSE_NS(Expression, ReplacementObject, _expr.getObject());
    m_os << L" with " << indent;
    VISITOR_TRAVERSE_NS(Constructor, ReplacementValue, _expr.getNewValues());
    m_os << unindent;
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseRecognizerExpr(RecognizerExpr &_expr) {
    VISITOR_ENTER(RecognizerExpr, _expr);
    m_os << _expr.getConstructorName() << L"?(" << indent;
    VISITOR_TRAVERSE_NS(Expression, RecognizerExpression, _expr.getObject());
    m_os << unindent << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseAccessorExpr(AccessorExpr &_expr) {
    VISITOR_ENTER(AccessorExpr, _expr);
    m_os << _expr.getConstructorName() << L"!(" << indent;
    VISITOR_TRAVERSE_NS(Expression, AccessorExpression, _expr.getObject());
    m_os << unindent << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFunctionCall(FunctionCall &_expr) {
    VISITOR_ENTER(FunctionCall, _expr);
    VISITOR_TRAVERSE_NS(Expression, FunctionCallee, _expr.getPredicate());
    m_os << L"(" << indent;
    VISITOR_TRAVERSE_COL(Expression, FunctionCallArgs, _expr.getArgs());
    m_os << unindent << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFormulaCall(FormulaCall &_expr) {
    VISITOR_ENTER(FormulaCall, _expr);
    printPath(_expr.getTarget());
    m_os << _expr.getName() << L"(" << indent;
    VISITOR_TRAVERSE_COL(Expression, FormulaCallArgs, _expr.getArgs());
    m_os << unindent << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseStructFieldDefinition(StructFieldDefinition &_cons) {
    VISITOR_ENTER(StructFieldDefinition, _cons);

    if (getLoc().cPosInCollection > 0)
        m_os << L", ";

    if (!_cons.getName().empty())
        m_os << _cons.getName() << L": ";

    VISITOR_TRAVERSE_NS(Expression, StructFieldValue, _cons.getValue());
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseStructConstructor(StructConstructor &_expr) {
    VISITOR_ENTER(StructConstructor, _expr);
    m_os << L"(" << indent;
    VISITOR_TRAVERSE_COL(StructFieldDefinition, StructFieldDef, _expr);
    m_os << unindent << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseUnionConstructor(UnionConstructor &_expr) {
    VISITOR_ENTER(UnionConstructor, _expr);
    m_os << _expr.getName() << L"(" << indent;
    VISITOR_TRAVERSE_COL(StructFieldDefinition, UnionCostructorParam, _expr);
    m_os << unindent << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseElementDefinition(ElementDefinition &_cons) {
    VISITOR_ENTER(ElementDefinition, _cons);
    if (getLoc().cPosInCollection > 0)
        m_os << L", ";

    VISITOR_TRAVERSE_NS(Expression, ElementIndex, _cons.getIndex());
    if (_cons.getIndex())
        m_os << L": ";

    VISITOR_TRAVERSE_NS(Expression, ElementValue, _cons.getValue());
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseArrayConstructor(ArrayConstructor &_expr) {
    VISITOR_ENTER(ArrayConstructor, _expr);
    m_os << L"[" << indent;
    VISITOR_TRAVERSE_COL(ElementDefinition, ArrayElementDef, _expr);
    m_os << unindent << L"]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseMapConstructor(MapConstructor &_expr) {
    VISITOR_ENTER(MapConstructor, _expr);
    m_os << L"[{" << indent;
    VISITOR_TRAVERSE_COL(ElementDefinition, MapElementDef, _expr);
    m_os << unindent << L"}]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseSetConstructor(SetConstructor &_expr) {
    VISITOR_ENTER(SetConstructor, _expr);
    m_os << L"{" << indent;
    VISITOR_TRAVERSE_COL(Expression, SetElementDef, _expr);
    m_os << unindent << L"}";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseListConstructor(ListConstructor &_expr) {
    VISITOR_ENTER(ListConstructor, _expr);
    m_os << L"[[" << indent;
    VISITOR_TRAVERSE_COL(Expression, ListElementDef, _expr);
    m_os << unindent << L"]]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseArrayPartDefinition(ArrayPartDefinition &_cons) {
    VISITOR_ENTER(ArrayPartDefinition, _cons);
    m_os << L"\ncase ";
    VISITOR_TRAVERSE_COL(Expression, ArrayPartCond, _cons.getConditions());
    m_os << L": " << indent;
    VISITOR_TRAVERSE_NS(Expression, ArrayPartValue, _cons.getExpression());
    m_os << unindent;
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseArrayIteration(ArrayIteration &_expr) {
    VISITOR_ENTER(ArrayIteration, _expr);

    m_os << L"for (" << indent;
    VISITOR_TRAVERSE_COL(NamedValue, ArrayIterator, _expr.getIterators());
    m_os << L") {";
    VISITOR_TRAVERSE_COL(ArrayPartDefinition, ArrayIterationPart, _expr);

    if (_expr.getDefault()) {
        m_os << L"\ndefault: " << indent;
        VISITOR_TRAVERSE_NS(Expression, ArrayIterationDefault, _expr.getDefault());
        m_os << unindent;
    }

    m_os << unindent << L"\n}";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseArrayPartExpr(ArrayPartExpr &_expr) {
    VISITOR_ENTER(ArrayPartExpr, _expr);
    VISITOR_TRAVERSE_NS(Expression, ArrayPartObject, _expr.getObject());
    m_os << L"[" << indent;
    VISITOR_TRAVERSE_COL(Expression, ArrayPartIndex, _expr.getIndices());
    m_os << unindent << L"]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFieldExpr(FieldExpr &_expr) {
    VISITOR_ENTER(FieldExpr, _expr);
    VISITOR_TRAVERSE_NS(Expression, FieldObject, _expr.getObject());
    m_os << L"." << _expr.getFieldName();
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseMapElementExpr(MapElementExpr &_expr) {
    VISITOR_ENTER(MapElementExpr, _expr);
    VISITOR_TRAVERSE_NS(Expression, MapElementObject, _expr.getObject());
    m_os << L"[" << indent;
    VISITOR_TRAVERSE_NS(Expression, MapElementIndex, _expr.getIndex());
    m_os << unindent << L"]";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseListElementExpr(ListElementExpr &_expr) {
    VISITOR_ENTER(ListElementExpr, _expr);
    VISITOR_TRAVERSE_NS(Expression, ListElementObject, _expr.getObject());
    m_os << L"[" << indent;
    VISITOR_TRAVERSE_NS(Expression, ListElementIndex, _expr.getIndex());
    m_os << unindent << L"]";
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
        m_pContext->nameGenerator().collect(*m_pNode);
        m_pContext->collectPaths(*m_pNode);
        Param::updateUsed(*m_pNode);
        traverseNode(*m_pNode);
    } else if (m_bCompact)
        m_os << L"NULL";
}

void PrettyPrinterSyntax::print(Node &_node) {
    m_pContext->nameGenerator().collect(_node);
    m_pContext->collectPaths(_node);
    traverseNode(_node);
}

void PrettyPrinterSyntax::print(const NodePtr &_pNode) {
    if (!_pNode) {
        m_os << L"NULL";
        return;
    }
    print(*_pNode);
}

void prettyPrintSyntax(Node &_node, std::wostream & _os, const ContextPtr& _pContext, bool _bNewLine) {
    PrettyPrinterSyntax(_node, _os, _pContext).run();
    if (_bNewLine)
        _os << L"\n";
}

void prettyPrintSyntax(Node &_node, size_t nDepth, std::wostream & _os) {
    PrettyPrinterSyntax(_node, _os, nDepth).run();
}

void prettyPrintCompact(Node &_node, std::wostream &_os, int _nFlags, const ContextPtr& _pContext) {
    std::wstringstream wstringstream;
    PrettyPrinterSyntax(wstringstream, true, _nFlags, _pContext).print(_node);
    _os << removeRedundantSymbols(wstringstream.str(), L"\r\n ");
}

}
