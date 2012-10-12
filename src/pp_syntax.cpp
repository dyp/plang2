
#include <list>
#include "pp_syntax.h"
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"

using namespace ir;

// NODE / MODULE
bool PrettyPrinterSyntax::_traverseDeclarationGroup(DeclarationGroup &_decl) {
    printGroup(_decl.getPredicates(), L"\n");
    printGroup(_decl.getTypes(), L";\n");
    printGroup(_decl.getVariables(), L";\n");
    printGroup(_decl.getFormulas(), L";\n");
    printGroup(_decl.getLemmas(), L";\n");

    VISITOR_TRAVERSE_COL(Message, MessageDecl, _decl.getMessages());
    VISITOR_TRAVERSE_COL(Process, ProcessDecl, _decl.getProcesses());
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

    VISITOR_TRAVERSE_COL(Module, ModuleDecl, _module.getModules());

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
    m_usedLabels.insert(_label.getName());
    m_os << _label.getName() << ": ";
    return true;
}

// NODE / TYPE
std::wstring fmtBits(int _bits) {
    switch (_bits) {
        case Number::GENERIC: return L"generic"; break;
        case Number::NATIVE:  return L"native";  break;
        default:              return fmtInt(_bits);
    }
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
    m_os << (m_bCompact ? L")" : L")");
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
    TypePtr pBaseType = _type.getBaseType();
    while (pBaseType->getKind() == Type::ARRAY)
        pBaseType = pBaseType.as<ArrayType>()->getBaseType();

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
        if (!bIsLast)
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
    return (getRole() != R_PredicateInParam && getRole() != R_PredicateOutParam);
}

std::wstring getNewFieldName(std::set<std::wstring>& _usedNames) {
    size_t i = 1;
    std::wstring strName;

    do {
        std::wstring strName = intToAlpha(i++);
    } while (!_usedNames.insert(strName).second);

    return strName;
}

bool PrettyPrinterSyntax::traverseStructType(StructType &_type) {
    VISITOR_ENTER(StructType, _type);

    m_os << (!m_bCompact ? L"struct(" : L"(");

    const bool bNeedsIndent = needsIndent() && !m_bCompact;
    if (bNeedsIndent)
        incTab();

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

    if (bNeedsIndent) {
        m_os << L"\n";
        decTab();
        m_os << fmtIndent();
    }
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

    const bool bNeedsIndent = needsIndent() && !m_bCompact;
    if (bNeedsIndent)
        incTab();

    for (size_t i = 0; i < _type.getConstructors().size(); ++i) {
        if (bNeedsIndent)
            m_os << L"\n" << fmtIndent();

        traverseUnionConstructorDeclaration(*_type.getConstructors().get(i));

        const bool bIsLast = (_type.getConstructors().size() - 1 == i);
        if (!bIsLast)
            m_os << ", ";
    }

    if (bNeedsIndent) {
        m_os << L"\n";
        decTab();
        m_os << fmtIndent();
    }
    m_os << L")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traversePredicateType(PredicateType &_type) {
    VISITOR_ENTER(PredicateType, _type);
    m_os << L"predicate(";
    VISITOR_TRAVERSE_COL(Param, PredicateTypeInParam, _type.getInParams());

    for (size_t i = 0; i < _type.getOutParams().size(); ++i)
        VISITOR_TRAVERSE_COL(Param, PredicateTypeOutParam, *_type.getOutParams().get(i));

    m_os << L")";
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
        names.push_back(getNewLabelName(strGeneralName));

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

//TODO: Use this function.
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
        m_os << "( ";
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
    VISITOR_TRAVERSE(Expression, LemmaDeclBody, _stmt.getProposition(), _stmt, LemmaDeclaration, setProposition);

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

    if (!_val.getName().empty() && getLoc().type != N_Param) {
        if (getRole() != R_EnumValueDecl)
            m_os << L" ";
        m_os << _val.getName();
    }

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
            if (pChild->getOperator() >= pParent->getOperator())
                return false;
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
    m_os << _node.getName();
    return false;
}

