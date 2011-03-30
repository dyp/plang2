////////////////
// Nodes
////////////////

// Types.
NODE(Type, Node)
NODE(EnumType, Type)
NODE(StructType, Type)
NODE(UnionType, Type)
NODE(DerivedType, Type)
NODE(ArrayType, DerivedType)
NODE(SetType, DerivedType)
NODE(MapType, DerivedType)
NODE(ListType, DerivedType)
NODE(Subtype, Type)
NODE(Range, Type)
NODE(PredicateType, Type)
NODE(ParameterizedType, Type)
NODE(NamedReferenceType, Type)
NODE(TypeType, Type)

// Named.
NODE(NamedValue, Node)
NODE(EnumValue, NamedValue)
NODE(Param, NamedValue)
NODE(Variable, NamedValue)

// Expression.
NODE(Expression, Node)
NODE(Literal, Expression)
NODE(VariableReference, Expression)
NODE(PredicateReference, Expression)
NODE(Unary, Expression)
NODE(Binary, Expression)
NODE(Ternary, Expression)
NODE(TypeExpr, Expression)
NODE(Component, Expression)
NODE(ArrayPartExpr, Component)
NODE(StructFieldExpr, Component)
NODE(UnionAlternativeExpr, Component)
NODE(MapElementExpr, Component)
NODE(ListElementExpr, Component)
NODE(Replacement, Component)
NODE(FunctionCall, Expression)
NODE(FormulaCall, Expression)
NODE(Lambda, Expression)
NODE(Binder, Expression)
NODE(Formula, Expression)
NODE(Constructor, Expression)
NODE(StructConstructor, Constructor)
NODE(ArrayConstructor, Constructor)
NODE(SetConstructor, Constructor)
NODE(MapConstructor, Constructor)
NODE(ListConstructor, Constructor)
NODE(ArrayIteration, Constructor)
NODE(UnionConstructor, Constructor)
NODE(CastExpr, Expression)

// Statements.
NODE(Statement, Node)
NODE(Block, Statement)
NODE(ParallelBlock, Statement)
NODE(Jump, Statement)
NODE(Assignment, Statement)
NODE(Multiassignment, Statement)
NODE(Call, Statement)
NODE(Switch, Statement)
NODE(If, Statement)
NODE(For, Statement)
NODE(While, Statement)
NODE(Break, Statement)
NODE(With, Statement)
NODE(Receive, Statement)
NODE(Send, Statement)
NODE(TypeDeclaration, Statement)
NODE(VariableDeclaration, Statement)
NODE(FormulaDeclaration, Statement)
NODE(LemmaDeclaration, Statement)
NODE(Predicate, Statement)

// Misc.
NODE(UnionConstructorDeclaration, Node)
NODE(StructFieldDefinition, Node)
NODE(ElementDefinition, Node)
NODE(ArrayPartDefinition, Node)
NODE(Label, Node)
NODE(SwitchCase, Node)
NODE(DeclarationGroup, Node)
NODE(Module, DeclarationGroup)
NODE(Class, DeclarationGroup)
NODE(Process, Node)
NODE(Message, Node)
NODE(MessageHandler, Node)
