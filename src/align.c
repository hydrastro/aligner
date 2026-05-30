/*
 * align.c — line aligner.
 *
 * The input is text (the destringer's emit output, or any earlier
 * aligner output if we're checking idempotence). We re-tokenize it with
 * our own lexer so that numbers (including hex floats like 0x1F.8p+2),
 * string literals (including L"..." / u"..." / U"..." / u8"..."), and
 * character literals are all atomic — the aligner never tries to peer
 * inside them.
 *
 * Each significant token then falls into one of two classes:
 *   word:    IDENT / INT_LIT / FLOAT_LIT          (left side)
 *   symbol:  PUNCT / STRING_LIT / CHAR_LIT        (right side)
 *
 * Word atoms get joined with single-space separators. Symbol atoms get
 * concatenated with no separator (whitespace between adjacent punctuators
 * is purely visual; dropping it is valid C). A NEWLINE flushes the current
 * (word, symbol) pair onto one output line, padded to LINE_WIDTH.
 *
 * Preprocessor directives keep their iconic one-line look (with `\`-
 * continuations preserved). We don't try to format individual tokens
 * inside them.
 */

#include "align.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>

#define LINE_WIDTH 80

static int aln_is_hspace(int c) { return c == ' ' || c == '\t'; }

static int is_word_char(int c) {
    return c == '_'
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9');
}

static int is_blank(const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c != ' ' && c != '\t' && c != '\r') return 0;
    }
    return 1;
}

/* Pad word + symbol to LINE_WIDTH columns, with a trailing newline. */
static int emit_pair(const char *wb, size_t wlen,
                     const char *sb, size_t slen,
                     ds_str_t *out) {
    long pad;
    int rc = 0;
    if (wlen + slen == 0) return str_pushc(out, '\n');
    if (wlen > 0) rc |= str_append(out, wb, wlen);
    pad = (long)LINE_WIDTH - (long)wlen - (long)slen;
    if (pad > 0) {
        long k;
        for (k = 0; k < pad; k++) rc |= str_pushc(out, ' ');
    }
    if (slen > 0) rc |= str_append(out, sb, slen);
    rc |= str_pushc(out, '\n');
    return rc;
}

/* Preprocessor line: word prefix (leading `#` glued on) left, the rest of
   the line right-justified. One source line → one output line. Called
   once per `\`-terminated segment of a PP directive's lexeme. */
static int align_pp_line(const char *line, size_t len, ds_str_t *out) {
    size_t start, split, left_end, right_end;
    if (is_blank(line, len)) return str_pushc(out, '\n');

    start = 0;
    while (start < len && aln_is_hspace((unsigned char)line[start])) start++;

    split = start;
    if (split < len && line[split] == '#') split++;
    while (split < len) {
        char c = line[split];
        if (is_word_char((unsigned char)c) || aln_is_hspace((unsigned char)c))
            split++;
        else break;
    }

    /* Function-like macro guard. `#define NAME(args) body` requires NO
       whitespace between NAME and `(`; inserting alignment padding there
       would silently turn the macro into an object-like one. If the next
       character is `(` and it's directly glued to the preceding word
       (no whitespace), consume `(...)` (matching parens) as part of the
       word prefix so padding lands AFTER the closing paren. */
    if (split < len && line[split] == '('
        && split > start
        && !aln_is_hspace((unsigned char)line[split - 1])) {
        int depth = 0;
        while (split < len) {
            char c = line[split];
            if (c == '(') depth++;
            else if (c == ')') depth--;
            split++;
            if (depth == 0) break;
        }
    }

    left_end = split;
    while (left_end > start && aln_is_hspace((unsigned char)line[left_end - 1]))
        left_end--;

    right_end = len;
    while (right_end > split &&
           (aln_is_hspace((unsigned char)line[right_end - 1])
            || line[right_end - 1] == '\r'))
        right_end--;

    return emit_pair(line + start, left_end - start,
                     line + split, right_end - split, out);
}

/* Grow buffer in-place. */
static int reserve(char **buf, size_t *cap, size_t need) {
    char *nb;
    size_t nc;
    if (*cap >= need) return 0;
    nc = *cap ? *cap : 256;
    while (nc < need) nc *= 2;
    nb = (char *)realloc(*buf, nc);
    if (!nb) return -1;
    *buf = nb; *cap = nc;
    return 0;
}

