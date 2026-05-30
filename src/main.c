/*
 * main.c — aligner CLI.
 *
 * Stage 1: just expose the lexer for testing.
 *
 *   aligner --lex FILE    print each token (kind, line:col, lexeme)
 *   aligner FILE          [stage 5+] full pipeline (not yet implemented)
 */

#include "ds.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "pass.h"
#include "emit.h"
#include "align.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_file(const char *path, char **out_buf, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    *out_buf = buf;
    *out_len = got;
    return 0;
}

/* Print a token in a parseable form. Escapes lexeme for display. */
static void print_tok(const cfmt_tok_t *t, FILE *out) {
    fprintf(out, "%-13s %u:%-3u len=%-3zu",
            cfmt_tok_kind_name(t->kind),
            t->line, t->col, t->lexeme_len);

    if (t->kind == TOK_STRING_LIT) {
        fprintf(out, " enc=%d bytes=%zu", (int)t->u.str.enc, t->u.str.byte_count);
    } else if (t->kind == TOK_CHAR_LIT) {
        fprintf(out, " enc=%d value=%lu", (int)t->u.chr.enc, t->u.chr.value);
    }

    fputs(" |", out);
    for (size_t i = 0; i < t->lexeme_len; i++) {
        unsigned char c = (unsigned char)t->lexeme[i];
        if (c == '\n')      fputs("\\n",  out);
        else if (c == '\t') fputs("\\t",  out);
        else if (c == '\r') fputs("\\r",  out);
        else if (c < 32)    fprintf(out, "\\x%02x", c);
        else                fputc((int)c, out);
    }
    fputs("|\n", out);
}

static int cmd_lex(const char *path) {
    char *src = NULL;
    size_t len = 0;
    if (read_file(path, &src, &len) != 0) {
        fprintf(stderr, "aligner: cannot read %s\n", path);
        return 1;
    }

    ds_context_t ctx;
    ds_arena_t   arena;
    ds_context_init(&ctx);
    /* 4 MiB arena: enough for typical input + decoded literals. */
    if (ds_arena_create(&arena, 4u * 1024u * 1024u, &ctx) != DS_OK) {
        fprintf(stderr, "aligner: arena alloc failed\n");
        free(src);
        return 1;
    }
    ds_context_use_arena(&ctx, &arena);

    cfmt_tok_stream_t stream;
    memset(&stream, 0, sizeof(stream));

    ds_status_t st = cfmt_lex(src, len, &ctx, &arena, &stream);
    if (st != DS_OK) {
        fprintf(stderr, "aligner: lex failed (%s)\n", ds_status_name(st));
        free(stream.tokens);
        ds_arena_destroy(&arena, &ctx);
        free(src);
        return 1;
    }

    for (size_t i = 0; i < stream.count; i++) {
        print_tok(&stream.tokens[i], stdout);
    }

    free(stream.tokens);
    ds_arena_destroy(&arena, &ctx);
    free(src);
    return 0;
}

static int cmd_ast(const char *path) {
    char *src = NULL;
    size_t len = 0;
    if (read_file(path, &src, &len) != 0) {
        fprintf(stderr, "aligner: cannot read %s\n", path);
        return 1;
    }

    ds_context_t ctx;
    ds_arena_t   arena;
    ds_context_init(&ctx);
    if (ds_arena_create(&arena, 16u * 1024u * 1024u, &ctx) != DS_OK) {
        fprintf(stderr, "aligner: arena alloc failed\n");
        free(src);
        return 1;
    }
    ds_context_use_arena(&ctx, &arena);

    cfmt_tok_stream_t stream;
    memset(&stream, 0, sizeof(stream));
    if (cfmt_lex(src, len, &ctx, &arena, &stream) != DS_OK) {
        fprintf(stderr, "aligner: lex failed\n");
        free(stream.tokens); ds_arena_destroy(&arena, &ctx); free(src);
        return 1;
    }

    cfmt_ast_t *root = cfmt_parse(&stream, &ctx);
    if (!root) {
        fprintf(stderr, "aligner: parse failed\n");
        free(stream.tokens); ds_arena_destroy(&arena, &ctx); free(src);
        return 1;
    }
    cfmt_ast_dump(root, &stream, stdout);

    free(stream.tokens);
    ds_arena_destroy(&arena, &ctx);
    free(src);
    return 0;
}

