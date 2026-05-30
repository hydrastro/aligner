/*
 * ast.c — AST node helpers. The parser proper lives in parser.c.
 */

#include "ast.h"

#include <stdio.h>
#include <string.h>

const char *cfmt_ast_kind_name(cfmt_ast_kind_t k) {
    switch (k) {
        case AST_TRANSLATION_UNIT: return "TU";
        case AST_PP_DIRECTIVE:     return "PP";
        case AST_DECL:             return "DECL";
        case AST_FUNC_DEF:         return "FUNC_DEF";
        case AST_BLOCK:            return "BLOCK";
        case AST_UNKNOWN:          return "UNKNOWN";
    }
    return "?";
}

int cfmt_ast_walk(cfmt_ast_t *root,
                  int (*visit)(cfmt_ast_t *node, void *user),
                  void *user) {
    if (!root) return 0;
    int rc = visit(root, user);
    if (rc) return rc;
    for (cfmt_ast_t *c = root->first_child; c; c = c->next_sibling) {
        rc = cfmt_ast_walk(c, visit, user);
        if (rc) return rc;
    }
    return 0;
}

cfmt_ast_t *cfmt_ast_enclosing_func(cfmt_ast_t *node) {
    for (cfmt_ast_t *p = node; p; p = p->parent) {
        if (p->kind == AST_FUNC_DEF) return p;
    }
    return NULL;
}

static void dump_node(cfmt_ast_t *n, const cfmt_tok_stream_t *stream,
                      FILE *out, int depth) {
    for (int i = 0; i < depth; i++) fputs("  ", out);
    fprintf(out, "(%s [%zu..%zu)", cfmt_ast_kind_name(n->kind),
            n->first_tok_idx, n->last_tok_idx);
    if (n->kind == AST_PP_DIRECTIVE && n->first_tok_idx < stream->count) {
        const cfmt_tok_t *t = &stream->tokens[n->first_tok_idx];
        /* Show first line of the directive. */
        size_t len = t->lexeme_len;
        if (len > 40) len = 40;
        fputs(" \"", out);
        for (size_t i = 0; i < len; i++) {
            char c = t->lexeme[i];
            if (c == '\n') break;
            fputc((c < 32 || c == '"') ? '?' : c, out);
        }
        fputc('"', out);
    }
    if (!n->first_child) {
        fputs(")\n", out);
        return;
    }
    fputs("\n", out);
    for (cfmt_ast_t *c = n->first_child; c; c = c->next_sibling) {
        dump_node(c, stream, out, depth + 1);
    }
    for (int i = 0; i < depth; i++) fputs("  ", out);
    fputs(")\n", out);
}

void cfmt_ast_dump(cfmt_ast_t *root, const cfmt_tok_stream_t *stream, FILE *out) {
    dump_node(root, stream, out, 0);
}