static int append_word(char **buf, size_t *cap, size_t *len,
                       const char *s, size_t n) {
    /* Word atoms get a single-space separator if there's already a word. */
    int rc;
    if (*len > 0) {
        rc = reserve(buf, cap, *len + 1 + n);
        if (rc != 0) return rc;
        (*buf)[(*len)++] = ' ';
    } else {
        rc = reserve(buf, cap, *len + n);
        if (rc != 0) return rc;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    return 0;
}

static int append_sym(char **buf, size_t *cap, size_t *len,
                      const char *s, size_t n) {
    /* Symbol atoms concatenate directly. */
    int rc = reserve(buf, cap, *len + n);
    if (rc != 0) return rc;
    memcpy(*buf + *len, s, n);
    *len += n;
    return 0;
}

/* Walk a PP_DIRECTIVE token's lexeme, splitting on internal `\n` chars
   (the embedded `\<newline>` continuations) and emitting one aligned line
   per piece. The trailing `\` stays at the end of each continuation line,
   right-justified to col 80, which is the iconic look. */
static int align_pp_token(const cfmt_tok_t *t, ds_str_t *out) {
    const char *lx = t->lexeme;
    size_t      ln = t->lexeme_len;
    size_t      i  = 0;
    while (i < ln) {
        size_t j = i;
        while (j < ln && lx[j] != '\n') j++;
        if (align_pp_line(lx + i, j - i, out) != 0) return -1;
        i = (j < ln) ? j + 1 : j;
    }
    return 0;
}

ds_status_t cfmt_align(const char *in, size_t in_len, ds_str_t *out) {
    ds_context_t      ctx;
    ds_arena_t        arena;
    cfmt_tok_stream_t stream;
    char  *wbuf = NULL, *sbuf = NULL;
    size_t wcap = 0,    scap = 0;
    size_t wlen = 0,    slen = 0;
    ds_status_t st;
    size_t i;
    int rc = 0;

    ds_context_init(&ctx);
    if (ds_arena_create(&arena, 4u * 1024u * 1024u, &ctx) != DS_OK) {
        return DS_ERR_ALLOC;
    }
    ds_context_use_arena(&ctx, &arena);
    memset(&stream, 0, sizeof(stream));

    st = cfmt_lex(in, in_len, &ctx, &arena, &stream);
    if (st != DS_OK) {
        if (stream.tokens) free(stream.tokens);
        ds_arena_destroy(&arena, &ctx);
        return st;
    }

    /* `pp_pending_nl` is set right after a PP_DIRECTIVE was emitted (which
       already wrote a trailing '\n'); the lexer's NEWLINE for that same
       source line then gets consumed silently to avoid a doubled newline. */
    int pp_pending_nl = 0;

    for (i = 0; i < stream.count && rc == 0; i++) {
        const cfmt_tok_t *t = &stream.tokens[i];
        switch (t->kind) {
            case TOK_NEWLINE:
                if (pp_pending_nl) {
                    pp_pending_nl = 0;
                } else {
                    rc = emit_pair(wbuf, wlen, sbuf, slen, out);
                    wlen = slen = 0;
                }
                break;

            case TOK_PP_DIRECTIVE:
                if (wlen > 0 || slen > 0) {
                    rc = emit_pair(wbuf, wlen, sbuf, slen, out);
                    wlen = slen = 0;
                }
                if (rc == 0) rc = align_pp_token(t, out);
                pp_pending_nl = 1;
                break;

            case TOK_WHITESPACE:
            case TOK_COMMENT_LINE:
            case TOK_COMMENT_BLOCK:
                /* Stripped — comments are gone by design; whitespace is
                   re-synthesized as padding. */
                break;

            case TOK_IDENT:
            case TOK_INT_LIT:
            case TOK_FLOAT_LIT:
                /* A new word arriving after we've already collected a
                   symbol means the previous (word, symbol) pair is done. */
                if (slen > 0) {
                    rc = emit_pair(wbuf, wlen, sbuf, slen, out);
                    wlen = slen = 0;
                }
                if (rc == 0)
                    rc = append_word(&wbuf, &wcap, &wlen,
                                     t->lexeme, t->lexeme_len);
                break;

            case TOK_PUNCT:
            case TOK_STRING_LIT:
            case TOK_CHAR_LIT:
                /* Atomic on the symbol side. String/char literal lexemes
                   already include any L/u/U/u8 prefix and surrounding
                   quotes, so they survive the right-justify intact. */
                rc = append_sym(&sbuf, &scap, &slen,
                                t->lexeme, t->lexeme_len);
                break;

            case TOK_EOF:
                /* Trailing pair, if any. */
                if (wlen > 0 || slen > 0) {
                    rc = emit_pair(wbuf, wlen, sbuf, slen, out);
                    wlen = slen = 0;
                }
                break;

            default:
                break;
        }
    }

    free(wbuf);
    free(sbuf);
    if (stream.tokens) free(stream.tokens);
    ds_arena_destroy(&arena, &ctx);
    return (rc == 0) ? DS_OK : DS_ERR_ALLOC;
}
