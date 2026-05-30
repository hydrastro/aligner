#include "str_algo.h"
#include <string.h>
#include <stdlib.h>

int ds__in_set(unsigned char c, const unsigned char *set, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) if (c == set[i]) return 1;
  return 0;
}

void ds__maximal_suffix(const unsigned char *x, size_t m, int *ms, int *p) {
  size_t i = 0, j = 1, k = 1;
  int a;
  *p = 1;
  *ms = 0;
  while (j + k <= m) {
    a = (int)x[j + k - 1] - (int)x[i + k - 1];
    if (a < 0) { j += k; k = 1; *p = (int)(j - i); }
    else if (a > 0) { i = j; j = i + 1; k = 1; *p = 1; }
    else { if (k == (size_t)*p) { j += (size_t)*p; k = 1; } else { ++k; } }
  }
}

void ds__maximal_suffix_rev(const unsigned char *x, size_t m, int *ms, int *p) {
  size_t i = 0, j = 1, k = 1;
  int a;
  *p = 1;
  *ms = 0;
  while (j + k <= m) {
    a = (int)x[i + k - 1] - (int)x[j + k - 1];
    if (a < 0) { j += k; k = 1; *p = (int)(j - i); }
    else if (a > 0) { i = j; j = i + 1; k = 1; *p = 1; }
    else { if (k == (size_t)*p) { j += (size_t)*p; k = 1; } else { ++k; } }
  }
}

