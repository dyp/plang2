/// \file parser.h
///


#ifndef PARSER_H_
#define PARSER_H_

#include "lexer.h"
#include "ir/declarations.h"
#include "typecheck.h"
#include "prettyprinter.h"

/// Parse token stream.
/// \param _tokens Sequence of tokens to parse.
/// \return Module pointer.
ir::ModulePtr parse(lexer::Tokens &_tokens);

tc::ContextPtr parseTypeConstraints(lexer::Tokens &_tokens,
        tc::Formulas & _constraints, FreshTypeNames & _freshTypeNames);

#endif /* PARSER_H_ */
