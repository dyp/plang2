/// \file parser.h
///


#ifndef PARSER_H_
#define PARSER_H_

#include "lexer.h"
#include "ir/declarations.h"

/// Parse token stream.
/// \param _tokens Sequence of tokens to parse.
/// \param _pModule Reference to Module pointer. Will be allocated when parssing.
///     Caller is responsible for deleting it.
/// \return True if parsed successfully, false otherwise.
bool parse(lexer::Tokens & _tokens, ir::Module * & _pModule);

#endif /* PARSER_H_ */
