/*
 * pass.h — analysis passes over the AST.
 *
 * Passes do not mutate the AST. They produce side data that the emitter
 * consumes. Right now we have one pass — destring — but the result struct
 * is designed so we can add more.
 */
#ifndef CFMT_PASS_H
#define CFMT_PASS_H

#include "ast.h"
#include "ds.h"
#include "lexer.h"

#include <stddef.h>

/* One unique string literal, deduplicated by hash within its scope. */
typedef struct cfmt_lit_entry {
    unsigned int hash;
    unsigned char *bytes;     /* arena-owned copy of decoded content */
    size_t         byte_count;
} cfmt_lit_entry_t;

/* Ordered set of literal entries — deduplicated and stably ordered (by
 * first-seen order, so emit is deterministic). */
typedef struct cfmt_lit_table {
    cfmt_lit_entry_t **entries;   /* dynamic array */
    size_t count;
    size_t cap;
    ds_string_map_t *by_hash;     /* hex hash → entry ptr (dedup) */
    ds_context_t   *ctx;
} cfmt_lit_table_t;

/* What to do with a particular token at emit time. */
typedef enum {
    CFMT_ACT_NONE = 0,         /* keep as-is */
    CFMT_ACT_VAR_REF,          /* STRING_LIT → reference to sXXX variable */
    CFMT_ACT_BRACE_INIT,       /* STRING_LIT → {b, b, ..., 0} for char[] init */
    CFMT_ACT_CHAR_NUM,         /* CHAR_LIT → decimal integer value */
    CFMT_ACT_OMIT              /* trailing token in adjacent-string run */
} cfmt_action_kind_t;

typedef struct cfmt_action {
    cfmt_action_kind_t kind;
    cfmt_lit_entry_t  *entry;  /* for VAR_REF / BRACE_INIT */
} cfmt_action_t;

/* Per-AST-node side data for emit. We use a simple resizable array indexed
 * by sequential function index (0, 1, 2, ...) — the emitter walks FUNC_DEFs
 * in source order and reads func_lits[seen_so_far]. */
typedef struct cfmt_pass_result {
    cfmt_lit_table_t  file_lits;       /* file-scope literals */
    cfmt_lit_table_t *func_lits;       /* one per FUNC_DEF in source order */
    size_t            func_lits_count;
    size_t            func_lits_cap;

    cfmt_action_t    *actions;         /* size == stream->count */
    size_t            actions_count;

    ds_context_t     *ctx;
} cfmt_pass_result_t;

/* Initialize a fresh pass result. The caller-supplied context drives all
   allocations. */
ds_status_t cfmt_pass_result_init(cfmt_pass_result_t *r,
                                  cfmt_tok_stream_t *stream,
                                  ds_context_t      *ctx);

/* Run the destringer pass.
 * For every STRING_LIT token, register it in the appropriate literal table
 * (file scope or its enclosing function scope) and record an emit action.
 * For every CHAR_LIT token whose use-context is not the lexeme inside a
 * preprocessor directive (those are inside PP tokens already), record a
 * CHAR_NUM action. */
ds_status_t cfmt_pass_destring(cfmt_ast_t        *root,
                               cfmt_tok_stream_t *stream,
                               cfmt_pass_result_t *result);

/* FNV-1a 32-bit (exposed for tests). */
unsigned int cfmt_fnv1a(const unsigned char *data, size_t n);

#endif /* CFMT_PASS_H */
