#include "str_unicode.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

int ds__is_nonchar(unsigned long cp) {
  if (cp >= 0xFDD0ul && cp <= 0xFDEFul) return 1;
  if ((cp & 0xFFFEul) == 0xFFFEul && cp <= 0x10FFFFul) return 1;
  return 0;
}

int ds__decode_one(const unsigned char *s, size_t n, size_t *adv,
                          unsigned long *out, unsigned int flags)
{
  unsigned long cp; size_t need;

  if (n == 0) return 0;
  if (s[0] < 0x80u) {
    *adv = 1; cp = s[0];
  } else if ((s[0] & 0xE0u) == 0xC0u) {
    if (n < 2 || (s[1] & 0xC0u) != 0x80u) return -1;
    cp = ((unsigned long)(s[0] & 0x1Fu) << 6) | (unsigned long)(s[1] & 0x3Fu);
    *adv = 2; if ((flags & DS_U8_REJECT_OVERLONG) && cp < 0x80ul) return -1;
  } else if ((s[0] & 0xF0u) == 0xE0u) {
    if (n < 3 || (s[1] & 0xC0u) != 0x80u || (s[2] & 0xC0u) != 0x80u) return -1;
    cp = ((unsigned long)(s[0] & 0x0Fu) << 12)
       | ((unsigned long)(s[1] & 0x3Fu) << 6)
       |  (unsigned long)(s[2] & 0x3Fu);
    *adv = 3;
    if ((flags & DS_U8_REJECT_OVERLONG) && cp < 0x800ul) return -1;
    if ((flags & DS_U8_REJECT_SURROGATE) && cp >= 0xD800ul && cp <= 0xDFFFul) return -1;
  } else if ((s[0] & 0xF8u) == 0xF0u) {
    if (n < 4 || (s[1] & 0xC0u) != 0x80u || (s[2] & 0xC0u) != 0x80u || (s[3] & 0xC0u) != 0x80u) return -1;
    cp = ((unsigned long)(s[0] & 0x07u) << 18)
       | ((unsigned long)(s[1] & 0x3Fu) << 12)
       | ((unsigned long)(s[2] & 0x3Fu) << 6)
       |  (unsigned long)(s[3] & 0x3Fu);
    *adv = 4;
    if ((flags & DS_U8_REJECT_OVERLONG) && cp < 0x10000ul) return -1;
    if ((flags & DS_U8_REJECT_OUT_OF_RANGE) && cp > 0x10FFFFul) return -1;
  } else {
    return -1;
  }

  if ((flags & DS_U8_REJECT_OUT_OF_RANGE) && cp > 0x10FFFFul) return -1;
  if ((flags & DS_U8_REJECT_NONCHAR) && ds__is_nonchar(cp)) return -1;

  if (out) *out = cp;
  (void)need;
  return 1;
}

size_t ds__encode_one(unsigned long cp, unsigned char out[4]) {
  if (cp > 0x10FFFFul || (cp >= 0xD800ul && cp <= 0xDFFFul)) cp = 0xFFFDul;
  if (cp <= 0x7Ful) { out[0]=(unsigned char)cp; return 1; }
  if (cp <= 0x7FFul) { out[0]=(unsigned char)(0xC0u | (cp>>6));
                       out[1]=(unsigned char)(0x80u | (cp & 0x3Fu)); return 2; }
  if (cp <= 0xFFFFul) { out[0]=(unsigned char)(0xE0u | (cp>>12));
                        out[1]=(unsigned char)(0x80u | ((cp>>6) & 0x3Fu));
                        out[2]=(unsigned char)(0x80u | (cp & 0x3Fu)); return 3; }
  out[0]=(unsigned char)(0xF0u | (cp>>18));
  out[1]=(unsigned char)(0x80u | ((cp>>12) & 0x3Fu));
  out[2]=(unsigned char)(0x80u | ((cp>>6) & 0x3Fu));
  out[3]=(unsigned char)(0x80u | (cp & 0x3Fu)); return 4;
}

int FUNC(str_u8_push_cp)(ds_str_t *s, unsigned long cp) {
  unsigned char buf[4]; size_t k;
  if (!s) return -1;
  if (cp > 0x10FFFFul || (cp >= 0xD800ul && cp <= 0xDFFFul)) cp = DS_U8_REPLACEMENT_CHAR;
  k = ds__encode_one(cp, buf);
  return FUNC(str_append)(s, buf, k);
}

