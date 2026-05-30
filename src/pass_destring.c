/*
 * pass_destring.c — string-literal lift-and-rename pass.
 *
 * For every TOK_STRING_LIT we plan a replacement:
 *   - If the run of adjacent string literals (single logical literal in C
 *     terms) sits in array-initializer context (` ... ] = "..."`), the
 *     first token is replaced with a brace-initializer `{b,b,...,0}` and
 *     subsequent tokens in the run are omitted.
 *   - Otherwise, the run is replaced with a reference to a generated
 *     variable `sXXXXXXXX` (FNV-1a hex hash), declared once at the start
 *     of its enclosing function (or as a `static const char` array at file
 *     scope if it appears outside any function).
 *
 * Adjacent strings sharing an encoding prefix (plain / L / u / U / u8) are
 * concatenated. Mixed-prefix runs are left alone (rare; the program punts).
 *
 * For every plain (encoding-NONE) TOK_CHAR_LIT we plan a CHAR_NUM action so
 * the emitter writes the decimal value instead of the quoted character.
 */

#include "pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- FNV-1a 32-bit --------------------------------------------------------- */

unsigned int cfmt_fnv1a(const unsigned char *data, size_t n) {
    unsigned int h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

/* --- Literal tables ------------------------------------------------------- */

static ds_status_t lit_table_init(cfmt_lit_table_t *t, ds_context_t *ctx) {
    memset(t, 0, sizeof(*t));
    t->ctx = ctx;
    t->by_hash = ds_string_map_create_ctx(ctx);
    if (!t->by_hash) return DS_ERR_ALLOC;
    return DS_OK;
}

/* Intern `bytes`/`byte_count` into `t`. If an entry with the same hash is
 * already present, returns it. Otherwise allocates a new entry and adds it. */
static cfmt_lit_entry_t *lit_table_intern(cfmt_lit_table_t *t,
                                          const unsigned char *bytes,
                                          size_t byte_count) {
    unsigned int hash = cfmt_fnv1a(bytes, byte_count);

    /* Key = hex representation of the hash. */
    char key[16];
    snprintf(key, sizeof(key), "%08x", hash);

    /* Look up. ds_string_map_get returns DS_OK with non-NULL value on hit,
       DS_NOT_FOUND otherwise. */
    void *existing = NULL;
    if (ds_string_map_get(t->by_hash, key, &existing) == DS_OK && existing) {
        return (cfmt_lit_entry_t *)existing;
    }

    /* Create new entry. */
    cfmt_lit_entry_t *e = (cfmt_lit_entry_t *)ds_context_alloc(t->ctx, sizeof(*e));
    if (!e) return NULL;
    e->hash = hash;
    e->byte_count = byte_count;
    if (byte_count > 0) {
        e->bytes = (unsigned char *)ds_context_alloc(t->ctx, byte_count);
        if (!e->bytes) return NULL;
        memcpy(e->bytes, bytes, byte_count);
    } else {
        e->bytes = NULL;
    }

    /* Push into ordered array. */
    if (t->count >= t->cap) {
        size_t newcap = t->cap ? t->cap * 2 : 16;
        cfmt_lit_entry_t **nb = (cfmt_lit_entry_t **)realloc(
            t->entries, newcap * sizeof(cfmt_lit_entry_t *));
        if (!nb) return NULL;
        t->entries = nb;
        t->cap = newcap;
    }
    t->entries[t->count++] = e;

    /* Insert into hash dedup map (key is strdup'd internally). */
    if (ds_string_map_put(t->by_hash, key, e) != DS_OK) return NULL;

    return e;
}

/* --- Pass result allocation ----------------------------------------------- */

ds_status_t cfmt_pass_result_init(cfmt_pass_result_t *r,
                                  cfmt_tok_stream_t *stream,
                                  ds_context_t      *ctx) {
    memset(r, 0, sizeof(*r));
    r->ctx = ctx;
    if (lit_table_init(&r->file_lits, ctx) != DS_OK) return DS_ERR_ALLOC;
    r->actions = (cfmt_action_t *)calloc(stream->count, sizeof(cfmt_action_t));
    if (!r->actions) return DS_ERR_ALLOC;
    r->actions_count = stream->count;
    return DS_OK;
}

/* --- Helpers --------------------------------------------------------------- */

static int pd_is_significant(cfmt_tok_kind_t k) {
    return k != TOK_WHITESPACE &&
           k != TOK_NEWLINE    &&
           k != TOK_COMMENT_LINE &&
           k != TOK_COMMENT_BLOCK;
}

/* True iff the declarator immediately before `eq_idx` (the `=`) is
 * pointer-like — i.e., contains an unmatched `*` at brace-depth 0 between
 * the previous declaration boundary (`;` or `,` at depth 0) and `eq_idx`.
 * This lets us tell apart:
 *
 *   char arr[]   = { ... }         no `*`           → BRACE_INIT
 *   char *p      = { ... }         `*` at depth 0   → VAR_REF
 *   char *a[]    = { "...", ...}   `*` at depth 0   → VAR_REF
 *   struct X x   = { ... }         no `*` at d0     → BRACE_INIT
 *   struct{char *p;} s = {...}     `*` only inside  → BRACE_INIT (limit)
 *                                  the struct body
 *
 * Brace-depth 0 means we skip over the contents of struct/union/enum
 * definitions like `struct {char *p;}`, which can contain `*` belonging to
 * fields, not to the outer variable. */
static int declarator_is_pointer(const cfmt_tok_stream_t *s, long eq_idx) {
    /* Walk back from eq_idx-1 until hitting `;` or `,` at top brace and
       paren depth, marking that point as `start`. */
    long start = eq_idx - 1;
    int brace = 0;
    int paren = 0;
    while (start >= 0) {
        const cfmt_tok_t *t = &s->tokens[start];
        if (!pd_is_significant(t->kind)) { start--; continue; }
        if (cfmt_tok_is_punct(t, "}")) { brace++; start--; continue; }
        if (cfmt_tok_is_punct(t, ")")) { paren++; start--; continue; }
        if (cfmt_tok_is_punct(t, "{")) {
            if (brace > 0) { brace--; start--; continue; }
            /* Hit an enclosing { — stop. */
            start++;
            break;
        }
        if (cfmt_tok_is_punct(t, "(")) {
            if (paren > 0) { paren--; start--; continue; }
            start++;
            break;
        }
        if (brace == 0 && paren == 0 &&
            (cfmt_tok_is_punct(t, ";") || cfmt_tok_is_punct(t, ","))) {
            start++;
            break;
        }
        start--;
    }
    if (start < 0) start = 0;

    /* Now scan forward from start to eq_idx looking for `*` at brace-depth 0
       and paren-depth 0. */
    brace = 0; paren = 0;
    for (long i = start; i < eq_idx; i++) {
        const cfmt_tok_t *t = &s->tokens[i];
        if (!pd_is_significant(t->kind)) continue;
        if (cfmt_tok_is_punct(t, "{")) { brace++; continue; }
        if (cfmt_tok_is_punct(t, "}")) { if (brace) brace--; continue; }
        if (cfmt_tok_is_punct(t, "(")) { paren++; continue; }
        if (cfmt_tok_is_punct(t, ")")) { if (paren) paren--; continue; }
        if (brace == 0 && paren == 0 && cfmt_tok_is_punct(t, "*")) {
            return 1;
        }
    }
    return 0;
}

/* Decide whether a string literal at index `i` is in an "init context" —
 * meaning the destringer should expand it as a brace initializer in place
 * (so the surrounding aggregate/array assignment continues to work).
 *
 * Returns:
 *   0 — not init context; emit as VAR_REF (default)
 *   1 — array/aggregate init context AND the declarator isn't pointer-like;
 *       emit as BRACE_INIT
 *
 * The two patterns we recognize as init contexts:
 *
 *   (a) Direct array init:  T name [N] = STRING  /  T name [] = STRING
 *       Walking backward from STRING through significant tokens, the first
 *       is `=`, and `]` appears before any boundary token (`;`/`,`/`(`/`{`).
 *
 *   (b) Inside a (possibly nested) brace initializer:
 *           T name = { ..., STRING, ... }
 *           T arr[] = { {...}, {STRING}, ... }
 *           struct {char buf[N];} s = {STRING, ...};
 *       Walking backward we cross `{`s whose immediate predecessor is `=` or
 *       another `{`/`,` of an enclosing init brace. paren_depth and
 *       brace_depth track nesting along the way.
 *
 * Once we find the anchoring `=`, we inspect the declarator before it for a
 * top-level `*` (declarator_is_pointer). If the variable being initialized
 * is pointer-like, we return 0 so the literal is lifted as a separate
 * named array and the original `{ ... }` slots become pointer references —
 * preserving the original semantics. */
static int looks_like_array_init(const cfmt_tok_stream_t *s, size_t i) {
    long j = (long)i - 1;
    int brace_depth = 0;
    int paren_depth = 0;

    while (j >= 0) {
        cfmt_tok_kind_t k = s->tokens[j].kind;
        if (!pd_is_significant(k)) { j--; continue; }
        const cfmt_tok_t *t = &s->tokens[j];

        if (cfmt_tok_is_punct(t, "}")) { brace_depth++; j--; continue; }
        if (cfmt_tok_is_punct(t, ")")) { paren_depth++; j--; continue; }

        if (cfmt_tok_is_punct(t, "{")) {
            if (brace_depth > 0) { brace_depth--; j--; continue; }
            /* Enclosing `{`. Check what came right before. */
            long p = j - 1;
            while (p >= 0 && !pd_is_significant(s->tokens[p].kind)) p--;
            if (p < 0) return 0;
            const cfmt_tok_t *prev = &s->tokens[p];
            if (cfmt_tok_is_punct(prev, "=")) {
                /* Anchor `=` is at index p. Decide based on declarator. */
                return declarator_is_pointer(s, p) ? 0 : 1;
            }
            if (cfmt_tok_is_punct(prev, "{") ||
                cfmt_tok_is_punct(prev, ",")) {
                /* Nested init brace; continue walking back past this `{`. */
                j = p;
                continue;
            }
            return 0;
        }

        if (cfmt_tok_is_punct(t, "(")) {
            if (paren_depth > 0) { paren_depth--; j--; continue; }
            return 0;  /* inside parens (call/decl), not init */
        }

        if (cfmt_tok_is_punct(t, "=") && brace_depth == 0 && paren_depth == 0) {
            /* Direct `... = STRING` pattern: array iff `]` before a boundary
               AND the declarator isn't pointer-like. */
            long p = j - 1;
            int saw_bracket = 0;
            while (p >= 0) {
                if (!pd_is_significant(s->tokens[p].kind)) { p--; continue; }
                const cfmt_tok_t *u = &s->tokens[p];
                if (cfmt_tok_is_punct(u, "]")) { saw_bracket = 1; break; }
                if (cfmt_tok_is_punct(u, ";") ||
                    cfmt_tok_is_punct(u, ",") ||
                    cfmt_tok_is_punct(u, "(") ||
                    cfmt_tok_is_punct(u, "{")) break;
                p--;
            }
            if (!saw_bracket) return 0;
            return declarator_is_pointer(s, j) ? 0 : 1;
        }

        if (cfmt_tok_is_punct(t, ";") && brace_depth == 0 && paren_depth == 0) {
            return 0;  /* statement boundary; not in init */
        }

        j--;
    }
    return 0;
}

/* Find the maximal adjacent-string run starting at index `i` and sharing the
 * encoding of stream->tokens[i]. Returns the index one past the last
 * STRING_LIT token in the run (note: this is NOT the same as "past the last
 * token", because there may be insignificant tokens after). */
static size_t adjacent_string_run_end(const cfmt_tok_stream_t *s, size_t i) {
    cfmt_enc_t enc = s->tokens[i].u.str.enc;
    size_t last_string = i;
    size_t j = i + 1;
    while (j < s->count) {
        cfmt_tok_kind_t k = s->tokens[j].kind;
        if (k == TOK_STRING_LIT) {
            if (s->tokens[j].u.str.enc != enc) break;
            last_string = j;
            j++;
            continue;
        }
        if (pd_is_significant(k) || k == TOK_PP_DIRECTIVE) break;
        j++;
    }
    return last_string + 1;  /* half-open past the last STRING_LIT */
}

/* --- Function-table bookkeeping ------------------------------------------- */

/* Allocate the next per-function literal table slot, return a pointer to it. */
static cfmt_lit_table_t *next_func_table(cfmt_pass_result_t *r) {
    if (r->func_lits_count >= r->func_lits_cap) {
        size_t newcap = r->func_lits_cap ? r->func_lits_cap * 2 : 8;
        cfmt_lit_table_t *nb = (cfmt_lit_table_t *)realloc(
            r->func_lits, newcap * sizeof(cfmt_lit_table_t));
        if (!nb) return NULL;
        r->func_lits = nb;
        r->func_lits_cap = newcap;
    }
    cfmt_lit_table_t *slot = &r->func_lits[r->func_lits_count++];
    if (lit_table_init(slot, r->ctx) != DS_OK) {
        r->func_lits_count--;
        return NULL;
    }
    return slot;
}

/* --- Token-range processing ---------------------------------------------- */

typedef struct {
    cfmt_tok_stream_t  *stream;
    cfmt_pass_result_t *result;
} ctx_t;

/* Process tokens in [start, end) that belong directly to this AST node
   (i.e., not owned by a deeper child). `active_table` is the lit table for
   the enclosing scope. */
static int process_own_tokens(ctx_t *C, cfmt_ast_t *node,
                              cfmt_lit_table_t *active_table) {
    size_t i = node->first_tok_idx;
    /* Iterate but skip the ranges of children. */
    while (i < node->last_tok_idx) {
        /* If this token is inside a child, skip past that child. */
        cfmt_ast_t *into_child = NULL;
        for (cfmt_ast_t *c = node->first_child; c; c = c->next_sibling) {
            if (i >= c->first_tok_idx && i < c->last_tok_idx) {
                into_child = c;
                break;
            }
        }
        if (into_child) { i = into_child->last_tok_idx; continue; }

        cfmt_tok_t *t = &C->stream->tokens[i];

        if (t->kind == TOK_STRING_LIT) {
            /* Wide-encoding strings: leave alone (would need wchar / utf16/32
               handling). */
            if (t->u.str.enc != CFMT_ENC_NONE) { i++; continue; }

            size_t run_end = adjacent_string_run_end(C->stream, i);
            /* Concat bytes of every STRING_LIT in [i, run_end). */
            size_t total = 0;
            for (size_t k = i; k < run_end; k++) {
                if (C->stream->tokens[k].kind == TOK_STRING_LIT) {
                    total += C->stream->tokens[k].u.str.byte_count;
                }
            }
            unsigned char *concat = NULL;
            if (total > 0) {
                concat = (unsigned char *)ds_context_alloc(C->result->ctx, total);
                if (!concat) return -1;
                size_t off = 0;
                for (size_t k = i; k < run_end; k++) {
                    if (C->stream->tokens[k].kind == TOK_STRING_LIT) {
                        memcpy(concat + off,
                               C->stream->tokens[k].u.str.bytes,
                               C->stream->tokens[k].u.str.byte_count);
                        off += C->stream->tokens[k].u.str.byte_count;
                    }
                }
            }

            int array_init = looks_like_array_init(C->stream, i);
            cfmt_lit_entry_t *e = lit_table_intern(active_table, concat, total);
            if (!e) return -1;

            C->result->actions[i].kind =
                array_init ? CFMT_ACT_BRACE_INIT : CFMT_ACT_VAR_REF;
            C->result->actions[i].entry = e;

            /* Mark trailing string tokens in the run as OMIT. */
            for (size_t k = i + 1; k < run_end; k++) {
                if (C->stream->tokens[k].kind == TOK_STRING_LIT) {
                    C->result->actions[k].kind = CFMT_ACT_OMIT;
                }
            }
            i = run_end;
            continue;
        }

        if (t->kind == TOK_CHAR_LIT && t->u.chr.enc == CFMT_ENC_NONE) {
            C->result->actions[i].kind = CFMT_ACT_CHAR_NUM;
        }
        i++;
    }
    return 0;
}

/* Recursive walk: maintain the active table; create a new one per FUNC_DEF. */
static int walk(ctx_t *C, cfmt_ast_t *node, cfmt_lit_table_t *active) {
    cfmt_lit_table_t *here = active;

    if (node->kind == AST_FUNC_DEF) {
        here = next_func_table(C->result);
        if (!here) return -1;
    }

    if (process_own_tokens(C, node, here) != 0) return -1;

    for (cfmt_ast_t *c = node->first_child; c; c = c->next_sibling) {
        if (walk(C, c, here) != 0) return -1;
    }
    return 0;
}

/* --- Entry point ---------------------------------------------------------- */

ds_status_t cfmt_pass_destring(cfmt_ast_t        *root,
                               cfmt_tok_stream_t *stream,
                               cfmt_pass_result_t *result) {
    ctx_t C;
    C.stream = stream;
    C.result = result;
    if (walk(&C, root, &result->file_lits) != 0) return DS_ERR_ALLOC;
    return DS_OK;
}
