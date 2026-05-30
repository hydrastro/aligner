/*
 * emit.c — produce unaligned post-transformation text.
 *
 * The output is later fed to the aligner, which makes every line exactly
 * 80 characters wide.
 */

#include "emit.h"
#include "ast.h"
#include "lexer.h"
#include "pass.h"
#include "ds.h"

#include <stdio.h>
#include <string.h>

/* --- Output helpers ------------------------------------------------------ */

static int put_bytes(ds_str_t *out, const char *data, size_t n) {
    return str_append(out, data, n);
}
static int put_cstr(ds_str_t *out, const char *s) {
    return str_append_cstr(out, s);
}
static int put_fmt(ds_str_t *out, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int rc = str_append_vfmt(out, fmt, ap);
    va_end(ap);
    return rc;
}

/* Emit the "var name" form of a literal entry: sXXXXXXXX (8 hex). */
static int put_varname(ds_str_t *out, const cfmt_lit_entry_t *e) {
    return put_fmt(out, "s%08x", e->hash);
}

/* --- File-scope literal declarations ------------------------------------- */
/*
 *   static const char sXXX[N]={
 *   b0,
 *   b1,
 *   ...
 *   0};
 *
 * The array's length includes the null terminator.
 */
static int emit_file_scope_decl(ds_str_t *out, const cfmt_lit_entry_t *e) {
    size_t total = e->byte_count + 1;  /* +1 for null terminator */
    int rc = 0;
    rc |= put_fmt(out, "static const char s%08x[%zu]={\n", e->hash, total);
    for (size_t i = 0; i < e->byte_count; i++) {
        rc |= put_fmt(out, "%u,\n", (unsigned)e->bytes[i]);
    }
    rc |= put_cstr(out, "0};\n");
    return rc;
}

/* --- Function-scope literal declaration ---------------------------------- */
/*
 * For VAR_REF use inside a function we emit:
 *
 *   static const char sXXX[N]={
 *   b0,
 *   b1,
 *   ...
 *   0};
 *
 * `static const` is critical for two reasons:
 *   - lifetime: a string literal in C has static storage duration; a code
 *     path that returns or stashes a pointer to it must continue to work
 *     after the transformation. A non-static function-local `char[]` would
 *     dangle after return.
 *   - correctness for unique-instance semantics: making it const matches
 *     the original literal (writing to a string literal is UB).
 *
 * Each element is on its own line so the aligner can format each at 80
 * chars wide.
 */
static int emit_func_scope_decl(ds_str_t *out, const cfmt_lit_entry_t *e) {
    size_t total = e->byte_count + 1;
    int rc = 0;
    rc |= put_fmt(out, "static const char s%08x[%zu]={\n", e->hash, total);
    for (size_t i = 0; i < e->byte_count; i++) {
        rc |= put_fmt(out, "%u,\n", (unsigned)e->bytes[i]);
    }
    rc |= put_cstr(out, "0};\n");
    return rc;
}

/* --- In-place brace-init expansion --------------------------------------- */
/*
 * Replaces a STRING_LIT token in array-init context:
 *
 *   {
 *   b0,
 *   b1,
 *   ...
 *   0}
 *
 * The opening `{` is on its own line so it joins the previous token's line
 * (typical context: `char arr[]={`). The closing `}` is on the last byte's
 * line. Surrounding `;` (or `,`) comes from the next token.
 */
static int emit_brace_init(ds_str_t *out, const cfmt_lit_entry_t *e) {
    int rc = 0;
    rc |= put_cstr(out, "{\n");
    for (size_t i = 0; i < e->byte_count; i++) {
        rc |= put_fmt(out, "%u,\n", (unsigned)e->bytes[i]);
    }
    rc |= put_cstr(out, "0}");
    return rc;
}

/* --- Per-token emission -------------------------------------------------- */