int FUNC(str_u8_pop_cp)(ds_str_t *s, unsigned long *out_cp) {
  size_t len; const unsigned char *p; size_t i = 0;
  if (!s || s->len == 0) return -1;
  len = s->len; p = (const unsigned char*)s->buf;
  i = len;
  while (i > 0) {
    unsigned char c = p[i-1];
    if ((c & 0xC0u) != 0x80u) {
      size_t adv; unsigned long cp;
      if (ds__decode_one(p + (i-1), len - (i-1), &adv, &cp, DS_U8_STRICT) <= 0) return -1;
      if (i - 1 + adv != len) return -1;
      if (out_cp) *out_cp = cp;
      return FUNC(str_erase)(s, i-1, adv);
    }
    --i;
    if (len - i > 4) break;
  }
  return -1;
}

size_t FUNC(str_u8_count)(const ds_str_t *s, unsigned int flags, int *valid_out) {
  size_t i = 0, n; size_t count = 0; const unsigned char *p;
  int ok = 1;
  if (!s) { if (valid_out) *valid_out = 0; return 0; }
  n = s->len; p = (const unsigned char*)s->buf;
  while (i < n) {
    size_t adv; int r = ds__decode_one(p + i, n - i, &adv, NULL, flags);
    if (r <= 0) { ok = 0; break; }
    ++count; i += adv;
  }
  if (valid_out) *valid_out = ok;
  return ok ? count : 0;
}

size_t FUNC(str_view_u8_count)(ds_str_view_t v, unsigned int flags, int *valid_out) {
  const unsigned char *p = (const unsigned char *)v.data;
  size_t i = 0, n = v.len, count = 0;
  int ok = 1;

  while (i < n) {
    size_t adv;
    int r = ds__decode_one(p + i, n - i, &adv, NULL, flags);
    if (r <= 0) { ok = 0; break; }
    ++count; i += adv;
  }
  if (valid_out) *valid_out = ok;
  return ok ? count : 0;
}

int FUNC(str_u8_validate)(const ds_str_t *s, unsigned int flags) {
  int ok; (void)FUNC(str_u8_count)(s, flags, &ok); return ok;
}

int FUNC(str_view_u8_validate)(ds_str_view_t v, unsigned int flags) {
  int ok; (void)FUNC(str_view_u8_count)(v, flags, &ok); return ok;
}

int FUNC(str_u8_next)(const char *buf, size_t len, size_t *ioff, unsigned long *out_cp, unsigned int flags) {
  size_t i = ioff ? *ioff : 0; size_t adv; int r;
  if (!buf) return -1;
  if (i >= len) return 0;
  r = ds__decode_one((const unsigned char*)buf + i, len - i, &adv, out_cp, flags);
  if (r <= 0) return -1;
  if (ioff) *ioff = i + adv;
  return 1;
}

int FUNC(str_u8_prev)(const char *buf, size_t len, size_t *ioff, unsigned long *out_cp, unsigned int flags) {
  size_t i = ioff ? *ioff : len;
  const unsigned char *p = (const unsigned char*)buf;
  if (!buf) return -1;
  if (i == 0) return 0;
  {
    size_t j = i;
    size_t start = j;
    size_t adv; unsigned long cp;
    if (j > len) j = len;
    while (j > 0 && (p[j-1] & 0xC0u) == 0x80u) --j;
    if (j == 0) return -1;
    start = j;
    if (ds__decode_one(p + start, len - start, &adv, &cp, flags) <= 0) return -1;
    if (start + adv != i) return -1;
    if (out_cp) *out_cp = cp;
    if (ioff) *ioff = start;
  }
  return 1;
}

size_t FUNC(str_u8_cp_to_byte)(const ds_str_t *s, size_t cp_index, unsigned int flags) {
  size_t i = 0, n; size_t cp = 0; const unsigned char *p;
  if (!s) return SIZE_MAX;
  n = s->len; p = (const unsigned char*)s->buf;
  while (i < n) {
    size_t adv; int r = ds__decode_one(p + i, n - i, &adv, NULL, flags);
    if (r <= 0) return SIZE_MAX;
    if (cp == cp_index) return i;
    ++cp; i += adv;
  }
  return (cp_index == cp) ? i : SIZE_MAX;
}

