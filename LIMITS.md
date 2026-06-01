# Limits of aligner — honest accounting

Test status: **67 of 67 passing** (lexer losslessness, parser coverage,
integration with diff-against-original, idempotence, self-format, amalgam,
self-hosted). The aligner formats its own source, the result compiles
and produces byte-identical output to the original on every corpus file,
and the single-file `aligner.c` checked into the root is in sync with
`src/` via `make self-host`.

This document is the upstream catalog of what the project does, what it
chooses not to do, and what is genuinely difficult. It is updated by hand
whenever a limit moves between tiers.

## Solved

These were limits in earlier versions and now work.

- **Arbitrary input sizes.** No fixed buffers; tokens, AST nodes, literal
  tables grow dynamically (arena + `realloc`).
- **Universal character names** (`\uHHHH`, `\UHHHHHHHH`) decoded inline.
- **K&R-style function definitions.** Verified end-to-end.
- **Multi-line string literals** via `\<newline>` continuation, both within
  a single literal and as adjacent-string concatenation.
- **Real C tokenizer.** Hex floats (`0x1.8p+2`), all integer suffixes
  (`ULL`, `LLU`, …), C23 digit separators (`0'001'234`), all character
  prefixes (`L'X'`, `u'X'`, `U'X'`, `u8"…"`), all escape forms.
- **Proper function-body detection.** The parser identifies each
  `AST_FUNC_DEF` explicitly with bracketed body bounds.
- **Adjacent string concatenation.** `"foo" "bar"` is treated as a single
  literal `"foobar"`, hashed once, replaced as a unit. Trailing tokens
  are emitted as `OMIT`. The parser also merges adjacent `TOK_STRING_LIT`s
  into a single `AST_STRING_LIT` so call arguments do not split a run.
- **Per-function dedup.** Each function has its own literal table; the
  same content twice gets one shared `static const char` array.
- **Array-init vs pointer detection via declarator inspection.** Before
  treating a string as a brace-init expansion, the destringer walks back
  to the anchoring `=` and looks for a top-level `*` in the declarator.
  Handles `char arr[] = "hi"`, `char *p = "hi"`, `char *a[] = {"x","y"}`,
  and the common nested cases.
- **Static-storage lifetime.** Function-scope lifted literals are
  `static const char sXXX[N] = {…}`, so `return "EOF"` works.
- **Idempotence.** `aligner(aligner(x))` is byte-identical to `aligner(x)`.
- **Self-formatting.** Every `src/*.c` runs through the aligner and the
  result compiles and produces matching corpus output.
- **Real expression + statement AST.** As of the v0.3 parser rewrite, each
  statement in a block becomes a typed node (`IF_STMT`, `FOR_STMT`,
  `RETURN_STMT`, …) and each expression is a typed sub-tree (`BINOP`,
  `CALL_EXPR`, `INDEX_EXPR`, `MEMBER_EXPR`, `COND_EXPR`, `ASSIGN_EXPR`,
  `UNOP_PRE`, `UNOP_POST`, `SIZEOF_EXPR`, etc.) with C99 precedence. The
  earlier coarse AST (TU / PP / DECL / FUNC_DEF / BLOCK) is still the
  outer skeleton; the new richness lives inside `BLOCK` and inside
  `EXPR_STMT`. Existing passes (destring, emit, align) are unchanged by
  the richer tree — they walk tokens — but new passes can now match on
  expression shape.

## Remaining — Tier 1 (small)

- **Wide string transformation.** `L"…"`, `u"…"`, `U"…"`, `u8"…"` are
  tokenized with decoded byte content but pass through unchanged. Lifting
  needs `wchar_t[]` / `char16_t[]` / `char32_t[]` declarations and a way
  to emit the correct element type per encoding.
- **PP-directive string contents.** A `#define` line is one token and the
  string inside survives in the output. Lifting strings out of macro
  replacement lists is the obvious next aesthetic win; it requires
  parsing into `TOK_PP_DIRECTIVE` and reasoning about where the
  replacement list begins.
- **Lines that naturally exceed 80 columns.** The aligner emits them at
  natural length (no padding) rather than wrapping. A "wrap at 80 if
  possible" pass could break at commas / operators, but wrapping is
  somewhat against the aesthetic.
