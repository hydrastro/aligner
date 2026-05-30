/*
 * parser.h — token stream → AST.
 *
 * The parser is structural: it identifies top-level entities (preprocessor
 * directives, declarations, function definitions) and recursively models
 * brace-nested blocks inside function bodies. It does not attempt to parse
 * expression structure, types, or anything that would require name
 * resolution.
 */
#ifndef CFMT_PARSER_H
#define CFMT_PARSER_H

#include "ast.h"
#include "lexer.h"

cfmt_ast_t *cfmt_parse(cfmt_tok_stream_t *stream, ds_context_t *ctx);

#endif /* CFMT_PARSER_H */