int FUNC(str_u8_slice_cp)(ds_str_t *dst, const ds_str_t *src, size_t start_cp, size_t count_cp, unsigned int flags) {
  size_t a, b;
  size_t start_byte = FUNC(str_u8_cp_to_byte)(src, start_cp, flags);
  if (start_byte == SIZE_MAX) return -1;

  if (count_cp == (size_t)-1) {
    a = start_byte; b = src->len;
  } else {
    size_t end_cp = start_cp + count_cp;
    size_t end_byte = FUNC(str_u8_cp_to_byte)(src, end_cp, flags);
    if (end_byte == SIZE_MAX) return -1;
    a = start_byte; b = end_byte;
  }

  if (dst == src) {
    ds_str_t *tmp = FUNC(str_from)(src->buf + a, b - a);
    if (!tmp) return -1;
    FUNC(str_clear)(dst);
    (void)FUNC(str_append)(dst, tmp->buf, tmp->len);
    FUNC(str_destroy)(tmp);
    return 0;
  }
  return FUNC(str_assign)(dst, src->buf + a, b - a);
}

int FUNC(str_u8_sanitize)(ds_str_t *dst, const ds_str_t *src, unsigned int flags) {
  size_t i = 0, n;
  const unsigned char *p;
  unsigned char enc[4];

  if (!dst || !src) return -1;

  if (dst == src) {
    ds_str_t *tmp = FUNC(str_create)();
    int rc;
    if (!tmp) return -1;
    rc = FUNC(str_u8_sanitize)(tmp, src, flags);
    if (rc == 0) rc = FUNC(str_assign)(dst, tmp->buf, tmp->len);
    FUNC(str_destroy)(tmp);
    return rc;
  }

#ifdef DS_THREAD_SAFE
  LOCK(dst);
#endif
  FUNC(str_clear)(dst);

  n = src->len;
  p = (const unsigned char *)src->buf;

  while (i < n) {
    unsigned long cp;
    size_t adv;
    int r = ds__decode_one(p + i, n - i, &adv, &cp, flags);
    if (r <= 0) {
      size_t k = ds__encode_one(DS_U8_REPLACEMENT_CHAR, enc);
      if (FUNC(str_append)(dst, enc, k) != 0) {
#ifdef DS_THREAD_SAFE
        UNLOCK(dst);
#endif
        return -1;
      }
      i += 1;
    } else {
      size_t k = ds__encode_one(cp, enc);
      if (FUNC(str_append)(dst, enc, k) != 0) {
#ifdef DS_THREAD_SAFE
        UNLOCK(dst);
#endif
        return -1;
      }
      i += adv;
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(dst);
#endif
  return 0;
}

int FUNC(str_u8_strip_bom)(ds_str_t *s) {
  const unsigned char bom[3] = {0xEFu, 0xBBu, 0xBFu};
  if (!s || s->len < 3) return 0;
  if ((unsigned char)s->buf[0] == bom[0] &&
      (unsigned char)s->buf[1] == bom[1] &&
      (unsigned char)s->buf[2] == bom[2]) {
    (void)FUNC(str_erase)(s, 0u, 3u);
    return 1;
  }
  return 0;
}

unsigned char ds__ascii_lower(unsigned char c) { return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + 32u) : c; }
unsigned char ds__ascii_upper(unsigned char c) { return (c >= 'a' && c <= 'z') ? (unsigned char)(c - 32u) : c; }

int FUNC(str_u8_ascii_tolower)(ds_str_t *dst, const ds_str_t *src) {
  size_t i, n;
  if (!dst || !src) return -1;
#ifdef DS_THREAD_SAFE
  if (dst) LOCK(dst);
#endif
  if (FUNC(str_reserve)(dst, src->len) != 0) { 
#ifdef DS_THREAD_SAFE
    if (dst) UNLOCK(dst);
#endif
    return -1; 
  }
  for (i = 0, n = src->len; i < n; ++i) dst->buf[i] = (char)ds__ascii_lower((unsigned char)src->buf[i]);
  dst->len = src->len; dst->buf[dst->len] = '\0';
#ifdef DS_THREAD_SAFE
  if (dst) UNLOCK(dst);
#endif
  return 0;
}

int FUNC(str_u8_ascii_toupper)(ds_str_t *dst, const ds_str_t *src) {
  size_t i, n;
  if (!dst || !src) return -1;
#ifdef DS_THREAD_SAFE
  if (dst) LOCK(dst);
#endif
  if (FUNC(str_reserve)(dst, src->len) != 0) { 
#ifdef DS_THREAD_SAFE
    if (dst) UNLOCK(dst);
#endif
    return -1; 
  }
  for (i = 0, n = src->len; i < n; ++i) dst->buf[i] = (char)ds__ascii_upper((unsigned char)src->buf[i]);
  dst->len = src->len; dst->buf[dst->len] = '\0';
#ifdef DS_THREAD_SAFE
  if (dst) UNLOCK(dst);
#endif
  return 0;
}

int FUNC(str_u8_ascii_casefold)(ds_str_t *dst, const ds_str_t *src) {
  return FUNC(str_u8_ascii_tolower)(dst, src);
}

unsigned char ds__ascii_fold(unsigned char c) { return ds__ascii_lower(c); }

long FUNC(str_find_ascii_folded)(ds_str_t *s, const void *needle, size_t n, size_t start) {
  const unsigned char *h, *nd = (const unsigned char*)needle; size_t i, len;
  if (!s || !needle || n == 0u) return -1;
  len = s->len; if (start > len) return -1;
  h = (const unsigned char*)s->buf;
  if (n == 1u) {
    unsigned char t = ds__ascii_fold(nd[0]);
    for (i = start; i < len; ++i) if (ds__ascii_fold(h[i]) == t) return (long)i;
    return -1;
  }
  for (i = start; i + n <= len; ++i) {
    size_t k = 0;
    while (k < n && ds__ascii_fold(h[i+k]) == ds__ascii_fold(nd[k])) ++k;
    if (k == n) return (long)i;
  }
  return -1;
}

int FUNC(str_u8_normalize)(ds_str_t *dst, const ds_str_t *src, ds_u8_norm_form form) {
  int compat, compose;
  (void)form;
  compat = (form == DS_U8_NFKC || form == DS_U8_NFKD);
  compose = (form == DS_U8_NFC  || form == DS_U8_NFKC);
  return ds__ucd_normalize(dst, src, compat, compose);
}

int FUNC(str_u8_casefold)(ds_str_t *dst, const ds_str_t *src) {
  return ds__ucd_casefold(dst, src);
}

int FUNC(str_u8_tolower)(ds_str_t *dst, const ds_str_t *src) {
  return ds__ucd_tolower(dst, src);
}

int FUNC(str_u8_toupper)(ds_str_t *dst, const ds_str_t *src) {
  return ds__ucd_toupper(dst, src);
}

long FUNC(str_u8_len_graphemes)(const ds_str_t *s) {
  return ds__ucd_grapheme_len(s);
}


int FUNC(str_view_u8_next)(ds_str_view_t v, size_t *ioff, unsigned long *out_cp, unsigned int flags) {
  size_t i = ioff ? *ioff : 0, adv; int r;
  if (i >= v.len) return 0;
  r = ds__decode_one((const unsigned char*)v.data + i, v.len - i, &adv, out_cp, flags);
  if (r <= 0) return -1;
  if (ioff) *ioff = i + adv;
  return 1;
}

int FUNC(str_view_u8_prev)(ds_str_view_t v, size_t *ioff, unsigned long *out_cp, unsigned int flags) {
  size_t i = ioff ? *ioff : v.len;
  const unsigned char *p = (const unsigned char*)v.data;
  if (i == 0) return 0;
  {
    size_t j = i, start, adv; unsigned long cp;
    while (j > 0 && (p[j-1] & 0xC0u) == 0x80u) --j;
    if (j == 0) return -1;
    start = j;
    if (ds__decode_one(p + start, v.len - start, &adv, &cp, flags) <= 0) return -1;
    if (start + adv != i) return -1;
    if (out_cp) *out_cp = cp;
    if (ioff) *ioff = start;
  }
  return 1;
}

int FUNC(str_u8_grapheme_iter_init)(ds_u8_grapheme_iter_t *it, const ds_str_t *s) {
  if (!it || !s) return -1;
  if (ucd_init_once()!=0) return -1;
  return ds__grapheme_iter_init(&it->_it, s->buf, s->len);
}

int FUNC(str_u8_grapheme_next)(ds_u8_grapheme_iter_t *it, size_t *out_start, size_t *out_len) {
  if (!it) return -1;
  return ds__grapheme_iter_next(&it->_it, out_start, out_len);
}

int FUNC(str_view_u8_grapheme_next)(ds_u8_grapheme_iter_t *it, ds_str_view_t *out_view) {
  size_t a, n;
  int r;
  if (!it || !out_view) return -1;
  r = ds__grapheme_iter_next(&it->_it, &a, &n);
  if (r == 1) {
    out_view->data = it->_it.buf + a;
    out_view->len  = n;
  }
  return r;
}

int FUNC(str_u8_slice_graphemes)(ds_str_t *dst, const ds_str_t *src,
                                 size_t start_gc, size_t count_gc) {
  ds_u8_grapheme_iter_t it;
  size_t a = 0, b = 0;
  size_t gc = 0;
  int r;

  if (!dst || !src) return -1;
  if (ucd_init_once()!=0) return -1;

  r = FUNC(str_u8_grapheme_iter_init)(&it, src);
  if (r != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &b)) == 1) {
    if (gc == start_gc) {
      size_t start_byte = a;
      size_t end_byte   = a + b;

      if (count_gc == (size_t)-1) {
        end_byte = src->len;
        return FUNC(str_assign)(dst, src->buf + start_byte, end_byte - start_byte);
      }

      {
        size_t left = (count_gc > 0) ? (count_gc - 1) : 0;
        while (left > 0) {
          r = FUNC(str_u8_grapheme_next)(&it, &a, &b);
          if (r != 1) break;
          end_byte = a + b;
          --left;
        }
      }
      return FUNC(str_assign)(dst, src->buf + start_byte, (end_byte > start_byte) ? (end_byte - start_byte) : 0);
    }
    ++gc;
  }

  return FUNC(str_assign)(dst, "", 0u);
}

