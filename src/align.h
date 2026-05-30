/*
 * align.h — line-oriented right-justifier.
 *
 * For each input line, split into:
 *   - word prefix:  leading-whitespace-stripped run of `word | space` chars,
 *                   where `word` = letter | digit | underscore. Trailing
 *                   spaces are stripped.
 *   - symbol tail:  everything from the first "symbol" character onward.
 *
 * Output: word prefix, then spaces, then symbol tail; total width 80.
 * If the natural length already exceeds 80, no padding is inserted (the line
 * is emitted as-is).
 *
 * Blank lines (or lines containing only whitespace) are emitted as a single
 * newline.
 */
#ifndef CFMT_ALIGN_H
#define CFMT_ALIGN_H

#include "ds.h"

ds_status_t cfmt_align(const char *in, size_t in_len, ds_str_t *out);

#endif /* CFMT_ALIGN_H */
