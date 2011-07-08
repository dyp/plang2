/// \file parser.h
///


#ifndef PARSER_H_
#define PARSER_H_

#include "lexer.h"
#include "ir/declarations.h"

/// Parse token stream.
/// \param _tokens Sequence of tokens to parse.
/// \return Module pointer.
ir::ModulePtr parse(lexer::Tokens &_tokens);

#endif /* PARSER_H_ */
