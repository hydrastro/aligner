/*
 * parser.c — build a coarse AST from a token stream.
 *
 * Top level (cfmt_parse):
 *   { parse_top_level }* until EOF
 *
 * parse_top_level returns one of:
 *   - AST_PP_DIRECTIVE: a single TOK_PP_DIRECTIVE token
 *   - AST_DECL:         tokens up to and including the first `;` at paren
 *                       depth 0 (including any struct/union/enum bodies
 *                       found along the way)
 *   - AST_FUNC_DEF:     tokens up to and including the matching `}` of the
 *                       function body, where the body opened with `{`
 *                       immediately after a `)` at paren depth 0
 *   - AST_UNKNOWN:      anything we couldn't classify
 *
 * Inside a function body, parse_block walks nested `{` ... `}` regions and
 * builds AST_BLOCK children. Everything else (statements, expressions)
 * is implicit in the parent block's token range — we don't model
 * statements individually.
 *
 * Comments, whitespace, and newlines are tokens. They are part of the parent
 * range but never "significant" for classification.
 */

#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "ds.h"

#include <stdio.h>
#include <string.h>

/* ----- Parser state ------------------------------------------------------- */

typedef struct {
    cfmt_tok_stream_t *stream;
    ds_context_t      *ctx;
    size_t             i;     /* current token index */
} parser_t;

static cfmt_ast_t *new_node(parser_t *P, cfmt_ast_kind_t kind, size_t first) {
    cfmt_ast_t *n = (cfmt_ast_t *)ds_context_alloc(P->ctx, sizeof(*n));
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    n->kind            = kind;
    n->first_tok_idx   = first;
    n->last_tok_idx    = first;
    n->open_brace_idx  = (size_t)-1;
    n->close_brace_idx = (size_t)-1;
    return n;
}

