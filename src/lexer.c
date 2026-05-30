/*
 * lexer.c — tokenizer for the aligner parser.
 *
 * Lossless: every input byte ends up inside exactly one token. The emitter can
 * reproduce the source bytewise by walking tokens and printing their lexemes.
 *
 * Not handled (and we don't pretend otherwise):
 *  - Trigraphs (??=, ??-, etc.). Nearly extinct; bypass via cpp.
 *  - Digraphs (<:, :>, <%, %>, %:). Recognised as their constituent
 *    punctuators rather than as a single token. Fine for our purposes.
 *  - `\<newline>` line splicing outside string/PP-directive context. Rare.
 *
 * Where the spec is ambiguous about how to split tokens (e.g. maximal munch
 * on punctuators), we follow the C standard.
 */

#include "lexer.h"

#include <stdlib.h>
#include <string.h>

/* --- Token kind name lookup ------------------------------------------------ */

const char *cfmt_tok_kind_name(cfmt_tok_kind_t k) {
    switch (k) {
        case TOK_EOF:           return "EOF";
        case TOK_IDENT:         return "IDENT";
        case TOK_INT_LIT:       return "INT_LIT";
        case TOK_FLOAT_LIT:     return "FLOAT_LIT";
        case TOK_CHAR_LIT:      return "CHAR_LIT";
        case TOK_STRING_LIT:    return "STRING_LIT";
        case TOK_PUNCT:         return "PUNCT";
        case TOK_PP_DIRECTIVE:  return "PP_DIRECTIVE";
        case TOK_COMMENT_LINE:  return "COMMENT_LINE";
        case TOK_COMMENT_BLOCK: return "COMMENT_BLOCK";
        case TOK_WHITESPACE:    return "WHITESPACE";
        case TOK_NEWLINE:       return "NEWLINE";
    }
    return "?";
}

/* --- Predicate helpers ---------------------------------------------------- */

