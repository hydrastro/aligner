/*
 * ast.h — coarse AST for cfmt.
 *
 * Design philosophy: a structural skeleton, not a full C parse tree. We track
 * what kind of region each token range belongs to (top-level entity, function
 * body, nested block) so that passes can answer "what scope owns this string
 * literal?" without doing semantic analysis.
 *
 * Nodes form a tree with explicit parent / first_child / next_sibling links.
 * Each node owns a half-open token range [first_tok, last_tok) into the token
 * stream the parser was given.
 *
 * All nodes are arena-allocated. There is no destroy step; the arena is
 * dropped when parsing is finished with.
 */
#ifndef CFMT_AST_H
#define CFMT_AST_H

#include "ds.h"
#include "lexer.h"

#include <stddef.h>

typedef enum {
    AST_TRANSLATION_UNIT = 0,
    AST_PP_DIRECTIVE,      /* a single PP line */
    AST_DECL,              /* file-scope declaration ending in ; */
    AST_FUNC_DEF,          /* function definition */
    AST_BLOCK,             /* { ... } compound region inside a function */
    AST_UNKNOWN            /* couldn't classify — passes pass it through raw */
} cfmt_ast_kind_t;

typedef struct cfmt_ast {
    cfmt_ast_kind_t kind;

    /* Half-open token range [first_tok_idx, last_tok_idx) into the stream
       given to cfmt_parse(). last_tok_idx is one past the last covered
       token, so first_tok_idx == last_tok_idx implies an empty range. */
    size_t first_tok_idx;
    size_t last_tok_idx;

    /* Tree links. */
    struct cfmt_ast *parent;
    struct cfmt_ast *first_child;
    struct cfmt_ast *last_child;
    struct cfmt_ast *next_sibling;

    /* For AST_FUNC_DEF: where the body's `{` is (token index). For AST_BLOCK
       and AST_FUNC_DEF, also where the matching `}` is. */
    size_t open_brace_idx;   /* index of { token; SIZE_MAX if N/A */
    size_t close_brace_idx;  /* index of } token; SIZE_MAX if N/A */
} cfmt_ast_t;

/* Parse `stream` into an AST. Returns the root (AST_TRANSLATION_UNIT) or NULL
   on allocation failure. All nodes allocated via the context. */
cfmt_ast_t *cfmt_parse(cfmt_tok_stream_t *stream, ds_context_t *ctx);

/* Walk the AST in pre-order, invoking `visit` for each node. The visitor may
   read the node but must not mutate the tree. If `visit` returns non-zero,
   the walk stops and the value is returned. */
int cfmt_ast_walk(cfmt_ast_t *root,
                  int (*visit)(cfmt_ast_t *node, void *user),
                  void *user);

/* Find the deepest AST_FUNC_DEF ancestor of `node`, or NULL if at file scope. */
cfmt_ast_t *cfmt_ast_enclosing_func(cfmt_ast_t *node);

/* Human-readable kind name (for debugging). */
const char *cfmt_ast_kind_name(cfmt_ast_kind_t k);

/* Dump the tree to `out` as nested s-expressions for debugging. */
void cfmt_ast_dump(cfmt_ast_t *root, const cfmt_tok_stream_t *stream, FILE *out);

#endif /* CFMT_AST_H */