static void append_child(cfmt_ast_t *parent, cfmt_ast_t *child) {
    child->parent = parent;
    if (parent->last_child) {
        parent->last_child->next_sibling = child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
}

static const cfmt_tok_t *par_peek(parser_t *P) {
    return &P->stream->tokens[P->i];
}

static int at_end(parser_t *P) {
    return P->stream->tokens[P->i].kind == TOK_EOF;
}

/* True if the token kind is "significant": affects classification. */
static int par_is_significant(cfmt_tok_kind_t k) {
    return k != TOK_WHITESPACE &&
           k != TOK_NEWLINE    &&
           k != TOK_COMMENT_LINE &&
           k != TOK_COMMENT_BLOCK;
}

/* (next_sig_idx removed; classify() walks inline.) */


/* ----- Block parsing ------------------------------------------------------ */

/* parse_block — caller has positioned P->i AT a `{` punct. Consumes through
   the matching `}`. Returns AST_BLOCK or NULL on allocation failure. */
static cfmt_ast_t *parse_block(parser_t *P) {
    size_t open = P->i;
    cfmt_ast_t *block = new_node(P, AST_BLOCK, open);
    if (!block) return NULL;
    block->open_brace_idx = open;
    P->i++;  /* consume { */

    while (!at_end(P)) {
        const cfmt_tok_t *t = par_peek(P);

        if (cfmt_tok_is_punct(t, "}")) {
            block->close_brace_idx = P->i;
            P->i++;
            block->last_tok_idx = P->i;
            return block;
        }
        if (cfmt_tok_is_punct(t, "{")) {
            cfmt_ast_t *nested = parse_block(P);
            if (!nested) return NULL;
            append_child(block, nested);
            continue;
        }
        if (t->kind == TOK_PP_DIRECTIVE) {
            cfmt_ast_t *pp = new_node(P, AST_PP_DIRECTIVE, P->i);
            if (!pp) return NULL;
            P->i++;
            pp->last_tok_idx = P->i;
            append_child(block, pp);
            continue;
        }
        P->i++;
    }

    /* Unterminated block — close it where we ended. */
    block->last_tok_idx = P->i;
    return block;
}

/* ----- Top-level parsing -------------------------------------------------- */

/* Scan forward from start to classify the next top-level entity.
 * The classification is based on what punctuator we hit first at paren
 * depth 0 (ignoring tokens inside `{...}` brace-init bodies):
 *   - `;`             → DECL
 *   - `{` after `)`   → FUNC_DEF
 *   - `{` otherwise   → DECL with a brace body (struct/union/enum/init)
 *   - EOF             → UNKNOWN
 * On return, *p_body_open points at the `{` if FUNC_DEF (else SIZE_MAX).
 * *p_end is the index of the terminating `;` or `}` (inclusive). */
static cfmt_ast_kind_t classify(parser_t *P, size_t start,
                                size_t *p_body_open, size_t *p_end) {
    *p_body_open = (size_t)-1;
    *p_end       = (size_t)-1;

    int paren_depth = 0;
    size_t last_sig = (size_t)-1;
    size_t i = start;
    const cfmt_tok_stream_t *s = P->stream;

    while (i < s->count) {
        const cfmt_tok_t *t = &s->tokens[i];
        if (t->kind == TOK_EOF) break;
        if (!par_is_significant(t->kind)) { i++; continue; }

        if (cfmt_tok_is_punct(t, "(")) {
            paren_depth++;
            last_sig = i;
            i++;
            continue;
        }
        if (cfmt_tok_is_punct(t, ")")) {
            if (paren_depth > 0) paren_depth--;
            last_sig = i;
            i++;
            continue;
        }
        if (paren_depth == 0 && cfmt_tok_is_punct(t, ";")) {
            *p_end = i;
            return AST_DECL;
        }
        if (paren_depth == 0 && cfmt_tok_is_punct(t, "{")) {
            /* Function body iff previous significant token was ) */
            int is_func = 0;
            if (last_sig != (size_t)-1 &&
                cfmt_tok_is_punct(&s->tokens[last_sig], ")")) {
                is_func = 1;
            }
            if (is_func) {
                *p_body_open = i;
                /* Find matching }. */
                size_t j = i + 1;
                int depth = 1;
                while (j < s->count && depth > 0) {
                    const cfmt_tok_t *u = &s->tokens[j];
                    if (cfmt_tok_is_punct(u, "{")) depth++;
                    else if (cfmt_tok_is_punct(u, "}")) depth--;
                    j++;
                }
                *p_end = (depth == 0) ? (j - 1) : j;
                return AST_FUNC_DEF;
            }
            /* Brace-init or struct/union/enum body — skip to matching }. */
            size_t j = i + 1;
            int depth = 1;
            while (j < s->count && depth > 0) {
                const cfmt_tok_t *u = &s->tokens[j];
                if (cfmt_tok_is_punct(u, "{")) depth++;
                else if (cfmt_tok_is_punct(u, "}")) depth--;
                j++;
            }
            i = j;
            last_sig = (j > 0) ? (j - 1) : 0;
            continue;
        }
        last_sig = i;
        i++;
    }
    *p_end = (i < s->count) ? i : (s->count - 1);
    return AST_UNKNOWN;
}

/* Parse one top-level entity, append to parent, advance P->i. */
static int parse_top_level(parser_t *P, cfmt_ast_t *parent) {
    const cfmt_tok_t *t = par_peek(P);

    /* Skip leading non-significant tokens — they belong to whatever follows.
       (For now we attach them to the next entity by starting its range at
       the same position; this keeps the AST losslessly cover all tokens.) */

    if (t->kind == TOK_PP_DIRECTIVE) {
        cfmt_ast_t *pp = new_node(P, AST_PP_DIRECTIVE, P->i);
        if (!pp) return -1;
        P->i++;
        pp->last_tok_idx = P->i;
        append_child(parent, pp);
        return 0;
    }

    /* Skip whitespace/newlines/comments — they'll be absorbed by the next
       significant entity. */
    if (!par_is_significant(t->kind)) {
        /* Create a tiny AST_UNKNOWN that covers just this token so the tree
           still spans every token (useful for emit). */
        cfmt_ast_t *u = new_node(P, AST_UNKNOWN, P->i);
        if (!u) return -1;
        P->i++;
        u->last_tok_idx = P->i;
        append_child(parent, u);
        return 0;
    }

    size_t start = P->i;
    size_t body_open, end_idx;
    cfmt_ast_kind_t k = classify(P, start, &body_open, &end_idx);

    if (k == AST_DECL) {
        cfmt_ast_t *decl = new_node(P, AST_DECL, start);
        if (!decl) return -1;
        P->i = end_idx + 1;  /* past the ; */
        decl->last_tok_idx = P->i;
        append_child(parent, decl);
        return 0;
    }

    if (k == AST_FUNC_DEF) {
        cfmt_ast_t *fn = new_node(P, AST_FUNC_DEF, start);
        if (!fn) return -1;
        fn->open_brace_idx  = body_open;
        fn->close_brace_idx = end_idx;
        /* Jump to body open, parse_block, then we should be at end_idx+1. */
        P->i = body_open;
        cfmt_ast_t *body = parse_block(P);
        if (!body) return -1;
        append_child(fn, body);
        fn->last_tok_idx = P->i;
        append_child(parent, fn);
        return 0;
    }

    /* AST_UNKNOWN: consume to end_idx + 1 to make progress. */
    cfmt_ast_t *u = new_node(P, AST_UNKNOWN, start);
    if (!u) return -1;
    P->i = end_idx + 1;
    if (P->i <= start) P->i = start + 1;
    u->last_tok_idx = P->i;
    append_child(parent, u);
    return 0;
}

/* ----- Public entry ------------------------------------------------------- */

cfmt_ast_t *cfmt_parse(cfmt_tok_stream_t *stream, ds_context_t *ctx) {
    parser_t P;
    P.stream = stream;
    P.ctx    = ctx;
    P.i      = 0;

    cfmt_ast_t *tu = new_node(&P, AST_TRANSLATION_UNIT, 0);
    if (!tu) return NULL;

    while (!at_end(&P)) {
        if (parse_top_level(&P, tu) != 0) return NULL;
    }
    tu->last_tok_idx = P.i;
    return tu;
}
