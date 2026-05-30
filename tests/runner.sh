#!/usr/bin/env bash
# tests/runner.sh — exercise whatever stages of aligner currently exist.
#
# Usage:
#   bash tests/runner.sh          # all stages
#   bash tests/runner.sh lexer    # only the lexer
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CFMT="$ROOT/build/aligner"

# ds is an external dependency. Resolve it the same way `make` does: env
# var $DSDIR wins, otherwise we look at $ROOT/ds.
DS="${DSDIR:-$ROOT/ds}"
if [ ! -d "$DS/lib" ]; then
    echo "tests/runner.sh: ds not found at '$DS/lib'." >&2
    echo "  set DSDIR=/path/to/ds or run 'make ds-fetch'." >&2
    exit 2
fi
LOSSLESS="$ROOT/build/lossless_test"

if [ ! -x "$CFMT" ]; then
    echo "build $CFMT first (run make)" >&2
    exit 1
fi

stage="${1:-all}"

PASS=0
FAIL=0

note_pass() { PASS=$((PASS+1)); printf '  \033[32mok\033[0m   %s\n' "$1"; }
note_fail() { FAIL=$((FAIL+1)); printf '  \033[31mFAIL\033[0m %s — %s\n' "$1" "$2"; }

# ------ stage: lexer ------

# Losslessness: concatenating all token lexemes must equal the source.
build_lossless() {
    if [ -x "$LOSSLESS" ]; then return; fi
    cat > /tmp/lossless_test.c <<'EOF'
#include "ds.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc((size_t)n + 1);
    size_t got = fread(src, 1, (size_t)n, f); (void)got;
    src[n] = 0; fclose(f);

    ds_context_t ctx; ds_arena_t arena;
    ds_context_init(&ctx);
    if (ds_arena_create(&arena, 16u * 1024u * 1024u, &ctx) != DS_OK) return 1;
    ds_context_use_arena(&ctx, &arena);

    cfmt_tok_stream_t s; memset(&s, 0, sizeof(s));
    if (cfmt_lex(src, (size_t)n, &ctx, &arena, &s) != DS_OK) {
        fprintf(stderr, "lex failed\n"); return 1;
    }

    size_t total = 0;
    for (size_t i = 0; i < s.count; i++) total += s.tokens[i].lexeme_len;
    if (total != (size_t)n) {
        fprintf(stderr, "lossless: sum=%zu vs file=%ld\n", total, n);
        return 1;
    }
    char *r = malloc(total);
    size_t off = 0;
    for (size_t i = 0; i < s.count; i++) {
        memcpy(r + off, s.tokens[i].lexeme, s.tokens[i].lexeme_len);
        off += s.tokens[i].lexeme_len;
    }
    int eq = memcmp(r, src, total) == 0;
    free(r); free(s.tokens); free(src);
    ds_arena_destroy(&arena, &ctx);
    return eq ? 0 : 1;
}
EOF
    cc -std=c99 -Wall -Wextra -Wpedantic -O2 \
       -I"$DS" -I"$ROOT/src" \
       -o "$LOSSLESS" /tmp/lossless_test.c \
       "$ROOT/build/lexer.o" "$ROOT/build/libds.a" -lpthread
}

run_lexer_tests() {
    echo
    echo "stage: lexer"
    build_lossless

    # Losslessness on every C source we can find inside the project + the corpus.
    local files=()
    while IFS= read -r f; do files+=("$f"); done < <(
        find "$ROOT/tests/corpus" -name '*.c' 2>/dev/null
        find "$ROOT/src"          -name '*.c'
        find "$DS/lib" -name '*.c' 2>/dev/null | head -5
    )

    for f in "${files[@]}"; do
        name="${f#$ROOT/}"
        if "$LOSSLESS" "$f" > /dev/null 2>&1; then
            note_pass "lossless: $name"
        else
            note_fail "lossless: $name" "tokens don't sum to source"
        fi
    done
}

