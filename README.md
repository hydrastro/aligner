# aligner

A C source formatter that aligns every line of code so alphanumeric tokens
hug the left margin and symbols hug column 80. Because string literals,
character literals, and comments can't survive that aesthetic intact, the
program also:

- replaces every string literal with a byte-by-byte declaration whose
  variable name is the FNV-1a hash of the content (`s2cdd8587`, `scfbe045a`,
  …)
- replaces every character literal with its numeric value (`'A'` → `65`)
- strips every comment (the source has none after; their visual weight
  would jar against alignment)
- deduplicates literals within each scope (same content → one declaration)
- concatenates adjacent string literals (`"foo" "bar"` is one literal)
- right-justifies `#include` paths to col 80 (the iconic look)
- leaves wide strings (`L"…"`, `u"…"`, `U"…"`, `u8"…"`) intact, treated as
  atomic blocks on the right side
- glues `(args)` to the name of function-like macros so alignment padding
  doesn't silently change `#define FOO(x)` into the object-like form

The output compiles and runs identically to the input.

## Self-hosted

`aligner.c` at the repo root **is** this program: the aligner's full source,
run through itself. 8.8k lines of aligned C, 670 KB, libc-only:

```sh
cc -std=c99 -O2 -o aligner aligner.c
./aligner some-file.c > formatted.c
```

That's the whole build. No Makefile required, no headers, no dependencies.
The source is its own demonstration. `aligner aligner.c == aligner.c`
(byte-identical) — the file is a fixed point. The test suite verifies that
the self-hosted binary produces output byte-identical to the regular build
on every corpus file.

```
$ ./build/aligner tests/corpus/01_hello.c | head -3
#include                                                               <stdio.h>
int main                                                                (void) {
static const char scfbe045a                                               [15]={
```

## Build

### With Nix

```sh
nix build
./result/bin/aligner some-file.c > formatted.c
nix develop           # gcc, make, gdb, valgrind, clangd, bear
```

### With make

```sh
make
./build/aligner some-file.c
make                  # build ./build/aligner (modular, from src/)
make test             # full test suite (7 stages, 67 tests)
make aligner-c        # generate build/aligner.c (single-file amalgam, libc only)
make self-host        # regenerate the canonical aligned aligner.c at repo root
make from-aligner-c   # build the binary straight from aligner.c at repo root
make clean            # remove build artifacts
make distclean        # remove build/, editor backups, swap files, *.o etc.
make repo-cleanup     # distclean + remove old experiment files at root
```

## Architecture

```
source bytes
    ↓
 lexer        → token stream  (lossless: every byte covered)
    ↓
 parser       → AST           (coarse: TU / PP / DECL / FUNC_DEF / BLOCK)
    ↓
 destring     → actions       (per-token replacement plan + per-scope
                               literal tables)
    ↓
 emitter      → unaligned text (applies actions; inserts declarations at
                                file scope and at each function-body open)
    ↓
 aligner      → final text     (brace-wrap pass; then 80-col right-justify
                                of every line)
```

The point of building an AST is that transformations become principled.
`char arr[] = "..."` becomes obvious from the parent node's shape.
Function-scope vs file-scope literals are an attribute of which AST node
owns them. K&R declarators, adjacent strings, struct initializers, arrays
of `char *` literals — each is a node-shape or token-window problem rather
than a tokenizer hack.

We do not run the preprocessor. Files with macro-heavy logic should be
piped through `gcc -E -P` first.

We do not type-check or resolve names. The AST is structural only. For
ambiguous cases (e.g. a struct with both `char[]` and `char *` fields), see
`LIMITS.md`.

## Tests

```
make test         # all 5 stages
```

| Stage | What it checks |
|-------|---|
| lexer        | Concatenating all token lexemes equals the original source (lossless) |
| parser       | Every non-EOF token is owned by exactly one deepest AST node; brace pairings are valid |
| integration  | format → compile → run → diff stdout against original |
| idempotence  | `aligner(aligner(x))` is byte-identical to `aligner(x)` |
| self-format  | Build aligner from its own self-formatted source; verify byte-identical output |
| amalgam      | Build the single-file `build/aligner.c`; verify byte-identical output to the regular build |
| self-hosted  | `aligner.c` at repo root is in sync with src/; builds with libc only; produces byte-identical output |

Current status: **67 of 67 passing**, including 10 integration tests, 10
idempotence tests, the self-format check, the amalgam check, and the full
self-hosted check.

## Layout

```
aligner.c              the canonical aligned source (8.8k lines, libc only)
flake.nix              Nix build + dev shell
Makefile               make / make test / make self-host / make repo-cleanup
src/                   modular development source
  lexer.h/.c           tokenizer
  ast.h/.c             AST node types
  parser.h/.c          token stream → AST
  pass.h               pass framework
  pass_destring.c      destringer / dechar pass
  emit.h/.c            AST + actions → unaligned text
  align.h/.c           token-based aligner
  main.c               CLI
tests/
  corpus/              input programs
  runner.sh            test driver
tools/
  shim.c               minimal ds replacement for the amalgam build
  build_single.sh      generates build/aligner.c from src + shim
  repo-cleanup.sh      wipe old experiments and other workspace debris
ds/                    data-structures library this project depends on
```

## CLI

```
aligner FILE              full pipeline → stdout (default)
aligner --lex FILE        dump tokens
aligner --ast FILE        dump AST
aligner --destring FILE   show planned literal lift decisions
aligner --emit FILE       emit unaligned post-destring text (skip aligner)
```

## What works

See `LIMITS.md` for the full accounting. Highlights:

- Arbitrary input size (no fixed buffers)
- All C escape forms incl. `\uHHHH`, `\UHHHHHHHH`, `\xHH`, octal, simple
- All character prefixes (`L`, `u`, `U`, `u8`)
- C23 digit separators
- K&R-style function definitions
- Multi-line string literals (`\<newline>` continuation)
- Adjacent string concatenation
- Per-function and file-scope dedup
- Static-storage lifetime for lifted literals (no dangling pointers)
- Array-init vs pointer-init context detection via declarator inspection
- Self-formatting; idempotence
- 1300+-line production files (`hash_table.c`, `allocators.c`, …) format
  cleanly and recompile

## What doesn't

See `LIMITS.md`. The biggest still-open items: running `cpp` first,
handling mixed-type aggregate inits where some fields are `char[]` and
others `char *`, and teaching aligner about printf-family functions so
format-string warnings survive.