bool PrettyPrinterSyntax::visitPredicateReference(ir::PredicateReference &_node) {
    m_os << _node.getName();
    return true;
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

bool PrettyPrinterSyntax::traverseFunctionCall(ir::FunctionCall &_expr) {
    VISITOR_ENTER(FunctionCall, _expr);
    VISITOR_TRAVERSE(Expression, FunctionCallee, _expr.getPredicate(), _expr, FunctionCall, setPredicate);
    m_os << "(";
    VISITOR_TRAVERSE_COL(Expression, FunctionCallArgs, _expr.getArgs());
    m_os << ")";
    VISITOR_EXIT();
}

bool PrettyPrinterSyntax::traverseFormulaCall(ir::FormulaCall &_node) {

    m_os << _node.getName();

    m_os << "(";
    const bool bResult = Visitor::traverseFormulaCall(_node);
    m_os << ")";

    return bResult;

}

void PrettyPrinterSyntax::run() {
    if (m_pNode)
        traverseNode( *m_pNode );
    else if (m_bCompact)
        m_os << "NULL";
}

void PrettyPrinterSyntax::print(Node &_node) {
    if (&_node == NULL)
        m_os << "NULL";
    else
        traverseNode(_node);
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
std::wstring PrettyPrinterSyntax::getNewLabelName(const std::wstring& _name) {
    for (size_t i = 1;; ++i) {
        const std::wstring strName = _name + fmtInt(i, L"%d");
        if (m_usedLabels.insert(strName).second)
            return strName;
    }
}

class PrettyPrinterSyntaxCompact : public PrettyPrinterSyntax {
public:
    PrettyPrinterSyntaxCompact(ir::Node &_node, size_t _depth, std::wostream &_os) :
        PrettyPrinterSyntax(_node, _os), m_szDepth(_depth)
    {}

    virtual bool traverseStatement(Statement &_stmt) {
        bool result = true;
        if (m_szDepth != 0) {
            --m_szDepth;
            result = Visitor::traverseStatement(_stmt);
            ++m_szDepth;
        }
        else
            m_os << "...";
        return result;
    }

    virtual bool traverseAssignment(Assignment &_stmt) {
        VISITOR_ENTER(Assignment, _stmt);
        VISITOR_TRAVERSE(Expression, LValue, _stmt.getLValue(), _stmt, Assignment, setLValue);
        m_os << " := ";
        VISITOR_TRAVERSE(Expression, RValue, _stmt.getExpression(), _stmt, Assignment, setExpression);
        VISITOR_EXIT();
    }

    virtual bool traverseMultiassignment(Multiassignment &_stmt) {
        VISITOR_ENTER(Multiassignment, _stmt);
        m_os << "| ";
        VISITOR_TRAVERSE_COL(Expression, LValue, _stmt.getLValues());
        m_os << " | := | ";
        VISITOR_TRAVERSE_COL(Expression, RValue, _stmt.getExpressions());
        m_os << " |";
        VISITOR_EXIT();
    }

    virtual bool traverseBlock(Block &_stmt) {
        VISITOR_ENTER(Block, _stmt);
        m_os << "{ ";

        size_t i = 0;
        for (i = 0; i<_stmt.size() && i<m_szDepth; ++i) {
            traverseStatement(*_stmt.get(i));
            if (i + 1 != _stmt.size())
                m_os << "; ";
        }

        if (i<_stmt.size() && i == m_szDepth)
            m_os << " ... ";

        m_os << " }";
        VISITOR_EXIT();
    }

    virtual bool traverseParallelBlock(ParallelBlock &_stmt) {
        VISITOR_ENTER(ParallelBlock, _stmt);
        m_os << "{ ";

        size_t i = 0;
        for (i = 0; i<_stmt.size() && i<m_szDepth; ++i) {
            traverseStatement(*_stmt.get(i));
            if (i + 1 != _stmt.size())
                m_os << " || ";
        }

        if (i<_stmt.size() && i == m_szDepth)
            m_os << " ... ";

        m_os << " }";
        VISITOR_EXIT();
    }

    virtual bool traverseIf(If &_stmt) {
        VISITOR_ENTER(If, _stmt);
        m_os << "if (";
        VISITOR_TRAVERSE(Expression, IfArg, _stmt.getArg(), _stmt, If, setArg);
        m_os << ") ";
        VISITOR_TRAVERSE(Statement, IfBody, _stmt.getBody(), _stmt, If, setBody);
        if (_stmt.getElse()) {
            m_os << " else ";
            VISITOR_TRAVERSE(Statement, IfElse, _stmt.getElse(), _stmt, If, setElse);
        }
        VISITOR_EXIT();
    }

    virtual bool traverseSwitch(Switch &_stmt) {
        VISITOR_ENTER(Switch, _stmt);
        m_os << "switch (";
        VISITOR_TRAVERSE(Expression, SwitchArg, _stmt.getArg(), _stmt, Switch, setArg);
        m_os << ") ...";
        VISITOR_EXIT();
    }

    virtual bool traverseCall(Call &_stmt) {
        VISITOR_ENTER(Call, _stmt);
        if (_stmt.getPredicate()->getKind() == Expression::PREDICATE)
            m_os << _stmt.getPredicate().as<PredicateReference>()->getName();
        else
            m_os << "AnonymousPredicate";
        m_os << "(";
        VISITOR_TRAVERSE_COL(Expression, PredicateCallArgs, _stmt.getArgs());
        m_os << ": ";

        for (size_t i = 0; i < _stmt.getBranches().size(); ++i) {
            CallBranch &br = *_stmt.getBranches().get(i);
            if (_stmt.getBranches().size() > 1)
                m_os << "#" << i + 1 << " ";
            VISITOR_TRAVERSE_COL(Expression, PredicateCallBranchResults, br);
        }

        m_os << ")";
        VISITOR_EXIT();
    }

    virtual bool traversePredicate(Predicate &_stmt) {
        m_os << _stmt.getName();
        m_os << "(";
        VISITOR_TRAVERSE_COL(Param, PredicateInParam, _stmt.getInParams());
        m_os << ": ";
        for (size_t i = 0; i < _stmt.getOutParams().size(); ++i) {
            Branch &br = *_stmt.getOutParams().get(i);
            if (_stmt.getOutParams().size() > 1) {
                m_os << "#";
                if (br.getLabel())
                    m_os << br.getLabel()->getName() << " ";
                else
                    m_os << i + 1 << " ";
            }
            VISITOR_TRAVERSE_COL(Param, PredicateOutParam, br);
        }
        m_os << ")";
        return true;
    }

private:
    size_t m_szDepth;
};

void prettyPrintSyntax(ir::Node &_node, std::wostream & _os) {
    PrettyPrinterSyntax(_node, _os).run();
}

void prettyPrintSyntaxCompact(ir::Node &_node, size_t _depth, std::wostream & _os) {
    PrettyPrinterSyntaxCompact(_node, _depth, _os).run();
}