int FUNC(str_u8_backspace_one_egc)(ds_str_t *s) {
  ds_u8_grapheme_iter_t it;
  size_t a=0, n=0;
  size_t last_a=0, last_n=0;
  int r, saw_any = 0;

  if (!s) return -1;
  if (s->len == 0) return 0;
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    last_a = a; last_n = n; saw_any = 1;
  }
  if (r < 0) return -1;
  if (!saw_any) return 0;

  return FUNC(str_erase)(s, last_a, last_n) == 0 ? 1 : -1;
}

int FUNC(str_u8_truncate_graphemes)(ds_str_t *s, size_t max_gc) {
  ds_u8_grapheme_iter_t it;
  size_t a=0, n=0;
  size_t gc = 0;
  size_t cut_byte = s ? s->len : 0;
  int r;

  if (!s) return -1;
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (gc == max_gc) { cut_byte = a; break; }
    ++gc;
  }
  if (r < 0) return -1;

  if (gc <= max_gc) return 0;

  return FUNC(str_erase)(s, cut_byte, s->len - cut_byte);
}

int FUNC(str_u8_grapheme_to_byte)(const ds_str_t *s, size_t gi, size_t *byte_out) {
  ds_u8_grapheme_iter_t it;
  size_t a, n, idx = 0;
  int r;

  if (!s || !byte_out) return -1;
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (idx == gi) { *byte_out = a; return 0; }
    idx++;
  }
  if (r < 0) return -1;

  if (idx == gi) { *byte_out = s->len; return 0; }

  return -1;
}