static int emit_token(ds_str_t *out, const cfmt_tok_t *t, const cfmt_action_t *a) {
    /* Comments are stripped entirely. The aligner is destructive by
       construction — string literals become byte arrays, character literals
       become decimal numbers — and leaving comments would jar against that
       aesthetic. The surrounding whitespace/newline tokens stay, so a
       full-line comment becomes a blank line. */
    if (t->kind == TOK_COMMENT_LINE || t->kind == TOK_COMMENT_BLOCK) {
        return 0;
    }
    switch (a->kind) {
        case CFMT_ACT_NONE:
            return put_bytes(out, t->lexeme, t->lexeme_len);
        case CFMT_ACT_VAR_REF:
            return put_varname(out, a->entry);
        case CFMT_ACT_BRACE_INIT:
            return emit_brace_init(out, a->entry);
        case CFMT_ACT_CHAR_NUM:
            return put_fmt(out, "%lu", t->u.chr.value);
        case CFMT_ACT_OMIT:
            return 0;
    }
    return 0;
}

/* --- Walk the AST -------------------------------------------------------- */

typedef struct {
    cfmt_tok_stream_t  *stream;
    cfmt_pass_result_t *result;
    ds_str_t           *out;
    size_t              func_index;
} emit_ctx_t;

/* Emit all tokens in [start, end) that are *own tokens* of `node` (i.e., not
   in any child's range). Walks the slice; for each token, either applies its
   action or recurses into the child that contains it.

   If `node` is a function body (i.e., a BLOCK whose parent is a FUNC_DEF),
   we inject the function-scope literal preamble right after emitting the
   opening `{`. */
static int emit_node(emit_ctx_t *C, cfmt_ast_t *node) {
    int is_func_body =
        node->kind == AST_BLOCK &&
        node->parent != NULL &&
        node->parent->kind == AST_FUNC_DEF &&
        node->parent->open_brace_idx == node->open_brace_idx;

    cfmt_ast_t *child = node->first_child;
    size_t i = node->first_tok_idx;

    while (i < node->last_tok_idx) {
        /* If a child starts at this position, descend into it. */
        while (child && child->first_tok_idx < i) child = child->next_sibling;
        if (child && child->first_tok_idx == i) {
            if (emit_node(C, child) != 0) return -1;
            i = child->last_tok_idx;
            child = child->next_sibling;
            continue;
        }

        cfmt_tok_t *t = &C->stream->tokens[i];
        cfmt_action_t *a = &C->result->actions[i];

        if (is_func_body && i == node->open_brace_idx) {
            /* Emit the `{` itself. If this function has lifted literals,
               emit a newline then the declarations. Skip the extra newline
               when there are no declarations — otherwise we'd insert a
               spurious blank line and break idempotence. */
            if (emit_token(C->out, t, a) != 0) return -1;
            cfmt_lit_table_t *tab = &C->result->func_lits[C->func_index];
            if (tab->count > 0) {
                put_cstr(C->out, "\n");
                for (size_t k = 0; k < tab->count; k++) {
                    if (emit_func_scope_decl(C->out, tab->entries[k]) != 0) return -1;
                }
            }
            C->func_index++;
            i++;
            continue;
        }

        if (emit_token(C->out, t, a) != 0) return -1;
        i++;
    }
    return 0;
}

/* --- Public entry -------------------------------------------------------- */

ds_status_t cfmt_emit(cfmt_ast_t        *root,
                      cfmt_tok_stream_t *stream,
                      cfmt_pass_result_t *result,
                      ds_str_t          *out) {
    /* 1. Preamble: file-scope literals. */
    for (size_t i = 0; i < result->file_lits.count; i++) {
        if (emit_file_scope_decl(out, result->file_lits.entries[i]) != 0) {
            return DS_ERR_ALLOC;
        }
    }

    /* 2. Walk the AST emitting all tokens (with actions applied). */
    emit_ctx_t C;
    C.stream = stream;
    C.result = result;
    C.out    = out;
    C.func_index = 0;
    if (emit_node(&C, root) != 0) return DS_ERR_ALLOC;
    return DS_OK;
}
