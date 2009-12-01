grammar plang;

options {
	backtrack = false;
}

s : program;

WS    :  (' ' | '\r' | '\t' | '\u000C' | '\n') {$channel=HIDDEN;};

fragment LETTER : 'a'..'z' | 'A'..'Z';
fragment IDENT_CHAR : '_' | LETTER | '0'..'9';

IDENT : LETTER IDENT_CHAR*
	|	'_' IDENT_CHAR+;

QUALIFIED_IDENT : IDENT ('.' IDENT)+;

fragment SIGN       : '+' | '-';
fragment DIGITS     : DIGIT+;
fragment DIGIT      : '0'..'9';
fragment HEX_DIGITS : HEX_DIGIT+;
fragment HEX_DIGIT  : DIGIT | 'a'..'f' | 'A'..'F';

NAT : DIGIT+;
INT : SIGN DIGITS | '0x' HEX_DIGITS;

fragment EXPONENT : ('e' | 'E') SIGN? DIGITS;

REAL
	: SIGN? (DIGITS ('.' DIGITS)? EXPONENT? | 'inf')
	| 'nan'
	;

fragment ORDINARY_CHAR : ~('\u0000'..'\u0019' | '\"' | '\'' | '\\');
fragment ESCAPED_CHAR  : '\\' ('\'' | '\"' | '\\' | '0' | 'n' | 't' | 'r');
fragment CHAR_CODE     : '\\x' HEX_DIGIT (HEX_DIGIT (HEX_DIGIT HEX_DIGIT?)?)?;

CHAR : ORDINARY_CHAR | ESCAPED_CHAR | CHAR_CODE;

STRING : '"' (CHAR | '\'')* '"';

LABEL : DIGIT+ LETTER IDENT_CHAR*;

program : module_header? imports? declarations?;

module_header : 'module' simple_ident ';';

imports : ('import' simple_ident ';')+;

declarations : declaration (';' declaration)*;

simple_declaration
	: type_declaration
	| declaration_statement
	| message_declaration
	| predicate_definition
	| process_definition
	| pragma
	;

declaration
	: simple_declaration
	| class_definition
	;

identifier : IDENT | QUALIFIED_IDENT;

simple_ident : IDENT;

qualified_ident : QUALIFIED_IDENT;

label : LABEL | IDENT | NAT;

//
// Primitive literals.
//

literal
	: int_literal
	| real_literal
	| char_literal
	| string_literal
	| bool_literal
	| nil_literal
	| special_literal
	;

// to offset initialization of array dimensions
special_literal : '...';

int_literal : INT | NAT;

real_literal : REAL;

char_literal : '\'' (CHAR | '\"') '\'';

string_literal : STRING;

bool_literal : 'true' | 'false';

nil_literal : 'nil';

//
// Types.
//

type_declaration : 'type' simple_ident ('(' parameter_list ')')? ('=' type_reference)?;

type_reference
	: (named_type | type_definition) '*'?
	;

type_definition
	: primitive_type ('(' int_literal ')')?
	| subtype
	| enum_type
	| composite_type
	| predicate_type
	;

primitive_type
	: 'nat'
	| 'int'
	| 'real'
	| 'bool'
	| 'char'
	;

param_type
	: type_reference
	| generic_type
	;

generic_type : 'type';

named_type : identifier ('(' expression_list ')')?;

predicate_type : 'predicate' predicate_head;

subtype
	: 'subtype' '(' type_reference simple_ident ':' expression ')'
	| subrange;

subrange : (param_name | literal | '(' expression ')') '..' simple_expr;

enum_type : 'enum' '(' simple_ident (',' simple_ident)* ')';

composite_type
	: struct_type
	| union_type
	| string_type
	| seq_type
	| set_type
	| array_type
	
	// extension
	| map_type
	| list_type
	;

struct_type : 'struct' '(' parameter_list ')';

union_type : 'union' '(' alternatives ')';

alternatives : alternative (',' alternative )*;

alternative : type_reference simple_ident;

string_type : 'string';