int FUNC(str_u8_byte_to_grapheme)(const ds_str_t *s, size_t byte, size_t *gi_out) {
  ds_u8_grapheme_iter_t it;
  size_t a, n, idx = 0;
  int r;

  if (!s || !gi_out) return -1;
  if (byte > s->len) byte = s->len;
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (byte < a + n) { *gi_out = idx; return 0; }
    idx++;
  }
  if (r < 0) return -1;

  *gi_out = idx;
  return 0;
}

int FUNC(str_u8_delete_grapheme_at)(ds_str_t *s, size_t gi) {
  ds_u8_grapheme_iter_t it;
  size_t a, n, idx = 0;
  int r;

  if (!s) return -1;
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (idx == gi) {
      return FUNC(str_erase)(s, a, n) == 0 ? 1 : -1;
    }
    idx++;
  }
  if (r < 0) return -1;
  return 0;
}

int FUNC(str_u8_insert_at_grapheme)(ds_str_t *s, size_t gi, const void *data, size_t n) {
  size_t byte = 0;
  if (!s) return -1;
  if (FUNC(str_u8_grapheme_to_byte)(s, gi, &byte) != 0) return -1;
  return FUNC(str_insert)(s, byte, data, n);
}

int FUNC(str_u8_delete_next_egc_from_byte)(ds_str_t *s, size_t cursor_byte) {
  ds_u8_grapheme_iter_t it;
  size_t a, n;
  int r;

  if (!s) return -1;
  if (cursor_byte >= s->len) return 0;
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (cursor_byte < a + n) {
      return FUNC(str_erase)(s, a, n) == 0 ? 1 : -1;
    }
  }
  return (r < 0) ? -1 : 0;
}

