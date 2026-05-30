/*
 * lexer.h — tokenize C source.
 *
 * The lexer is lossless: every byte of input is accounted for by exactly one
 * token, including whitespace, newlines, and comments. This lets the emitter
 * reproduce the source bytewise when no transformations apply.
 *
 * Preprocessor directives are a single token (TOK_PP_DIRECTIVE) covering the
 * entire logical line, including any `\<newline>` continuations. The parser
 * does not look inside them; passes that need to (e.g., to recognise
 * #include) examine the lexeme.
 *
 * Adjacent string literals are NOT merged here — each "..." segment is its
 * own TOK_STRING_LIT. The parser/emit phase decides what to do with runs of
 * adjacent string tokens.
 */
#ifndef CFMT_LEXER_H
#define CFMT_LEXER_H

#include "ds.h"

#include <stddef.h>

typedef enum {
    TOK_EOF = 0,
    TOK_IDENT,          /* foo, _bar, sizeof, if, while — keywords undistinguished */
    TOK_INT_LIT,        /* 123, 0x1A, 0777, 0b1010, suffix included */
    TOK_FLOAT_LIT,      /* 1.0, 1e10, .5f, 0x1.8p+1 */
    TOK_CHAR_LIT,       /* 'X', '\n', L'A', u'A', U'A' */
    TOK_STRING_LIT,     /* "...", L"...", u"...", U"...", u8"..." */
    TOK_PUNCT,          /* operators and punctuators: ;, (, ), {, }, +, ==, ->, ... */
    TOK_PP_DIRECTIVE,   /* one full logical # line incl. continuations */
    TOK_COMMENT_LINE,   /* //... up to but not including the newline */
    TOK_COMMENT_BLOCK,  /* slash-star ... star-slash, may span lines */
    TOK_WHITESPACE,     /* runs of spaces/tabs/etc., NOT newlines */
    TOK_NEWLINE         /* a single \n */
} cfmt_tok_kind_t;

/* String- and char-literal "encoding prefix". */
typedef enum {
    CFMT_ENC_NONE = 0,  /* plain "..." */
    CFMT_ENC_L,         /* L"..." */
    CFMT_ENC_u,         /* u"..." */
    CFMT_ENC_U,         /* U"..." */
    CFMT_ENC_u8         /* u8"..." */
} cfmt_enc_t;

typedef struct cfmt_tok {
    cfmt_tok_kind_t kind;

    /* Verbatim source slice for this token. Points into the source buffer
       given to cfmt_lex(); do not free. */
    const char *lexeme;
    size_t      lexeme_len;

    /* 1-based, for diagnostics. */
    unsigned line;
    unsigned col;

    /* Decoded form for literals. Arena-allocated. */
    union {
        struct {
            unsigned char *bytes;
            size_t         byte_count;
            cfmt_enc_t     enc;
        } str;
        struct {
            unsigned long value;  /* enough for hex/octal large values */
            cfmt_enc_t     enc;
        } chr;
    } u;
} cfmt_tok_t;

/* Token stream — owned by the arena and the caller. */
typedef struct cfmt_tok_stream {
    cfmt_tok_t *tokens;
    size_t      count;
    size_t      cap;

    /* Owned by caller; allocations charged here. */
    ds_arena_t   *arena;
    ds_context_t *ctx;

    /* For pointer arithmetic in lexeme spans. */
    const char *source;
    size_t      source_len;
} cfmt_tok_stream_t;

/*
 * Tokenize `source` into `out`.
 * `out` should be zero-initialized; arena and ctx must be set.
 * Returns DS_OK on success, DS_ERR_* on allocation/IO failure.
 */
ds_status_t cfmt_lex(const char       *source,
                     size_t            source_len,
                     ds_context_t     *ctx,
                     ds_arena_t       *arena,
                     cfmt_tok_stream_t *out);

/* Human-readable name for a token kind. Useful in tests and diagnostics. */
const char *cfmt_tok_kind_name(cfmt_tok_kind_t k);

/* True if `t` is a punctuator with lexeme equal to the given C string. */
int cfmt_tok_is_punct(const cfmt_tok_t *t, const char *punct);

/* True if `t` is an identifier with lexeme equal to the given C string. */
int cfmt_tok_is_ident(const cfmt_tok_t *t, const char *ident);

#endif /* CFMT_LEXER_H */