long FUNC(str_find_twoway)(ds_str_t *s, const void *needle, size_t m, size_t start) {
  const unsigned char *y;
  const unsigned char *x = (const unsigned char*)needle;
  size_t n;
  int ms, p1, p2;
  size_t q, j, memory;
  long ret = -1;
  if (!s || !needle || m == 0u) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  n = s->len;
  if (start > n) { 
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1; 
  }
  y = (const unsigned char*)s->buf;
  if (m == 1u) {
    const void *p = memchr(y + start, x[0], n - start);
    ret = p ? (long)((const unsigned char*)p - y) : -1;
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return ret;
  }
  ds__maximal_suffix(x, m, &ms, &p1);
  q = (size_t)ms;
  ds__maximal_suffix_rev(x, m, &ms, &p2);
  if ((size_t)ms > q) q = (size_t)ms;

  if (memcmp(x, x + p1, (size_t)(m - (size_t)p1)) == 0) {
    size_t per = (size_t)p1;
    j = start; memory = 0;
    while (j <= n - m) {
      size_t i = (q > memory) ? q : memory;
      while (i < m && x[i] == y[j + i]) ++i;
      if (i >= m) {
        i = q;
        while (i > memory && x[i - 1] == y[j + i - 1]) --i;
        if (i <= memory) { ret = (long)j; break; }
        j += per; memory = m - per;
      } else {
        j += (i - q + 1); memory = 0;
      }
    }
  } else {
    size_t per = (size_t)p1;
    j = start;
    while (j <= n - m) {
      size_t i = q;
      while (i < m && x[i] == y[j + i]) ++i;
      if (i >= m) {
        i = q;
        while (i > 0 && x[i - 1] == y[j + i - 1]) --i;
        if (i == 0) { ret = (long)j; break; }
        j += per;
      } else {
        j += (i - q + 1);
      }
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return ret;
}

long FUNC(str_find_bmh)(ds_str_t *s, const void *needle, size_t n, size_t start) {
  const unsigned char *h, *pat = (const unsigned char*)needle;
  size_t i, len;
  unsigned char skip[256];
  long ret = -1;
  if (!s || !needle || n == 0u) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  len = s->len;
  if (start > len) { 
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1; 
  }
  if (n == 1u) {
    const void *p = memchr(s->buf + start, *pat, len - start);
    ret = p ? (long)((const unsigned char*)p - (const unsigned char*)s->buf) : -1;
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return ret;
  }
  if (n > 255u) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return FUNC(str_find_twoway)(s, needle, n, start);
  }
  for (i = 0; i < 256; ++i) skip[i] = (unsigned char)n;
  for (i = 0; i < n - 1; ++i) skip[pat[i]] = (unsigned char)(n - 1 - i);
  h = (const unsigned char*)s->buf; i = start;
  while (i + n <= len) {
    unsigned char last = h[i + n - 1];
    if (last == pat[n - 1] && memcmp(h + i, pat, n) == 0) { ret = (long)i; break; }
    i += skip[last];
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return ret;
}

int FUNC(str_ltrim_set)(ds_str_t *s, const unsigned char *set, size_t set_n) {
  size_t i = 0u, len;
  if (!s) return -1;
  len = s->len;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  while (i < len && ds__in_set((unsigned char)s->buf[i], set, set_n)) ++i;
  if (i) {
    memmove(s->buf, s->buf + i, len - i);
    s->len = len - i;
    s->buf[s->len] = '\0';
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_rtrim_set)(ds_str_t *s, const unsigned char *set, size_t set_n) {
  size_t len;
  if (!s) return -1;
  len = s->len;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  while (len && ds__in_set((unsigned char)s->buf[len - 1], set, set_n)) --len;
  s->len = len;
  s->buf[len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_trim_set)(ds_str_t *s, const unsigned char *set, size_t set_n) {
  if (FUNC(str_rtrim_set)(s, set, set_n) != 0) return -1;
  return FUNC(str_ltrim_set)(s, set, set_n);
}

int FUNC(str_ltrim_pred)(ds_str_t *s, ds_byte_pred pred) {
  size_t i = 0u, len;
  if (!s || !pred) return -1;
  len = s->len;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  while (i < len && pred((unsigned char)s->buf[i])) ++i;
  if (i) {
    memmove(s->buf, s->buf + i, len - i);
    s->len = len - i;
    s->buf[s->len] = '\0';
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_rtrim_pred)(ds_str_t *s, ds_byte_pred pred) {
  size_t len;
  if (!s || !pred) return -1;
  len = s->len;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  while (len && pred((unsigned char)s->buf[len - 1])) --len;
  s->len = len;
  s->buf[len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_trim_pred)(ds_str_t *s, ds_byte_pred pred) {
  if (FUNC(str_rtrim_pred)(s, pred) != 0) return -1;
  return FUNC(str_ltrim_pred)(s, pred);
}

void FUNC(str_split_each_c)(ds_str_t *s, unsigned char sep, ds_str_token_cb cb, void *ud) {
  size_t i = 0u, start = 0u, len;
  if (!s || !cb) return;
  len = s->len;
  for (i = 0; i < len; ++i) {
    if ((unsigned char)s->buf[i] == sep) {
      cb(s->buf + start, i - start, ud);
      start = i + 1;
    }
  }
  cb(s->buf + start, len - start, ud);
}

void FUNC(str_split_each_substr)(ds_str_t *s, const void *sep, size_t nsep, ds_str_token_cb cb, void *ud) {
  size_t pos = 0u;
  long at;
  if (!s || !cb || !sep || nsep == 0u) return;
  for (;;) {
    at = FUNC(str_find_twoway)(s, sep, nsep, pos);
    if (at < 0) {
      cb(s->buf + pos, s->len - pos, ud);
      break;
    }
    cb(s->buf + pos, (size_t)at - pos, ud);
    pos = (size_t)at + nsep;
    if (pos > s->len) pos = s->len;
  }
}

int FUNC(str_join_c)(ds_str_t *dst, ds_str_t **parts, size_t count, const void *sep, size_t nsep) {
  size_t i;
  if (!dst) return -1;
  if (!parts || count == 0) return 0;
#ifdef DS_THREAD_SAFE
  LOCK(dst);
#endif
  for (i = 0; i < count; ++i) {
    if (i) {
      if (FUNC(str_append)(dst, sep, nsep) != 0) {
#ifdef DS_THREAD_SAFE
        UNLOCK(dst);
#endif
        return -1;
      }
    }
    if (parts[i] && parts[i]->len) {
      if (FUNC(str_append)(dst, parts[i]->buf, parts[i]->len) != 0) {
#ifdef DS_THREAD_SAFE
        UNLOCK(dst);
#endif
        return -1;
      }
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(dst);
#endif
  return 0;
}

int FUNC(str_starts_with)(ds_str_t *s, const void *prefix, size_t n) {
  if (!s || !prefix) return 0;
  if (s->len < n) return 0;
  return memcmp(s->buf, prefix, n) == 0;
}

int FUNC(str_ends_with)(ds_str_t *s, const void *suffix, size_t n) {
  if (!s || !suffix) return 0;
  if (s->len < n) return 0;
  return memcmp(s->buf + (s->len - n), suffix, n) == 0;
}

long FUNC(str_count)(ds_str_t *s, const void *needle, size_t n) {
  size_t pos = 0u; long c = 0; long at;
  if (!s || !needle || n == 0u) return 0;
  for (;;) {
    at = FUNC(str_find_twoway)(s, needle, n, pos);
    if (at < 0) break;
    ++c; pos = (size_t)at + n;
    if (pos > s->len) break;
  }
  return c;
}

int FUNC(str_replace_all)(ds_str_t *s, const void *from, size_t nfrom, const void *to, size_t nto) {
  size_t pos = 0u; long at;
  if (!s || !from || nfrom == 0u) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  while (1) {
    at = FUNC(str_find_twoway)(s, from, nfrom, pos);
    if (at < 0) break;
    if (FUNC(str_erase)(s, (size_t)at, nfrom) != 0) { 
#ifdef DS_THREAD_SAFE
      UNLOCK(s);
#endif
      return -1; 
    }
    if (nto) {
      if (FUNC(str_insert)(s, (size_t)at, to, nto) != 0) { 
#ifdef DS_THREAD_SAFE
        UNLOCK(s);
#endif
        return -1; 
      }
      pos = (size_t)at + nto;
    } else {
      pos = (size_t)at;
    }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_replace_all_builder)(ds_str_t *s,
                                  const void *from, size_t nfrom,
                                  const void *to,   size_t nto) {
  size_t pos = 0u;
  long at;
  ds_str_t *out;
  size_t last = 0u;

  if (!s || !from || nfrom == 0u) return -1;

#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  out = FUNC(str_create_alloc)(s->allocator, s->deallocator);
  if (!out) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }

  if (s->len > 0u) {
    if (FUNC(str_reserve)(out, s->len) != 0) { FUNC(str_destroy)(out);
#ifdef DS_THREAD_SAFE
      UNLOCK(s);
#endif
      return -1;
    }
  }

  while (1) {
    at = FUNC(str_find_twoway)(s, from, nfrom, pos);
    if (at < 0) {
      if (last < s->len) {
        if (FUNC(str_append)(out, s->buf + last, s->len - last) != 0) {
          FUNC(str_destroy)(out);
#ifdef DS_THREAD_SAFE
          UNLOCK(s);
#endif
          return -1;
        }
      }
      break;
    }

    if ((size_t)at > last) {
      if (FUNC(str_append)(out, s->buf + last, (size_t)at - last) != 0) {
        FUNC(str_destroy)(out);
#ifdef DS_THREAD_SAFE
        UNLOCK(s);
#endif
        return -1;
      }
    }

    if (nto) {
      if (FUNC(str_append)(out, to, nto) != 0) {
        FUNC(str_destroy)(out);
#ifdef DS_THREAD_SAFE
        UNLOCK(s);
#endif
        return -1;
      }
    }

    pos  = (size_t)at + nfrom;
    last = pos;
    if (pos > s->len) pos = s->len;
  }

  if (s->buf) s->deallocator(s->buf);
  s->buf = out->buf;
  s->len = out->len;
  s->cap = out->cap;
  out->buf = NULL; out->len = 0u; out->cap = 0u;
  FUNC(str_destroy)(out);
  if (s->buf) s->buf[s->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}