long FUNC(str_u8_find_folded)(const ds_str_t *s, const ds_str_t *needle) {
  ds_str_t *hs = NULL, *ns = NULL;
  long pos = -1;

  if (!s || !needle) return -1;
  if (ucd_init_once()!=0) return -1;

  hs = FUNC(str_create)(); ns = FUNC(str_create)();
  if (!hs || !ns) goto done;

  if (FUNC(str_u8_casefold)(hs, s) != 0) goto done;
  if (FUNC(str_u8_normalize)(hs, hs, DS_U8_NFC) != 0) goto done;

  if (FUNC(str_u8_casefold)(ns, needle) != 0) goto done;
  if (FUNC(str_u8_normalize)(ns, ns, DS_U8_NFC) != 0) goto done;

  pos = FUNC(str_find)((ds_str_t*)hs, ns->buf, ns->len, 0);

done:
  if (hs) FUNC(str_destroy)(hs);
  if (ns) FUNC(str_destroy)(ns);
  return pos;
}

size_t FUNC(str_u8_cursor_prev)(const ds_str_t *s, size_t cursor_byte) {
  ds_u8_grapheme_iter_t it;
  size_t a, n;
  size_t last_start = 0;
  int r;

  if (!s) return SIZE_MAX;
  if (cursor_byte > s->len) cursor_byte = s->len;
  if (ucd_init_once()!=0) return SIZE_MAX;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return SIZE_MAX;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (a + n >= cursor_byte) {

      return (cursor_byte == 0) ? 0 : last_start;
    }
    last_start = a;
  }
  if (r < 0) return SIZE_MAX;

  return last_start;
}

size_t FUNC(str_u8_cursor_next)(const ds_str_t *s, size_t cursor_byte) {
  ds_u8_grapheme_iter_t it;
  size_t a, n;
  int r;

  if (!s) return SIZE_MAX;
  if (cursor_byte >= s->len) return s->len;
  if (ucd_init_once()!=0) return SIZE_MAX;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return SIZE_MAX;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (cursor_byte < a + n) {

      size_t end = a + n;
      if (end > s->len) end = s->len;
      return end;
    }
  }
  if (r < 0) return SIZE_MAX;
  return s->len;
}

int FUNC(str_u8_backspace)(ds_str_t *s, size_t cursor_byte, size_t *new_cursor) {
  ds_u8_grapheme_iter_t it;
  size_t a, n, prev_a = 0, prev_n = 0;
  int r;

  if (!s) return -1;
  if (cursor_byte == 0) { if (new_cursor) *new_cursor = 0; return 0; }
  if (cursor_byte > s->len) cursor_byte = s->len;
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (a + n >= cursor_byte) {
      if (prev_n == 0 && a == 0) { if (new_cursor) *new_cursor = 0; return 0; }
      if (prev_n == 0 && a > 0)  {
        if (FUNC(str_erase)(s, 0, a + n) != 0) return -1;
        if (new_cursor) *new_cursor = 0;
        return 1;
      }
      if (prev_n > 0) {
        if (FUNC(str_erase)(s, prev_a, prev_n) != 0) return -1;
        if (new_cursor) *new_cursor = prev_a;
        return 1;
      }
    }
    prev_a = a; prev_n = n;
  }
  if (r < 0) return -1;

  if (prev_n > 0) {
    if (FUNC(str_erase)(s, prev_a, prev_n) != 0) return -1;
    if (new_cursor) *new_cursor = prev_a;
    return 1;
  }
  if (new_cursor) *new_cursor = 0;
  return 0;
}