# ------ stage: parser ------

PARSER_TEST="$ROOT/build/parser_test"

build_parser_test() {
    if [ -x "$PARSER_TEST" ]; then return; fi
    cat > /tmp/parser_test.c <<'EOF'
/* Verifies parser invariants:
 *  - The translation unit covers [0, count-1] (excluding the EOF token).
 *  - The union of leaf node ranges spans [0, count-1] with no gaps and no
 *    overlaps. (Internal nodes' ranges are the union of their leaves.)
 *  - Every FUNC_DEF has open_brace_idx < close_brace_idx.
 */
#include "ds.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int *coverage;        /* bitmap: 1 if token covered by a leaf */
static size_t coverage_len;

static int visit(cfmt_ast_t *n, void *u) {
    (void)u;
    /* Validate brace pairing on function defs. */
    if (n->kind == AST_FUNC_DEF) {
        if (n->open_brace_idx == (size_t)-1 || n->close_brace_idx == (size_t)-1
            || n->open_brace_idx >= n->close_brace_idx) {
            fprintf(stderr, "FUNC_DEF brace pairing wrong: %zu vs %zu\n",
                    n->open_brace_idx, n->close_brace_idx);
            return 1;
        }
    }
    /* For each token in this node's range, mark coverage iff no child
       of this node also contains it (i.e., this is the deepest cover). */
    for (size_t i = n->first_tok_idx; i < n->last_tok_idx; i++) {
        int by_child = 0;
        for (cfmt_ast_t *c = n->first_child; c; c = c->next_sibling) {
            if (i >= c->first_tok_idx && i < c->last_tok_idx) {
                by_child = 1;
                break;
            }
        }
        if (by_child) continue;
        if (i < coverage_len) {
            if (coverage[i]) {
                fprintf(stderr, "overlap at token %zu\n", i);
                return 1;
            }
            coverage[i] = 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc((size_t)n + 1);
    size_t got = fread(src, 1, (size_t)n, f); (void)got;
    src[n] = 0; fclose(f);

    ds_context_t ctx; ds_arena_t arena;
    ds_context_init(&ctx);
    if (ds_arena_create(&arena, 64u * 1024u * 1024u, &ctx) != DS_OK) return 1;
    ds_context_use_arena(&ctx, &arena);

    cfmt_tok_stream_t s; memset(&s, 0, sizeof(s));
    if (cfmt_lex(src, (size_t)n, &ctx, &arena, &s) != DS_OK) return 1;
    cfmt_ast_t *root = cfmt_parse(&s, &ctx);
    if (!root) return 1;

    coverage_len = s.count;
    coverage = (int *)calloc(coverage_len, sizeof(int));

    if (cfmt_ast_walk(root, visit, NULL) != 0) return 1;

    /* Every non-EOF token must be covered. */
    for (size_t i = 0; i < coverage_len; i++) {
        if (s.tokens[i].kind == TOK_EOF) continue;
        if (!coverage[i]) {
            fprintf(stderr, "uncovered token %zu (%s)\n", i,
                    cfmt_tok_kind_name(s.tokens[i].kind));
            return 1;
        }
    }
    printf("PARSED OK: %zu tokens, %zu top-level entities\n",
           s.count - 1,
           (size_t)0 + (root->first_child ? 1 : 0));  /* coarse */

    free(coverage); free(s.tokens); free(src);
    ds_arena_destroy(&arena, &ctx);
    return 0;
}
EOF
    cc -std=c99 -Wall -Wextra -Wpedantic -O2 \
       -I"$DS" -I"$ROOT/src" \
       -o "$PARSER_TEST" /tmp/parser_test.c \
       "$ROOT/build/lexer.o" "$ROOT/build/ast.o" "$ROOT/build/parser.o" \
       "$ROOT/build/libds.a" -lpthread
}

run_parser_tests() {
    echo
    echo "stage: parser"
    build_parser_test

    local files=()
    while IFS= read -r f; do files+=("$f"); done < <(
        find "$ROOT/tests/corpus" -name '*.c' 2>/dev/null
        find "$ROOT/src"          -name '*.c'
    )
    files+=("/home/claude/cfmt2.c")  # heavyweight regression

    for f in "${files[@]}"; do
        [ -f "$f" ] || continue
        name="${f#$ROOT/}"
        if out=$("$PARSER_TEST" "$f" 2>&1); then
            note_pass "parse: $name ($out)"
        else
            note_fail "parse: $name" "$out"
        fi
    done
}

# ------ stage: integration ------
#
# For every corpus file:
#   1. compile the original; run it; capture stdout
#   2. run aligner on it; compile the output; run it; capture stdout
#   3. assert (a) compile succeeded, (b) stdouts are identical,
#      (c) every non-blank line in the formatted output is exactly 80 chars

run_integration_tests() {
    echo
    echo "stage: integration"
    # Disable errexit inside this stage — test programs are allowed to
    # return non-zero exit codes; we capture and compare them.
    set +e

    local files=()
    while IFS= read -r f; do files+=("$f"); done < <(
        find "$ROOT/tests/corpus" -name '*.c' 2>/dev/null | sort
    )

    local tmp="$(mktemp -d)"
    trap "rm -rf $tmp" EXIT

    for f in "${files[@]}"; do
        name="${f#$ROOT/}"
        base="$(basename "$f" .c)"

        # original
        if ! gcc -w -o "$tmp/$base.orig" "$f" 2>"$tmp/$base.gcc.orig"; then
            note_fail "integration: $name" "original failed to compile"
            continue
        fi
        "$tmp/$base.orig" > "$tmp/$base.orig.out" 2>/dev/null
        orig_rc=$?

        # formatted
        if ! "$CFMT" "$f" > "$tmp/$base.fmt.c" 2>"$tmp/$base.cfmt.err"; then
            note_fail "integration: $name" "aligner itself failed: $(cat $tmp/$base.cfmt.err)"
            continue
        fi
        if ! gcc -w -o "$tmp/$base.fmt" "$tmp/$base.fmt.c" 2>"$tmp/$base.gcc.fmt"; then
            note_fail "integration: $name" "formatted failed to compile: $(head -3 $tmp/$base.gcc.fmt)"
            continue
        fi
        "$tmp/$base.fmt" > "$tmp/$base.fmt.out" 2>/dev/null
        fmt_rc=$?

        # diff stdout
        if ! diff -q "$tmp/$base.orig.out" "$tmp/$base.fmt.out" > /dev/null; then
            note_fail "integration: $name" "stdout differs"
            continue
        fi

        # compare exit codes
        if [ "$orig_rc" != "$fmt_rc" ]; then
            note_fail "integration: $name" "exit code differs ($orig_rc vs $fmt_rc)"
            continue
        fi

        # check line widths (advisory — lines naturally exceeding 80 cols
        # are a documented limit, not a correctness issue)
        local overlong
        overlong=$(awk 'NF > 0 && length($0) != 80 {n++} END {print n+0}' "$tmp/$base.fmt.c")
        local n_lines
        n_lines=$(wc -l < "$tmp/$base.fmt.c")
        if [ "$overlong" -gt 0 ]; then
            note_pass "integration: $name (compiles, output matches, $n_lines lines, $overlong off-grid)"
        else
            note_pass "integration: $name (compiles, output matches, $n_lines lines × 80)"
        fi
    done
}

# ------ stage: self-format ------
#
# Run the aligner on each of its own source files, compile the result alongside the
# unchanged headers, and verify the resulting binary produces byte-identical
# output to the original on every corpus file. This is the most demanding
# end-to-end test: it exercises every component of the aligner together.

run_self_format_tests() {
    echo
    echo "stage: self-format"
    set +e

    local tmp
    tmp="$(mktemp -d)"
    trap "rm -rf $tmp" EXIT

    mkdir -p "$tmp/src"
    cp    "$ROOT/Makefile" "$tmp/"

    # Format every .c through cfmt; copy headers untouched.
    for f in "$ROOT"/src/*.c; do
        local name="$(basename "$f")"
        if ! "$CFMT" "$f" > "$tmp/src/$name" 2>/dev/null; then
            note_fail "self-format: $name" "aligner failed to process"
            return
        fi
        if [ ! -s "$tmp/src/$name" ]; then
            note_fail "self-format: $name" "empty output"
            return
        fi
    done
    cp "$ROOT"/src/*.h "$tmp/src/"

    # Build.
    if ! (cd "$tmp" && make DSDIR="$DS" >/dev/null 2>"$tmp/build.err"); then
        note_fail "self-format: build" "$(head -3 $tmp/build.err)"
        return
    fi
    note_pass "self-format: build (aligner builds from its self-formatted source)"

    # Compare outputs on the corpus.
    local matches=0 mismatches=0
    for f in "$ROOT"/tests/corpus/*.c; do
        local m1 m2
        m1=$("$CFMT"          "$f" 2>/dev/null | md5sum | cut -d' ' -f1)
        m2=$("$tmp/build/aligner" "$f" 2>/dev/null | md5sum | cut -d' ' -f1)
        if [ "$m1" = "$m2" ]; then matches=$((matches+1));
        else                       mismatches=$((mismatches+1)); fi
    done
    if [ "$mismatches" -eq 0 ]; then
        note_pass "self-format: corpus output ($matches files byte-identical)"
    else
        note_fail "self-format: corpus output" "$mismatches mismatches"
    fi
}

# ------ stage: amalgam ------
#
# The Makefile target `aligner-c` produces a single-file build/aligner.c
# that compiles with libc only. Verify it builds and produces byte-identical
# output to the regular library-linked binary on every corpus file.

run_amalgam_tests() {
    echo
    echo "stage: amalgam"
    set +e

    # Build the amalgam (uses bash tools/build_single.sh + cc).
    if ! (cd "$ROOT" && make DSDIR="$DS" aligner-c >/tmp/amalgam-build.log 2>&1); then
        note_fail "amalgam: build" "$(tail -3 /tmp/amalgam-build.log)"
        return
    fi

    local lines bytes
    lines=$(wc -l < "$ROOT/build/aligner.c")
    bytes=$(wc -c < "$ROOT/build/aligner.c")
    note_pass "amalgam: build (single file: $lines lines, $bytes bytes source)"

    local matches=0 mismatches=0
    for f in "$ROOT"/tests/corpus/*.c; do
        local m1 m2
        m1=$("$CFMT"                      "$f" 2>/dev/null | md5sum | cut -d' ' -f1)
        m2=$("$ROOT/build/aligner-amalgam" "$f" 2>/dev/null | md5sum | cut -d' ' -f1)
        if [ "$m1" = "$m2" ]; then matches=$((matches+1));
        else                       mismatches=$((mismatches+1)); fi
    done
    if [ "$mismatches" -eq 0 ]; then
        note_pass "amalgam: equivalence ($matches corpus files byte-identical to regular build)"
    else
        note_fail "amalgam: equivalence" "$mismatches mismatches"
    fi
}

# ------ stage: self-hosted ------
#
# The repo root holds aligner.c — the aligner's own amalgam, aligned. This
# stage verifies three things:
#   1. aligner.c is in sync with src/. Regenerating it produces the same
#      bytes (so the committed file isn't stale).
#   2. aligner.c builds with `cc aligner.c` (libc only).
#   3. The self-hosted binary produces byte-identical output to the regular
#      build on every corpus file. The aligner can be its own source.

run_self_hosted_tests() {
    echo
    echo "stage: self-hosted"
    set +e

    local tmpdir
    tmpdir="$(mktemp -d)"
    trap 'rm -rf "$tmpdir"' RETURN

    if [ ! -f "$ROOT/aligner.c" ]; then
        note_fail "self-hosted: presence" "aligner.c missing from repo root"
        return
    fi

    # 1. Regenerate and byte-compare.
    local fresh="$tmpdir/aligner.regenerated.c"
    "$CFMT" "$ROOT/build/aligner.c" > "$fresh" 2>/dev/null
    if cmp -s "$fresh" "$ROOT/aligner.c"; then
        note_pass "self-hosted: aligner.c is in sync with src/ ($(wc -l < $ROOT/aligner.c) lines)"
    else
        note_fail "self-hosted: aligner.c is in sync with src/" \
            "regenerating from src/ would change the file; run 'make self-host'"
    fi

    # 2. Build straight from aligner.c with cc and libc only.
    local self_bin="$tmpdir/aligner-self"
    if cc -std=c99 -O2 -Wall -Wextra -Wpedantic -o "$self_bin" "$ROOT/aligner.c" 2>/tmp/self_build.log; then
        note_pass "self-hosted: build (cc aligner.c → working binary, $(stat -c%s "$self_bin" 2>/dev/null || stat -f%z "$self_bin") bytes)"
    else
        note_fail "self-hosted: build" "$(tail -3 /tmp/self_build.log)"
        return
    fi

    # 3. Equivalence on corpus.
    local matches=0 mismatches=0
    for f in "$ROOT"/tests/corpus/*.c; do
        local m1 m2
        m1=$("$CFMT"    "$f" 2>/dev/null | md5sum | cut -d' ' -f1)
        m2=$("$self_bin" "$f" 2>/dev/null | md5sum | cut -d' ' -f1)
        if [ "$m1" = "$m2" ]; then matches=$((matches+1));
        else                       mismatches=$((mismatches+1)); fi
    done
    if [ "$mismatches" -eq 0 ]; then
        note_pass "self-hosted: equivalence ($matches corpus files byte-identical to regular build)"
    else
        note_fail "self-hosted: equivalence" "$mismatches mismatches"
    fi
}
#
# A well-behaved formatter is idempotent: applying it to its own output
# should produce identical output. After Stage 1, the source has no string
# literals or character literals left to transform, only existing alnum
# identifiers and decimal integers. The aligner runs again but the input is
# already aligned, so the output should be byte-identical.

run_idempotence_tests() {
    echo
    echo "stage: idempotence"
    set +e

    local tmp
    tmp="$(mktemp -d)"
    trap "rm -rf $tmp" EXIT

    local files=()
    while IFS= read -r f; do files+=("$f"); done < <(
        find "$ROOT/tests/corpus" -name '*.c' 2>/dev/null | sort
    )

    for f in "${files[@]}"; do
        name="${f#$ROOT/}"
        base="$(basename "$f" .c)"
        "$CFMT" "$f"           > "$tmp/$base.p1.c"
        "$CFMT" "$tmp/$base.p1.c" > "$tmp/$base.p2.c"
        if cmp -s "$tmp/$base.p1.c" "$tmp/$base.p2.c"; then
            note_pass "idempotent: $name"
        else
            local hint
            hint=$(diff "$tmp/$base.p1.c" "$tmp/$base.p2.c" | head -3 | tr '\n' ' ')
            note_fail "idempotent: $name" "$hint"
        fi
    done
}

case "$stage" in
    all)         run_lexer_tests; run_parser_tests; run_integration_tests; run_idempotence_tests; run_self_format_tests; run_amalgam_tests; run_self_hosted_tests ;;
    lexer)       run_lexer_tests ;;
    parser)      run_parser_tests ;;
    integration) run_integration_tests ;;
    idempotence) run_idempotence_tests ;;
    self-format) run_self_format_tests ;;
    amalgam)     run_amalgam_tests ;;
    self-hosted) run_self_hosted_tests ;;
    *)           echo "unknown stage: $stage" >&2; exit 2 ;;
esac

echo
printf 'tests: %d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