static int is_id_start(int c) {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int is_id_cont(int c) {
    return is_id_start(c) || (c >= '0' && c <= '9');
}
static int is_digit(int c)     { return c >= '0' && c <= '9'; }
static int is_octal(int c)     { return c >= '0' && c <= '7'; }
static int is_hex(int c)       {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static int hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}
static int is_hspace(int c) {
    /* horizontal whitespace, NOT newline */
    return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r';
}

/* --- Lexer state ---------------------------------------------------------- */

typedef struct {
    const char *p;       /* current cursor */
    const char *end;     /* one past last byte */
    unsigned    line;
    unsigned    col;
    int         line_start; /* 1 if only whitespace has been seen since last \n */
    cfmt_tok_stream_t *out;
} lexer_t;

static int lex_peek(const lexer_t *L, size_t off) {
    if (L->p + off >= L->end) return 0;
    return (unsigned char)L->p[off];
}

static void advance(lexer_t *L, size_t n) {
    for (size_t i = 0; i < n && L->p < L->end; i++) {
        if (*L->p == '\n') { L->line++; L->col = 1; }
        else                { L->col++; }
        L->p++;
    }
}

typedef struct { const char *start; unsigned line; unsigned col; } mark_t;
static mark_t take_mark(const lexer_t *L) {
    mark_t m; m.start = L->p; m.line = L->line; m.col = L->col;
    return m;
}

/* Token stream grow / emit. */
static ds_status_t push_tok(lexer_t *L, const cfmt_tok_t *tok) {
    cfmt_tok_stream_t *s = L->out;
    if (s->count >= s->cap) {
        size_t newcap = s->cap ? s->cap * 2 : 256;
        size_t bytes  = newcap * sizeof(cfmt_tok_t);
        cfmt_tok_t *nb = (cfmt_tok_t *)realloc(s->tokens, bytes);
        if (!nb) return DS_ERR_ALLOC;
        s->tokens = nb;
        s->cap    = newcap;
    }
    s->tokens[s->count++] = *tok;
    return DS_OK;
}

static void fill_span(cfmt_tok_t *t, cfmt_tok_kind_t k, const lexer_t *L, mark_t m) {
    t->kind        = k;
    t->lexeme      = m.start;
    t->lexeme_len  = (size_t)(L->p - m.start);
    t->line        = m.line;
    t->col         = m.col;
    memset(&t->u, 0, sizeof(t->u));
}

/* --- Whitespace and newlines --------------------------------------------- */

static ds_status_t lex_whitespace(lexer_t *L) {
    mark_t m = take_mark(L);
    while (L->p < L->end && is_hspace(*L->p)) advance(L, 1);
    cfmt_tok_t t;
    fill_span(&t, TOK_WHITESPACE, L, m);
    return push_tok(L, &t);
}

static ds_status_t lex_newline(lexer_t *L) {
    mark_t m = take_mark(L);
    advance(L, 1);
    cfmt_tok_t t;
    fill_span(&t, TOK_NEWLINE, L, m);
    L->line_start = 1;
    return push_tok(L, &t);
}

/* --- Comments ------------------------------------------------------------- */

static ds_status_t lex_line_comment(lexer_t *L) {
    mark_t m = take_mark(L);
    advance(L, 2);  /* eat // */
    while (L->p < L->end && *L->p != '\n') advance(L, 1);
    cfmt_tok_t t;
    fill_span(&t, TOK_COMMENT_LINE, L, m);
    return push_tok(L, &t);
}

static ds_status_t lex_block_comment(lexer_t *L) {
    mark_t m = take_mark(L);
    advance(L, 2);  /* eat the opening slash-star */
    while (L->p + 1 < L->end && !(L->p[0] == '*' && L->p[1] == '/')) advance(L, 1);
    if (L->p + 1 < L->end) advance(L, 2);  /* eat the closing */
    cfmt_tok_t t;
    fill_span(&t, TOK_COMMENT_BLOCK, L, m);
    return push_tok(L, &t);
}

/* --- Preprocessor directive (entire logical line) ------------------------ */

static ds_status_t lex_pp_directive(lexer_t *L) {
    mark_t m = take_mark(L);
    while (L->p < L->end) {
        char c = *L->p;
        if (c == '\\' && L->p + 1 < L->end && L->p[1] == '\n') {
            advance(L, 2);  /* line continuation: keep going */
            continue;
        }
        if (c == '\n') break;
        advance(L, 1);
    }
    cfmt_tok_t t;
    fill_span(&t, TOK_PP_DIRECTIVE, L, m);
    return push_tok(L, &t);
}

/* --- Identifiers and keywords -------------------------------------------- */

/* Caller has confirmed L->p[0] is id-start, OR is one of L/u/U preceding "/'. */
static ds_status_t lex_ident(lexer_t *L) {
    mark_t m = take_mark(L);
    while (L->p < L->end && is_id_cont(*L->p)) advance(L, 1);
    cfmt_tok_t t;
    fill_span(&t, TOK_IDENT, L, m);
    return push_tok(L, &t);
}

/* --- Numeric literals ---------------------------------------------------- */

/*
 * We don't fully decode the numeric value — the parser doesn't need it. We
 * only need to find the end of the lexeme correctly. We distinguish int vs
 * float by the presence of '.', 'e', 'E', 'p', 'P' (hex float exponent).
 */
static ds_status_t lex_number(lexer_t *L) {
    mark_t m   = take_mark(L);
    int is_float = 0;
    int is_hex_num = 0;

    /* Optional prefix: 0x / 0b / 0o (C2x) / leading 0 (octal). */
    if (L->p[0] == '0' && L->p + 1 < L->end &&
        (L->p[1] == 'x' || L->p[1] == 'X')) {
        is_hex_num = 1;
        advance(L, 2);
    } else if (L->p[0] == '0' && L->p + 1 < L->end &&
               (L->p[1] == 'b' || L->p[1] == 'B')) {
        advance(L, 2);
    }

    /* Body: digits / a-fA-F / ' (digit separator in C23) / single . */
    int seen_dot = 0;
    while (L->p < L->end) {
        char c = *L->p;
        if (c == '\'') { /* C23 digit separator */ advance(L, 1); continue; }
        if (c == '.') {
            if (seen_dot) break;
            seen_dot = 1; is_float = 1; advance(L, 1); continue;
        }
        if (is_hex_num) {
            if (is_hex((unsigned char)c)) { advance(L, 1); continue; }
        } else {
            if (is_digit((unsigned char)c)) { advance(L, 1); continue; }
        }
        break;
    }

    /* Exponent: e/E for decimal, p/P for hex floats. */
    if (L->p < L->end) {
        char c = *L->p;
        int has_exp =
            (!is_hex_num && (c == 'e' || c == 'E')) ||
            ( is_hex_num && (c == 'p' || c == 'P'));
        if (has_exp) {
            is_float = 1;
            advance(L, 1);
            if (L->p < L->end && (*L->p == '+' || *L->p == '-')) advance(L, 1);
            while (L->p < L->end && is_digit((unsigned char)*L->p)) advance(L, 1);
        }
    }

    /* Suffix: u, U, l, L, ll, LL, lu, llu, f, F, etc. — accept up to 4 chars. */
    for (int i = 0; i < 4; i++) {
        if (L->p >= L->end) break;
        char c = *L->p;
        if (c == 'u' || c == 'U' || c == 'l' || c == 'L' ||
            c == 'f' || c == 'F' || c == 'i' || c == 'I' ||
            c == 'j' || c == 'J') {
            advance(L, 1);
        } else break;
    }

    cfmt_tok_t t;
    fill_span(&t, is_float ? TOK_FLOAT_LIT : TOK_INT_LIT, L, m);
    return push_tok(L, &t);
}

/* --- Escape sequence decoding -------------------------------------------- */

/* Decode escape starting at L->p (which points to the char AFTER the backslash).
 * Returns the byte value and advances L->p past the escape. */
static int parse_escape(lexer_t *L) {
    if (L->p >= L->end) return 0;
    int c = (unsigned char)*L->p;

    /* Hex: \xHH... — any number of hex digits per the standard */
    if (c == 'x' || c == 'X') {
        advance(L, 1);
        int val = 0;
        while (L->p < L->end && is_hex((unsigned char)*L->p)) {
            val = (val << 4) | hex_val((unsigned char)*L->p);
            advance(L, 1);
        }
        return val & 0xFF;
    }
    /* Octal: \NNN — up to 3 octal digits */
    if (is_octal(c)) {
        int val = 0;
        for (int i = 0; i < 3 && L->p < L->end && is_octal((unsigned char)*L->p); i++) {
            val = (val * 8) + (*L->p - '0');
            advance(L, 1);
        }
        return val & 0xFF;
    }
    /* Universal char names: \uHHHH or \UHHHHHHHH — return low byte if <256 */
    if (c == 'u' || c == 'U') {
        int width = (c == 'u') ? 4 : 8;
        advance(L, 1);
        int val = 0;
        for (int i = 0; i < width && L->p < L->end && is_hex((unsigned char)*L->p); i++) {
            val = (val << 4) | hex_val((unsigned char)*L->p);
            advance(L, 1);
        }
        return val & 0xFF;
    }

    advance(L, 1);
    switch (c) {
        case 'n':  return 10;
        case 't':  return 9;
        case 'r':  return 13;
        case '0':  return 0;
        case '\\': return 92;
        case '"':  return 34;
        case '\'': return 39;
        case 'a':  return 7;
        case 'b':  return 8;
        case 'f':  return 12;
        case 'v':  return 11;
        case '?':  return 63;
        case 'e':  return 27;   /* GCC extension */
        default:   return c;    /* unknown escape — pass through */
    }
}

/* --- Character literals --------------------------------------------------- */

/* Caller has positioned L->p at the opening quote. `enc` is the prefix (or NONE). */
static ds_status_t lex_char_lit(lexer_t *L, cfmt_enc_t enc, mark_t m) {
    advance(L, 1);  /* eat opening ' */

    unsigned long value = 0;

    /* Multi-character literals like 'AB' are implementation-defined but legal.
       We accumulate bytes into `value` shift-left-and-or, lo-byte-last. */
    while (L->p < L->end && *L->p != '\'') {
        int b;
        if (*L->p == '\\' && L->p + 1 < L->end) {
            advance(L, 1);
            b = parse_escape(L);
        } else {
            b = (unsigned char)*L->p;
            advance(L, 1);
        }
        value = (value << 8) | (unsigned long)(b & 0xFF);
    }
    if (L->p < L->end) advance(L, 1);  /* closing ' */

    cfmt_tok_t t;
    fill_span(&t, TOK_CHAR_LIT, L, m);
    t.u.chr.value = value;
    t.u.chr.enc   = enc;
    return push_tok(L, &t);
}

/* --- String literals ------------------------------------------------------ */

/* Decode a string literal's bytes into an arena-allocated buffer.
 * Caller has positioned L->p at the opening ". `enc` is the prefix. */
static ds_status_t lex_string_lit(lexer_t *L, cfmt_enc_t enc, mark_t m) {
    advance(L, 1);  /* eat opening " */

    /* Two-pass approach would be cleaner, but a single growing buffer in the
       arena works because we know the upper bound is lexeme_len. We'll
       allocate optimistically and trim by tracking actual length. */
    /* Upper bound: distance from current point to first matching " — at most
       the remaining input size. Cheap to over-allocate. */
    size_t cap = 16;
    unsigned char *buf = (unsigned char *)ds_context_alloc(L->out->ctx, cap);
    if (!buf) return DS_ERR_ALLOC;
    size_t len = 0;

    while (L->p < L->end && *L->p != '"') {
        int b;
        if (*L->p == '\\' && L->p + 1 < L->end) {
            if (L->p[1] == '\n') {
                /* Line continuation inside string literal. */
                advance(L, 2);
                continue;
            }
            advance(L, 1);
            b = parse_escape(L);
        } else {
            b = (unsigned char)*L->p;
            advance(L, 1);
        }
        if (len >= cap) {
            size_t newcap = cap * 2;
            unsigned char *nb = (unsigned char *)ds_context_alloc(L->out->ctx, newcap);
            if (!nb) return DS_ERR_ALLOC;
            memcpy(nb, buf, len);
            buf = nb;
            cap = newcap;
            /* Old buf leaks in the arena, which is fine. */
        }
        buf[len++] = (unsigned char)(b & 0xFF);
    }
    if (L->p < L->end) advance(L, 1);  /* closing " */

    cfmt_tok_t t;
    fill_span(&t, TOK_STRING_LIT, L, m);
    t.u.str.bytes      = buf;
    t.u.str.byte_count = len;
    t.u.str.enc        = enc;
    return push_tok(L, &t);
}

/* --- Punctuators ---------------------------------------------------------- */

/*
 * Maximal munch over the C punctuator set. We try 4-char, then 3-char, then
 * 2-char, then 1-char.
 */
static const char *const PUNCT_4[] = { "%:%:", NULL };
static const char *const PUNCT_3[] = {
    "<<=", ">>=", "...", "->*", "::*", NULL
};
static const char *const PUNCT_2[] = {
    "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "++", "--",
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "->", "::",
    "##", NULL
};

static int try_match(const char *p, const char *end, const char *needle, size_t n) {
    if ((size_t)(end - p) < n) return 0;
    return memcmp(p, needle, n) == 0;
}

static ds_status_t lex_punct(lexer_t *L) {
    mark_t m = take_mark(L);
    size_t avail = (size_t)(L->end - L->p);
    size_t take  = 0;
    if (avail >= 4) {
        for (int i = 0; PUNCT_4[i]; i++) {
            if (memcmp(L->p, PUNCT_4[i], 4) == 0) { take = 4; break; }
        }
    }
    if (!take && avail >= 3) {
        for (int i = 0; PUNCT_3[i]; i++) {
            if (memcmp(L->p, PUNCT_3[i], 3) == 0) { take = 3; break; }
        }
    }
    if (!take && avail >= 2) {
        for (int i = 0; PUNCT_2[i]; i++) {
            if (memcmp(L->p, PUNCT_2[i], 2) == 0) { take = 2; break; }
        }
    }
    if (!take) take = 1;
    advance(L, take);
    (void)try_match;  /* unused helper kept for future use */

    cfmt_tok_t t;
    fill_span(&t, TOK_PUNCT, L, m);
    return push_tok(L, &t);
}

/* --- Top-level dispatch --------------------------------------------------- */

/* Look ahead from L->p to see if we have a wide-string/char prefix followed
   by " or '. If so, set *enc and *advance_n. Otherwise *advance_n = 0. */
static void detect_enc_prefix(const lexer_t *L, cfmt_enc_t *enc, size_t *advance_n) {
    *enc = CFMT_ENC_NONE;
    *advance_n = 0;
    if (L->p >= L->end) return;
    int c0 = (unsigned char)L->p[0];
    int c1 = lex_peek(L, 1);
    int c2 = lex_peek(L, 2);

    if (c0 == 'L' && (c1 == '"' || c1 == '\'')) {
        *enc = CFMT_ENC_L; *advance_n = 1;
    } else if (c0 == 'U' && (c1 == '"' || c1 == '\'')) {
        *enc = CFMT_ENC_U; *advance_n = 1;
    } else if (c0 == 'u' && c1 == '8' && c2 == '"') {
        *enc = CFMT_ENC_u8; *advance_n = 2;
    } else if (c0 == 'u' && (c1 == '"' || c1 == '\'')) {
        *enc = CFMT_ENC_u; *advance_n = 1;
    }
}

ds_status_t cfmt_lex(const char        *source,
                     size_t             source_len,
                     ds_context_t      *ctx,
                     ds_arena_t        *arena,
                     cfmt_tok_stream_t *out) {
    lexer_t L;
    L.p          = source;
    L.end        = source + source_len;
    L.line       = 1;
    L.col        = 1;
    L.line_start = 1;
    L.out        = out;
    L.out->ctx   = ctx;
    L.out->arena = arena;
    L.out->source = source;
    L.out->source_len = source_len;
    L.out->tokens = NULL;
    L.out->count  = 0;
    L.out->cap    = 0;
    (void)arena;

    while (L.p < L.end) {
        int c = (unsigned char)*L.p;
        int line_start_was = L.line_start;
        ds_status_t st = DS_OK;

        if (is_hspace(c)) {
            st = lex_whitespace(&L);
            /* line_start preserved */
        } else if (c == '\n') {
            st = lex_newline(&L);
        } else if (c == '/' && lex_peek(&L, 1) == '/') {
            st = lex_line_comment(&L);
            L.line_start = 0;
        } else if (c == '/' && lex_peek(&L, 1) == '*') {
            st = lex_block_comment(&L);
            L.line_start = 0;
        } else if (c == '#' && line_start_was) {
            st = lex_pp_directive(&L);
            L.line_start = 0;
        } else if (is_digit(c) || (c == '.' && is_digit(lex_peek(&L, 1)))) {
            st = lex_number(&L);
            L.line_start = 0;
        } else {
            cfmt_enc_t enc;
            size_t pref_n;
            detect_enc_prefix(&L, &enc, &pref_n);
            if (pref_n > 0) {
                mark_t m = take_mark(&L);
                advance(&L, pref_n);
                int q = (unsigned char)*L.p;
                if (q == '"')       st = lex_string_lit(&L, enc, m);
                else if (q == '\'') st = lex_char_lit(&L, enc, m);
                else                st = DS_ERR_INTERNAL;  /* unreachable */
                L.line_start = 0;
            } else if (is_id_start(c)) {
                st = lex_ident(&L);
                L.line_start = 0;
            } else if (c == '"') {
                mark_t m = take_mark(&L);
                st = lex_string_lit(&L, CFMT_ENC_NONE, m);
                L.line_start = 0;
            } else if (c == '\'') {
                mark_t m = take_mark(&L);
                st = lex_char_lit(&L, CFMT_ENC_NONE, m);
                L.line_start = 0;
            } else {
                st = lex_punct(&L);
                L.line_start = 0;
            }
        }

        if (st != DS_OK) return st;
    }

    /* Emit EOF sentinel. */
    cfmt_tok_t eof;
    memset(&eof, 0, sizeof(eof));
    eof.kind   = TOK_EOF;
    eof.lexeme = L.p;
    eof.line   = L.line;
    eof.col    = L.col;
    return push_tok(&L, &eof);
}

/* --- Convenience checks --------------------------------------------------- */

int cfmt_tok_is_punct(const cfmt_tok_t *t, const char *punct) {
    if (!t || t->kind != TOK_PUNCT) return 0;
    size_t n = strlen(punct);
    return t->lexeme_len == n && memcmp(t->lexeme, punct, n) == 0;
}

int cfmt_tok_is_ident(const cfmt_tok_t *t, const char *ident) {
    if (!t || t->kind != TOK_IDENT) return 0;
    size_t n = strlen(ident);
    return t->lexeme_len == n && memcmp(t->lexeme, ident, n) == 0;
}