static int cmd_destring(const char *path) {
    char *src = NULL;
    size_t len = 0;
    if (read_file(path, &src, &len) != 0) {
        fprintf(stderr, "aligner: cannot read %s\n", path);
        return 1;
    }
    ds_context_t ctx; ds_arena_t arena;
    ds_context_init(&ctx);
    if (ds_arena_create(&arena, 16u * 1024u * 1024u, &ctx) != DS_OK) {
        fprintf(stderr, "aligner: arena alloc failed\n");
        free(src); return 1;
    }
    ds_context_use_arena(&ctx, &arena);

    cfmt_tok_stream_t stream; memset(&stream, 0, sizeof(stream));
    if (cfmt_lex(src, len, &ctx, &arena, &stream) != DS_OK) {
        fprintf(stderr, "aligner: lex failed\n");
        free(stream.tokens); ds_arena_destroy(&arena, &ctx); free(src);
        return 1;
    }
    cfmt_ast_t *root = cfmt_parse(&stream, &ctx);
    if (!root) {
        fprintf(stderr, "aligner: parse failed\n");
        free(stream.tokens); ds_arena_destroy(&arena, &ctx); free(src);
        return 1;
    }

    cfmt_pass_result_t result;
    if (cfmt_pass_result_init(&result, &stream, &ctx) != DS_OK) {
        fprintf(stderr, "aligner: pass init failed\n");
        free(stream.tokens); ds_arena_destroy(&arena, &ctx); free(src);
        return 1;
    }
    if (cfmt_pass_destring(root, &stream, &result) != DS_OK) {
        fprintf(stderr, "aligner: destring pass failed\n");
        free(stream.tokens); ds_arena_destroy(&arena, &ctx); free(src);
        return 1;
    }

    /* Summary. */
    printf("File-scope literals: %zu unique\n", result.file_lits.count);
    for (size_t i = 0; i < result.file_lits.count; i++) {
        const cfmt_lit_entry_t *e = result.file_lits.entries[i];
        printf("  s%08x  %zu bytes: \"", e->hash, e->byte_count);
        for (size_t j = 0; j < e->byte_count && j < 40; j++) {
            unsigned char c = e->bytes[j];
            if (c == '\n') fputs("\\n", stdout);
            else if (c < 32 || c >= 127) printf("\\x%02x", c);
            else putchar((int)c);
        }
        if (e->byte_count > 40) fputs("...", stdout);
        printf("\"\n");
    }
    printf("\nFunctions: %zu\n", result.func_lits_count);
    for (size_t i = 0; i < result.func_lits_count; i++) {
        const cfmt_lit_table_t *t = &result.func_lits[i];
        if (t->count == 0) continue;
        printf("  func[%zu]: %zu unique literals\n", i, t->count);
        for (size_t j = 0; j < t->count; j++) {
            const cfmt_lit_entry_t *e = t->entries[j];
            printf("    s%08x  %zu bytes: \"", e->hash, e->byte_count);
            for (size_t k = 0; k < e->byte_count && k < 32; k++) {
                unsigned char c = e->bytes[k];
                if (c == '\n') fputs("\\n", stdout);
                else if (c < 32 || c >= 127) printf("\\x%02x", c);
                else putchar((int)c);
            }
            if (e->byte_count > 32) fputs("...", stdout);
            printf("\"\n");
        }
    }

    /* Actions. */
    size_t n_var = 0, n_brace = 0, n_omit = 0, n_char = 0;
    for (size_t i = 0; i < result.actions_count; i++) {
        switch (result.actions[i].kind) {
            case CFMT_ACT_VAR_REF:    n_var++; break;
            case CFMT_ACT_BRACE_INIT: n_brace++; break;
            case CFMT_ACT_OMIT:       n_omit++; break;
            case CFMT_ACT_CHAR_NUM:   n_char++; break;
            default: break;
        }
    }
    printf("\nActions: VAR_REF=%zu BRACE_INIT=%zu OMIT=%zu CHAR_NUM=%zu\n",
           n_var, n_brace, n_omit, n_char);

    free(stream.tokens);
    free(result.actions);
    /* result.file_lits.entries and func_lits arrays were realloc'd, free them */
    free(result.file_lits.entries);
    for (size_t i = 0; i < result.func_lits_count; i++) {
        free(result.func_lits[i].entries);
    }
    free(result.func_lits);
    ds_arena_destroy(&arena, &ctx);
    free(src);
    return 0;
}

