/*
 * emit.h — AST + actions → unaligned text.
 *
 * The emitter walks the AST in pre-order. It outputs:
 *   1. A preamble of file-scope literal declarations.
 *   2. For each token, either its lexeme verbatim or its planned action's
 *      replacement (var-ref, brace-init, char-num, or omit).
 *   3. At each function-body open `{`, the function's local literal
 *      declarations (followed by per-byte assignments) before the rest of
 *      the body's tokens.
 *
 * The output format is line-oriented and intentionally produces many short
 * lines (one byte per line within brace inits and assignments). The aligner
 * then reflows each line to exactly 80 columns.
 */
#ifndef CFMT_EMIT_H
#define CFMT_EMIT_H

#include "ast.h"
#include "ds.h"
#include "lexer.h"
#include "pass.h"

/* Emit unaligned text into `out` (an already-created ds_str_t). */
ds_status_t cfmt_emit(cfmt_ast_t        *root,
                      cfmt_tok_stream_t *stream,
                      cfmt_pass_result_t *result,
                      ds_str_t          *out);

#endif /* CFMT_EMIT_H */
