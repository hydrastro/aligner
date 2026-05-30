#ifndef DS_STR_UNICODE_H
#define DS_STR_UNICODE_H

#include "common.h"
#include "str.h"
#include "str_io.h"
#include "unicode_runtime.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS_U8_REJECT_SURROGATE 0x01u
#define DS_U8_REJECT_NONCHAR 0x02u
#define DS_U8_REJECT_OVERLONG 0x04u
#define DS_U8_REJECT_OUT_OF_RANGE 0x08u
#define DS_U8_STRICT                                                           \
  (DS_U8_REJECT_SURROGATE | DS_U8_REJECT_NONCHAR | DS_U8_REJECT_OVERLONG |     \
   DS_U8_REJECT_OUT_OF_RANGE)

#define DS_U8_REPLACEMENT_CHAR 0xFFFDu

typedef struct {
  ds__grapheme_iter_t _it;
} ds_u8_grapheme_iter_t;

typedef enum ds_u8_width_flags {
  DS_WIDTH_DEFAULT = 0,
  DS_WIDTH_EMOJI_IS_DOUBLE = 1u << 0,
  DS_WIDTH_AMBIG_IS_DOUBLE = 1u << 1
} ds_u8_width_flags;

typedef struct {
  unsigned long a, b;
} ds__range;

int FUNC(str_u8_cp_width)(unsigned long cp, unsigned int flags);

int FUNC(str_u8_width)(const ds_str_t *s, unsigned int flags,
                       size_t *out_width);

int FUNC(str_u8_grapheme_iter_init)(ds_u8_grapheme_iter_t *it,
                                    const ds_str_t *s);

int FUNC(str_u8_grapheme_next)(ds_u8_grapheme_iter_t *it, size_t *out_start,
                               size_t *out_len);

int FUNC(str_view_u8_grapheme_next)(ds_u8_grapheme_iter_t *it,
                                    ds_str_view_t *out_view);

int FUNC(str_u8_slice_graphemes)(ds_str_t *dst, const ds_str_t *src,
                                 size_t start_gc, size_t count_gc);

int FUNC(str_u8_push_cp)(ds_str_t *s, unsigned long cp);

int FUNC(str_u8_pop_cp)(ds_str_t *s, unsigned long *out_cp);

size_t FUNC(str_u8_count)(const ds_str_t *s, unsigned int flags,
                          int *valid_out);
size_t FUNC(str_view_u8_count)(ds_str_view_t v, unsigned int flags,
                               int *valid_out);

int FUNC(str_u8_validate)(const ds_str_t *s, unsigned int flags);
int FUNC(str_view_u8_validate)(ds_str_view_t v, unsigned int flags);

int FUNC(str_u8_next)(const char *buf, size_t len, size_t *ioff,
                      unsigned long *out_cp, unsigned int flags);
int FUNC(str_u8_prev)(const char *buf, size_t len, size_t *ioff,
                      unsigned long *out_cp, unsigned int flags);

size_t FUNC(str_u8_cp_to_byte)(const ds_str_t *s, size_t cp_index,
                               unsigned int flags);

int FUNC(str_u8_slice_cp)(ds_str_t *dst, const ds_str_t *src, size_t start_cp,
                          size_t count_cp, unsigned int flags);

int FUNC(str_u8_sanitize)(ds_str_t *dst, const ds_str_t *src,
                          unsigned int flags);

int FUNC(str_u8_strip_bom)(ds_str_t *s);

int FUNC(str_u8_ascii_tolower)(ds_str_t *dst, const ds_str_t *src);
int FUNC(str_u8_ascii_toupper)(ds_str_t *dst, const ds_str_t *src);
int FUNC(str_u8_ascii_casefold)(ds_str_t *dst, const ds_str_t *src);

int FUNC(str_view_u8_next)(ds_str_view_t v, size_t *ioff, unsigned long *out_cp,
                           unsigned int flags);
int FUNC(str_view_u8_prev)(ds_str_view_t v, size_t *ioff, unsigned long *out_cp,
                           unsigned int flags);

typedef enum ds_u8_norm_form {
  DS_U8_NFC = 0,
  DS_U8_NFD = 1,
  DS_U8_NFKC = 2,
  DS_U8_NFKD = 3
} ds_u8_norm_form;

int FUNC(str_u8_normalize)(ds_str_t *dst, const ds_str_t *src,
                           ds_u8_norm_form form);

int FUNC(str_u8_casefold)(ds_str_t *dst, const ds_str_t *src);
int FUNC(str_u8_tolower)(ds_str_t *dst, const ds_str_t *src);
int FUNC(str_u8_toupper)(ds_str_t *dst, const ds_str_t *src);

long FUNC(str_find_ascii_folded)(ds_str_t *s, const void *needle, size_t n,
                                 size_t start);

long FUNC(str_u8_len_graphemes)(const ds_str_t *s);

int ds__is_nonchar(unsigned long cp);
int ds__decode_one(const unsigned char *s, size_t n, size_t *adv,
                   unsigned long *out, unsigned int flags);
size_t ds__encode_one(unsigned long cp, unsigned char out[4]);
unsigned char ds__ascii_lower(unsigned char c);
unsigned char ds__ascii_upper(unsigned char c);
unsigned char ds__ascii_fold(unsigned char c);

int FUNC(str_u8_backspace_one_egc)(ds_str_t *s);

int FUNC(str_u8_truncate_graphemes)(ds_str_t *s, size_t max_gc);

int FUNC(str_u8_grapheme_to_byte)(const ds_str_t *s, size_t gi,
                                  size_t *byte_out);

int FUNC(str_u8_byte_to_grapheme)(const ds_str_t *s, size_t byte,
                                  size_t *gi_out);

int FUNC(str_u8_delete_grapheme_at)(ds_str_t *s, size_t gi);

int FUNC(str_u8_insert_at_grapheme)(ds_str_t *s, size_t gi, const void *data,
                                    size_t n);

int FUNC(str_u8_delete_next_egc_from_byte)(ds_str_t *s, size_t cursor_byte);

long FUNC(str_u8_find_folded)(const ds_str_t *s, const ds_str_t *needle);

size_t FUNC(str_u8_cursor_prev)(const ds_str_t *s, size_t cursor_byte);

size_t FUNC(str_u8_cursor_next)(const ds_str_t *s, size_t cursor_byte);

int FUNC(str_u8_backspace)(ds_str_t *s, size_t cursor_byte, size_t *new_cursor);

int FUNC(str_u8_delete)(ds_str_t *s, size_t cursor_byte, size_t *new_cursor);

int ds__is_c0_c1(unsigned long cp);

int ds__in_ranges(unsigned long cp, const ds__range *r, size_t n);


#ifdef __cplusplus
}
#endif

#endif /* DS_STR_UNICODE_H */