/* Full pipeline: lex → parse → destring → emit → align → stdout. */
static int cmd_format(const char *path) {
    char *src = NULL;
    size_t len = 0;
    if (read_file(path, &src, &len) != 0) {
        fprintf(stderr, "aligner: cannot read %s\n", path);
        return 1;
    }
    ds_context_t ctx; ds_arena_t arena;
    ds_context_init(&ctx);
    if (ds_arena_create(&arena, 64u * 1024u * 1024u, &ctx) != DS_OK) {
        fprintf(stderr, "aligner: arena alloc failed\n");
        free(src); return 1;
    }
    ds_context_use_arena(&ctx, &arena);

    cfmt_tok_stream_t stream; memset(&stream, 0, sizeof(stream));
    if (cfmt_lex(src, len, &ctx, &arena, &stream) != DS_OK) {
        fprintf(stderr, "aligner: lex failed\n"); return 1;
    }
    cfmt_ast_t *root = cfmt_parse(&stream, &ctx);
    if (!root) { fprintf(stderr, "aligner: parse failed\n"); return 1; }

    cfmt_pass_result_t result;
    if (cfmt_pass_result_init(&result, &stream, &ctx) != DS_OK ||
        cfmt_pass_destring(root, &stream, &result) != DS_OK) {
        fprintf(stderr, "aligner: destring failed\n"); return 1;
    }

    ds_str_t *emitted = str_create();
    if (!emitted) { fprintf(stderr, "aligner: emit-buffer alloc failed\n"); return 1; }
    if (cfmt_emit(root, &stream, &result, emitted) != DS_OK) {
        fprintf(stderr, "aligner: emit failed\n"); return 1;
    }

    ds_str_t *aligned = str_create();
    if (!aligned) { fprintf(stderr, "aligner: align-buffer alloc failed\n"); return 1; }
    if (cfmt_align(FUNC_str_data(emitted), FUNC_str_len(emitted), aligned) != DS_OK) {
        fprintf(stderr, "aligner: align failed\n"); return 1;
    }

    fwrite(FUNC_str_data(aligned), 1, FUNC_str_len(aligned), stdout);

    str_destroy(emitted);
    str_destroy(aligned);
    free(stream.tokens);
    free(result.actions);
    free(result.file_lits.entries);
    for (size_t i = 0; i < result.func_lits_count; i++) {
        free(result.func_lits[i].entries);
    }
    free(result.func_lits);
    ds_arena_destroy(&arena, &ctx);
    free(src);
    return 0;
}

static void usage(FILE *out) {
    fputs(
        "usage: aligner [--lex|--ast|--destring|--emit] FILE\n"
        "  --lex        dump the token stream and exit\n"
        "  --ast        dump the AST and exit\n"
        "  --destring   show what the destringer pass plans to do\n"
        "  --emit       emit unaligned post-destring text (no alignment)\n"
        "  (without flags, run the full pipeline)\n",
        out);
}

/* Like --format but stops after emit (unaligned). */
static int cmd_emit_only(const char *path) {
    char *src = NULL; size_t len = 0;
    if (read_file(path, &src, &len) != 0) return 1;
    ds_context_t ctx; ds_arena_t arena;
    ds_context_init(&ctx);
    ds_arena_create(&arena, 64u * 1024u * 1024u, &ctx);
    ds_context_use_arena(&ctx, &arena);
    cfmt_tok_stream_t stream; memset(&stream, 0, sizeof(stream));
    cfmt_lex(src, len, &ctx, &arena, &stream);
    cfmt_ast_t *root = cfmt_parse(&stream, &ctx);
    cfmt_pass_result_t result;
    cfmt_pass_result_init(&result, &stream, &ctx);
    cfmt_pass_destring(root, &stream, &result);
    ds_str_t *out = str_create();
    cfmt_emit(root, &stream, &result, out);
    fwrite(FUNC_str_data(out), 1, FUNC_str_len(out), stdout);
    str_destroy(out);
    free(stream.tokens);
    free(result.actions);
    free(result.file_lits.entries);
    for (size_t i = 0; i < result.func_lits_count; i++) free(result.func_lits[i].entries);
    free(result.func_lits);
    ds_arena_destroy(&arena, &ctx);
    free(src);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(stderr); return 2; }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout);
        return 0;
    }
    if (strcmp(argv[1], "--lex") == 0) {
        if (argc < 3) { usage(stderr); return 2; }
        return cmd_lex(argv[2]);
    }
    if (strcmp(argv[1], "--ast") == 0) {
        if (argc < 3) { usage(stderr); return 2; }
        return cmd_ast(argv[2]);
    }
    if (strcmp(argv[1], "--destring") == 0) {
        if (argc < 3) { usage(stderr); return 2; }
        return cmd_destring(argv[2]);
    }
    if (strcmp(argv[1], "--emit") == 0) {
        if (argc < 3) { usage(stderr); return 2; }
        return cmd_emit_only(argv[2]);
    }
    /* Default: full pipeline. */
    return cmd_format(argv[1]);
}