seq_type : 'seq' '(' type_reference ')';

set_type : 'set' '(' type_reference ')';

array_type : 'array' '(' type_reference (',' subrange)+ ')';

map_type : 'map' '(' type_reference ',' type_reference ')';

list_type : 'list' '(' type_reference ')';

//
// Expressions.
//

expression
	: quantified_expr
	| array_constructor
	| instantiation
	;

param_name : identifier '\''?;

atom
	: simple_ident '\''?
	| qualified_ident
	| primitive_type
	| literal
	| composite_literal
	| '(' expression ')'
	
	// Implementation should also handle (non LL(k)):
	// | type_reference // Type as an expression.
	// | type_reference composite_literal // Unambigous type of literal.
	;

simple_expr
	: atom component*
	| lambda
	;

simple_component : array_element_or_replacement;

component
	: simple_component
	| function_call
	;

array_element_or_replacement : '[' array_elements ']' | array_parts_definition;

function_call : '(' expression_list ')';

power_expr : simple_expr ('^' power_expr)?;

mult_expr : power_expr (('*' | '/' | '%') power_expr)*;

unary_expr
	: mult_expr
	| ('!' | '~' | '-') unary_expr;

sum_expr : unary_expr (('+' | '-') unary_expr)*;

shift_expr : sum_expr (('<<' | '>>') sum_expr)*;

in_expr : shift_expr ('in' shift_expr)*;

cmp_expr : in_expr (('<' | '<=' | '>=' | '>') in_expr)*;

eq_expr : cmp_expr (('=' | '!=') cmp_expr)*;

and_expr : eq_expr ('&' eq_expr)*;

xor_expr : and_expr ('xor' and_expr)*;

or_expr : xor_expr ('or' xor_expr)*;

ternary_expr : or_expr ('?' or_expr ':' or_expr)?;

// Subranges should be handled as atom -> type_reference -> subtype -> subrange.
subrange_expr : ternary_expr ('..' ternary_expr)?;

implication_expr : subrange_expr (('=>' | '<=>') subrange_expr)?;

quantified_expr
	: implication_expr
	| quantifier parameter_list '.' implication_expr
	;

quantifier : '\\A' | '\\E';

expression_list : expression (',' expression)*;

// Literals.

composite_literal
	: struct_literal
	| array_literal
	| set_literal
	
	// extension
	| map_literal
	| list_literal
	;

// FIXME
struct_literal : '((' struct_field_values? '))'; 

struct_field_values : struct_field_value (',' struct_field_value)+;

// Label rule allows using that same definition for unions.
struct_field_value : (label ':')? expression;

array_literal : '[' array_elements? ']';

array_elements : array_element (',' array_element)*;

array_element : expression (':' expression)?;

set_literal : '{' set_elements? '}';

set_elements : expression_list;

map_literal : '[{' array_elements '}]';

list_literal : '[[' array_elements ']]';

variable : param_name simple_component*;

variable_list : variable (',' variable)*;

lambda : 'predicate' predicate_head predicate_body;

array_constructor : 'for' '(' parameter_list ')' array_constructor_body;

array_constructor_body
	: expression
	| array_parts_definition
	;

array_parts_definition : '{' array_part_case+ array_part_default? '}';

array_indices
	: expression_list
//	| '(' expression_list ')' (',' '(' expression_list ')')* // UNCOMMENT ME
	;

array_part_case : 'case' array_indices ':' expression;

array_part_default : 'default' ':' expression;

// Predicates.

predicate_definition
	: predicate_name predicate_head
		(preconditions? predicate_body postconditions? | '<=>' formula);

predicate_name : simple_ident;

predicate_head : '(' predicate_params ')';

predicate_params : in_params? (':' out_params?)?;

preconditions : 'pre' formula (branch_precondition)*;

branch_precondition : 'pre' label ':' formula;

postconditions : (branch_postcondition)+;

branch_postcondition : 'post' (label ':')? formula;

formula : quantified_expr;

predicate_body : block;

in_params : parameter_list;