- **Trigraphs / digraphs.** Untouched. Pre-process with `gcc -trigraphs
  -E` if you need them.
- **`#include` aesthetic.** Whole PP directive is one token; the path
  ends up right-justified to col 80. Could be prettier.

## Remaining — Tier 2 (medium)

- **Typedef-name tracking.** The parser does not maintain a typedef set,
  so `MyT x;` parses as an expression statement rather than a
  declaration. Token coverage and downstream behaviour are unaffected;
  the AST is less accurate for code using typedefs. Fix: keep a simple
  set of typedef names accumulated during parse, consult it in
  `parse_statement` to choose `parse_decl` vs `parse_expr_stmt`.
- **Casts as `CAST_EXPR`.** `(int)x` currently parses as a `PAREN_EXPR`
  followed by a postfix run. Without type info we cannot reliably
  distinguish `(T)x` from `(expr)*x`. Heuristic: if the parenthesised
  content is a single identifier (or known type-spec sequence) and the
  following token starts a unary expression, classify as `CAST_EXPR`.
  Today the AST is structurally correct but uses the looser kind.
- **Mixed-type aggregate initialization.**
  `struct{char buf[N]; char*p;} s = {"hello", "world"}` — `"hello"` should
  be brace-init, `"world"` should be pointer ref. Our declarator scan
  picks one decision per anchor. Per-field decisions need either position
  tracking inside the init list plus the struct layout, or real type
  info. Workaround: split into two statements.
- **Format-string analysis at compile time.** `printf("%d", x)` becomes
  `printf(sXXX, x)`, which defeats gcc's `-Wformat` checks. Program runs
  correctly; static-analysis loss. Fix: recognize `format`-attributed
  functions and skip lifting their format-string argument.
- **Multi-file dedup across translation units.** Every file gets its own
  `sXXX` declarations. A multi-file mode could emit a shared header.
  Mostly a CLI question.
- **Run `cpp` first.** Resolves `#define LONG "a" "b" "c"`-style macros,
  `#if`-conditional bodies, flattens includes. We lose the visible
  `#include` aesthetic — worth a `--preprocess` flag.

## Remaining — Tier 3 (hard, may not be worth it)

- **Full type-aware transformation via libclang.** Solves mixed-field
  structs, casts, typedef tracking, and every other case where the
  transformation depends on the static type of the target. Cost is a
  non-trivial C++ dependency. The current heuristics handle >99% of real
  code.
- **Source-level debugger fidelity.** Lines and tokens are completely
  rearranged; `gdb` line numbers in the transformed source don't map
  back. Could emit `# line` directives — adds another aesthetic
  violation.
- **`--unalign` (inverse mode).** Take aligned output and recover normal
  C. Requires the lifted byte arrays to be recognised, references
  substituted back, and the symbol layout undone. Mostly mechanical
  thanks to the named arrays, but a real project on its own.

## Won't fix — would change semantics

- **Pointer identity of literals.** In `const char *a = "x", *b = "x";`
  whether `a == b` is implementation-defined. After lifting they always
  share storage and `a == b` is true. Real programs don't depend on the
  other direction; documenting for completeness.
- **`__attribute__((section("…")))` on literals.** Cannot be replicated
  on the lifted arrays in general. Real but rare.

## Known parser-AST gaps (post-rewrite, today's work)

These are minor and listed for completeness — none affects whether the
aligner produces correct output.

- `AST_CAST_EXPR` is declared but never emitted (see Tier 2).
- `AST_SIZEOF_EXPR` with a parenthesised *type* (e.g. `sizeof(int)`)
  produces a `PAREN_EXPR` operand whose inner is `AST_UNKNOWN` rather
  than a type-spec subtree. The token range still covers exactly the
  right tokens.
- Designated initialisers (`.field = …`, `[i] = …`) are absorbed into
  the surrounding `AST_INIT_LIST` as a token span; we don't break them
  out into per-designator subtrees.
- Old-style (K&R) function parameter declarations between the
  signature `()` and the body `{` are absorbed into the function
  signature span; the parser doesn't expose them as separate nodes.