int FUNC(str_u8_delete)(ds_str_t *s, size_t cursor_byte, size_t *new_cursor) {
  ds_u8_grapheme_iter_t it;
  size_t a, n;
  int r;

  if (!s) return -1;
  if (cursor_byte >= s->len) { if (new_cursor) *new_cursor = s->len; return 0; }
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    if (cursor_byte < a + n) {
      if (FUNC(str_erase)(s, a, n) != 0) return -1;
      if (new_cursor) *new_cursor = cursor_byte;

      return 1;
    }
  }
  return (r < 0) ? -1 : 0;
}

int ds__is_c0_c1(unsigned long cp) {
  return (cp < 0x20ul) || (cp == 0x7Ful) || (cp >= 0x80ul && cp <= 0x9Ful);
}

const ds__range DS__WIDE_TABLE[] = {
  {0x1100ul, 0x115Ful},
  {0x231Aul, 0x231Bul},
  {0x2329ul, 0x232Aul},
  {0x2E80ul, 0x303Eul},
  {0x3040ul, 0x3247ul},
  {0x3250ul, 0x4DBFul},
  {0x4E00ul, 0xA4C6ul},
  {0xA960ul, 0xA97Cul},
  {0xAC00ul, 0xD7A3ul},
  {0xF900ul, 0xFAFFul},
  {0xFE10ul, 0xFE19ul},
  {0xFE30ul, 0xFE6Bul},
  {0xFF01ul, 0xFF60ul},
  {0xFFE0ul, 0xFFE6ul},
  {0x1F300ul, 0x1F64Ful},
  {0x1F900ul, 0x1F9FFul},
  {0x20000ul,0x2FFFDul},
  {0x30000ul,0x3FFFDul}
};

int ds__in_ranges(unsigned long cp, const ds__range *r, size_t n) {
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = lo + (hi - lo)/2;
    if (cp < r[mid].a) hi = mid;
    else if (cp > r[mid].b) lo = mid + 1;
    else return 1;
  }
  return 0;
}

int FUNC(str_u8_cp_width)(unsigned long cp, unsigned int flags) {
  if (ds__is_c0_c1(cp)) return -1;

  if (get_ccc(cp) != 0ul) return 0;

  if (ds__in_ranges(cp, DS__WIDE_TABLE, sizeof(DS__WIDE_TABLE)/sizeof(DS__WIDE_TABLE[0]))) {
    return 2;
  }

  (void)flags;

  return 1;
}

int FUNC(str_u8_width)(const ds_str_t *s, unsigned int flags, size_t *out_width) {
  ds_u8_grapheme_iter_t it;
  size_t a, n;
  int r;
  size_t total = 0;

  if (!s || !out_width) return -1;
  if (ucd_init_once()!=0) return -1;

  if (FUNC(str_u8_grapheme_iter_init)(&it, s) != 0) return -1;

  while ((r = FUNC(str_u8_grapheme_next)(&it, &a, &n)) == 1) {
    int cluster_has_control = 0;
    int cluster_has_EP = 0;
    int cluster_base_width = 0;

    size_t i = a;
    while (i < a + n) {
      size_t adv;
      unsigned long cp;
      if (ds__decode_one((const unsigned char*)s->buf + i, (a + n) - i, &adv, &cp, DS_U8_STRICT) <= 0) {
        cluster_has_control = 0;
        if (cluster_base_width == 0) cluster_base_width = 1;
        i += 1;
        continue;
      }

      if (ds__is_c0_c1(cp)) {
        cluster_has_control = 1;
      }
      if (is_EP(cp)) {
        cluster_has_EP = 1;
      }

      if (get_ccc(cp) == 0ul && cluster_base_width == 0) {
        int w = FUNC(str_u8_cp_width)(cp, flags);
        if (w < 0) w = 0;
        cluster_base_width = w;
      }

      i += adv;
    }

    if (cluster_has_control) {
      continue;
    }

    if ((flags & DS_WIDTH_EMOJI_IS_DOUBLE) && cluster_has_EP) {
      total += 2;
      continue;
    }

    if (cluster_base_width == 0) {
      continue;
    }

    total += (size_t)cluster_base_width;
  }

  if (r < 0) return -1;
  *out_width = total;
  return 0;
}