parameter_list : param_type simple_ident result_mark? (',' param_type? simple_ident)*;

// If present after param X, param X' with the same type is prepended
// to the list of output parameters (not applicable to hyperfunctions)
result_mark : '*';

out_param_name : param_name;

parameter_group_out : type_reference out_param_name (',' out_param_name)*;

parameter_list_out : parameter_group_out (',' parameter_group_out)* ('#' label)?;

out_params : parameter_list_out (':' parameter_list_out)*;

// Statements.

statement
	: declaration_statement
	| (label ':')? executable_statement
	;

simple_statement
	: multiassignment
	| jump
	| block
	| assignment_or_call
	| switch
	| pragma

	// Processes.
	| message_receive
	| message_send
	| synchronized_block

	// Imperative extension.
	| imperative_for
	| imperative_break
	;

// Should be joined with type declarations in implementation.
declaration_statement : ('const' | 'mutable') type_reference simple_ident '=' expression;

executable_statement
	: conditional
	| simple_statement
	;

statements
	: declaration_statement (';' statements)?
	| executable_statement ((';' | '||') statements)?
	;

block : '{' statements? ';'? '}';

assignment_or_call : variable (assignment_tail | call_tail);

assignment_tail : '=' expression;

assignment : variable assignment_tail | multiassignment;

multiassignment : multivariable '=' multiexpression;

multivariable : '|' variable_list '|';

multiexpression : '|' expression_list '|';

jump : '#' label;

if : 'if' '(' expression ')' simple_statement;

// Nested if's require curly braces.
conditional : if ('else' if)* 'else' simple_statement;

call_params : '(' call_param_list ')';

// ':' can be omitted with imperative call with no result.
call_param_list
	:  expression_list (':' call_results?)?
	| ':' call_results?
	;

call_tail : call_params ('{' branch_handlers '}')?;

call_results : call_branch_results (':' call_branch_results)*;

call_branch_results : variable_list jump? | jump;

branch_handlers : branch_handler+;
branch_handler : 'case' label ':' statement;

switch : 'switch' '(' expression ')' '{' case_handler* '}';

case_handler
	: 'case' expression ':' statement
	| 'default' ':' statement
	;

//
// Imperative statements.
//

imperative_for
	: 'for' '(' for_declaration ';' expression ';' statement ')' statement;

for_declaration : type_reference? simple_ident assignment_tail?;

imperative_break : 'break';

imperative_while : 'while' '(' expression ')' statement;

//
// Processes.
//

process_definition
	: 'process' simple_ident predicate_head ? '{' process_body? '}';

process_body : statement (';' statement)*;

message_receive : 'receive' (message | '{' receive_body '}');

// Subset of function_call, should be detected while parsing.
message : identifier ('(' variable_list ')')?;

receive_single : message ':' statement;

receive_body : receive_single (';' 'or' receive_single)* timeout?;

timeout : 'after' expression ':' statement;

message_send : identifier '!' identifier ('(' expression_list ')')?;

type_list : type_reference (',' type_reference)*;

message_declaration : processing_type simple_ident ('(' type_list ')')?;

processing_type : 'message' | 'queue';

instantiation : 'new' identifier '(' expression_list? ')';

synchronized_block : 'with' '(' variable_list ')' block;

//
// Classes.
//

class_definition : 'class' simple_ident ('extends' identifier)? '{' class_body? '}';

class_body : class_member (';' class_member)*;

class_member : simple_declaration;

//
// Pragmas.
//

pragma : 'pragma' pragma_header (',' pragma_header)* block?;

pragma_header : pragma_name ('(' pragma_param (',' pragma_param)*)?;
// pragma (int_bitness: 12, int_overflow: wrap);
pragma_name
	: 'int_bitness'
	| 'real_bitness'
	| 'int_overflow'
	| 'real_overflow'
	;

pragma_param
	: literal
	| jump
	| pragma_keyword
	;

pragma_keyword
	: 'native' | 'unbounded' // Bitness control.
	| 'safe' | 'wrap' | 'strict' | 'fail' // Overflow control.
	;
