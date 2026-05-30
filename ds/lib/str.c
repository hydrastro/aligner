#include "str.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>

int ds__ensure_exact(ds_str_t *s, size_t need_cap) {
  char *nbuf;
  size_t bytes;
  if (!s) return -1;
  if (s->cap >= need_cap) return 0;
  if (need_cap > SIZE_MAX - 1u) return -1;
  bytes = need_cap + 1u;
  if (s->buf == NULL) {
    nbuf = (char *)s->allocator(bytes);
    if (!nbuf) return -1;
    nbuf[0] = '\0';
  } else {
    char *tmp = (char *)s->allocator(bytes);
    size_t copy = s->len;
    if (!tmp) return -1;
    if (copy) memcpy(tmp, s->buf, copy);
    tmp[copy] = '\0';
    s->deallocator(s->buf);
    nbuf = tmp;
  }
  s->buf = nbuf;
  s->cap = need_cap;
  return 0;
}

#ifdef DS_THREAD_SAFE
void ds__lock2(ds_str_t *a, ds_str_t *b) {
  uintptr_t ia, ib;
  if (a == b) { if (a) LOCK(a); return; }
  if (!a && !b) return;
  if (!a) { LOCK(b); return; }
  if (!b) { LOCK(a); return; }

  ia = (uintptr_t)(const void*)a;
  ib = (uintptr_t)(const void*)b;
  if (ia < ib) { LOCK(a); LOCK(b); }
  else         { LOCK(b); LOCK(a); }
}

void ds__unlock2(ds_str_t *a, ds_str_t *b) {
  uintptr_t ia, ib;
  if (a == b) { if (a) UNLOCK(a); return; }
  if (!a && !b) return;
  if (!a) { UNLOCK(b); return; }
  if (!b) { UNLOCK(a); return; }

  ia = (uintptr_t)(const void*)a;
  ib = (uintptr_t)(const void*)b;
  if (ia < ib) { UNLOCK(b); UNLOCK(a); }
  else         { UNLOCK(a); UNLOCK(b); }
}
#endif

size_t ds__grow_cap(size_t cur, size_t need) {
  size_t cap = cur ? cur : 16u;
  while (cap < need) {
    if (cap > SIZE_MAX / 2u) {
      cap = need;
      break;
    }
    cap *= 2u;
  }
  return cap;
}

int ds__ensure(ds_str_t *s, size_t need_cap) {
  char *nbuf;
  size_t bytes;
  if (!s) return -1;
  if (s->cap >= need_cap) return 0;
  need_cap = ds__grow_cap(s->cap, need_cap);
  if (need_cap > SIZE_MAX - 1u) return -1;
  bytes = need_cap + 1u;
  if (s->buf == NULL) {
    nbuf = (char *)s->allocator(bytes);
    if (!nbuf) return -1;
    nbuf[0] = '\0';
  } else {
    char *tmp = (char *)s->allocator(bytes);
    size_t copy = s->len;
    if (!tmp) return -1;
    if (copy) memcpy(tmp, s->buf, copy);
    tmp[copy] = '\0';
    s->deallocator(s->buf);
    nbuf = tmp;
  }
  s->buf = nbuf;
  s->cap = need_cap;
  return 0;
}

ds_str_t *FUNC(str_create)(void) {
  return FUNC(str_create_alloc)(malloc, free);
}

ds_str_t *FUNC(str_create_alloc)(void *(*allocator)(size_t), void (*deallocator)(void *)) {
  ds_str_t *s = (ds_str_t *)allocator(sizeof(ds_str_t));
  if (!s) return NULL;
  s->allocator = allocator;
  s->deallocator = deallocator;
  s->buf = NULL;
  s->len = 0u;
  s->cap = 0u;
#ifdef DS_THREAD_SAFE
  LOCK_INIT_RECURSIVE(s);
#endif
  return s;
}

ds_str_t *FUNC(str_with_capacity)(size_t cap) {
  ds_str_t *s = FUNC(str_create)();
  if (!s) return NULL;
  if (cap == 0) {
    if (ds__ensure(s, 0) != 0) { FUNC(str_destroy)(s); return NULL; }
    if (!s->buf) {
      char *p = (char*)s->allocator(1u);
      if (!p) { FUNC(str_destroy)(s); return NULL; }
      p[0] = '\0'; s->buf = p; s->cap = 0;
    }
    return s;
  }
  if (ds__ensure(s, cap) != 0) { FUNC(str_destroy)(s); return NULL; }
  return s;
}

ds_str_t *FUNC(str_from)(const void *data, size_t n) {
  ds_str_t *s = FUNC(str_with_capacity)(n);
  if (!s) return NULL;
  if (n) memcpy(s->buf, data, n);
  s->len = n;
  s->buf[n] = '\0';
  return s;
}

void FUNC(str_destroy)(ds_str_t *s) {
  if (!s) return;
#ifdef DS_THREAD_SAFE
  LOCK_DESTROY(s);
#endif
  if (s->buf) s->deallocator(s->buf);
  s->deallocator(s);
}

void FUNC(str_clear)(ds_str_t *s) {
  if (!s) return;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  s->len = 0u;
  if (s->buf) s->buf[0] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
}

void FUNC(str_delete)(ds_str_t *s) {
  FUNC(str_clear)(s);
}

int FUNC(str_reserve)(ds_str_t *s, size_t need) {
  int r;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  r = ds__ensure(s, need);
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return r;
}

int FUNC(str_shrink_to_fit)(ds_str_t *s) {
  char *nbuf;
  size_t bytes;
  if (!s) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  if (s->cap == s->len) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return 0;
  }
  if (s->len == SIZE_MAX) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  bytes = s->len + 1u;
  nbuf = (char*)s->allocator(bytes);
  if (!nbuf) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  if (s->len) memcpy(nbuf, s->buf, s->len);
  nbuf[s->len] = '\0';
  if (s->buf) s->deallocator(s->buf);
  s->buf = nbuf;
  s->cap = s->len;
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_append)(ds_str_t *s, const void *data, size_t n) {
  size_t need_len;
  if (!s || (!data && n)) return -1;
  if (n == 0u) return 0;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  need_len = s->len + n;
  if (need_len < s->len) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  if (ds__ensure(s, need_len) != 0) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  memcpy(s->buf + s->len, data, n);
  s->len = need_len;
  s->buf[s->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_append_cstr)(ds_str_t *s, const char *cstr) {
  size_t n = cstr ? strlen(cstr) : 0u;
  return FUNC(str_append)(s, cstr, n);
}

int FUNC(str_pushc)(ds_str_t *s, int c) {
  unsigned char ch = (unsigned char)(c & 0xFF);
  return FUNC(str_append)(s, &ch, 1u);
}

int FUNC(str_insert)(ds_str_t *s, size_t pos, const void *data, size_t n) {
  size_t len;
  if (!s || (!data && n)) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  len = s->len;
  if (pos > len) pos = len;
  if (n == 0u) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return 0;
  }
  if (ds__ensure(s, len + n) != 0) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  memmove(s->buf + pos + n, s->buf + pos, len - pos);
  memcpy(s->buf + pos, data, n);
  s->len = len + n;
  s->buf[s->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_erase)(ds_str_t *s, size_t pos, size_t n) {
  size_t len;
  if (!s) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  len = s->len;
  if (pos > len) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return 0;
  }
  if (n > len - pos) n = len - pos;
  if (n == 0u) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return 0;
  }
  memmove(s->buf + pos, s->buf + pos + n, len - pos - n);
  s->len = len - n;
  s->buf[s->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

ds_str_t *FUNC(str_clone)(ds_str_t *s) {
  ds_str_t *n;
  if (!s) return NULL;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  n = FUNC(str_create_alloc)(s->allocator, s->deallocator);
  if (!n) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return NULL;
  }
  if (ds__ensure(n, s->len) != 0) {
    FUNC(str_destroy)(n);
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return NULL;
  }
  if (s->len) memcpy(n->buf, s->buf, s->len);
  n->len = s->len;
  n->buf[n->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return n;
}

ds_str_t *FUNC(str_clone_cap)(ds_str_t *s) {
  ds_str_t *n;
  size_t want;
  if (!s) {return NULL;}
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  n = FUNC(str_create_alloc)(s->allocator, s->deallocator);
  if (!n) { 
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return NULL; 
  }
  want = s->cap ? s->cap : s->len;
  if (want && ds__ensure(n, want) != 0) { FUNC(str_destroy)(n);
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return NULL; }
  if (s->len) memcpy(n->buf, s->buf, s->len);
  n->len = s->len;
  if (n->buf) n->buf[n->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return n;
}

int FUNC(str_cmp)(ds_str_t *a, ds_str_t *b) {
  size_t la, lb, lm;
  int r;
#ifdef DS_THREAD_SAFE
  if (a == b) return 0;
  ds__lock2(a, b);
#else
  if (a == b) return 0;
#endif
  la = a ? a->len : 0u;
  lb = b ? b->len : 0u;
  lm = la < lb ? la : lb;
  r = lm ? memcmp(a ? a->buf : "", b ? b->buf : "", lm) : 0;
  if (r != 0) {
#ifdef DS_THREAD_SAFE
    ds__unlock2(a, b);
#endif
    return r;
  }
  if (la < lb) { 
#ifdef DS_THREAD_SAFE
    ds__unlock2(a, b);
#endif
    return -1; 
  }
  if (la > lb) { 
#ifdef DS_THREAD_SAFE
    ds__unlock2(a, b);
#endif
    return 1; 
  }
#ifdef DS_THREAD_SAFE
  ds__unlock2(a, b);
#endif
  return 0;
}

int FUNC(str_eq)(ds_str_t *a, ds_str_t *b) {
  return FUNC(str_cmp)(a, b) == 0;
}

long FUNC(str_find)(ds_str_t *s, const void *needle, size_t n, size_t start) {
  const unsigned char *h;
  const unsigned char *nd = (const unsigned char *)needle;
  size_t i, len;
  const void *p;
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
  h = (const unsigned char *)s->buf;
  if (n == 1u) {
    p = memchr(h + start, *nd, len - start);
    ret = p ? (long)((const unsigned char*)p - h) : -1;
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return ret;
  }
  for (i = start; i + n <= len; ++i) {
    if (memcmp(h + i, nd, n) == 0) { ret = (long)i; break; }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return ret;
}

long FUNC(str_rfind)(ds_str_t *s, const void *needle, size_t n, size_t start) {
  const unsigned char *h, *nd = (const unsigned char*)needle;
  size_t i, len;
  long ret = -1;
  if (!s || !needle || n == 0u) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  len = s->len;
  if (start > len) start = len;
  if (start < n) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  h = (const unsigned char*)s->buf;
  for (i = start - n + 1; i-- > 0;) {
    if (memcmp(h + i, nd, n) == 0) { ret = (long)i; break; }
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return ret;
}

void FUNC(str_to_upper)(ds_str_t *s) {
  size_t i, n;
  if (!s) return;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  n = s->len;
  for (i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)s->buf[i];
    if (c >= 'a' && c <= 'z') s->buf[i] = (char)(c - 'a' + 'A');
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
}

void FUNC(str_to_lower)(ds_str_t *s) {
  size_t i, n;
  if (!s) return;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  n = s->len;
  for (i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)s->buf[i];
    if (c >= 'A' && c <= 'Z') s->buf[i] = (char)(c - 'A' + 'a');
  }
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
}

void FUNC(str_swap)(ds_str_t *a, ds_str_t *b) {
  char *tbuf; size_t tlen, tcap;
  void *(*talloc)(size_t); void (*tfree)(void *);
  if (!a || !b || a == b) { return; }
#ifdef DS_THREAD_SAFE
  ds__lock2(a, b);
#endif
  tbuf = a->buf; a->buf = b->buf; b->buf = tbuf;
  tlen = a->len; a->len = b->len; b->len = tlen;
  tcap = a->cap; a->cap = b->cap; b->cap = tcap;
  talloc = a->allocator; a->allocator = b->allocator; b->allocator = talloc;
  tfree  = a->deallocator; a->deallocator = b->deallocator; b->deallocator = tfree;
#ifdef DS_THREAD_SAFE
  ds__unlock2(a, b);
#endif
}

int FUNC(str_clear_freebuf)(ds_str_t *s) {
  if (!s) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  if (s->buf) s->deallocator(s->buf);
  s->buf = NULL; s->len = 0; s->cap = 0;
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_append_vfmt)(ds_str_t *s, const char *fmt, va_list ap) {
  va_list ap2;
  int need;
  if (!s || !fmt) return -1;

  DS_VA_COPY(ap2, ap);
  need = vsnprintf(NULL, 0, fmt, ap2);
  va_end(ap2);
  if (need < 0) return -1;

#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  if (ds__ensure(s, s->len + (size_t)need) != 0) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  if (vsnprintf(s->buf + s->len, (size_t)need + 1, fmt, ap) != need) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  s->len += (size_t)need;
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_reserve_exact)(ds_str_t *s, size_t cap) {
  int r;
  if (!s) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  r = ds__ensure_exact(s, cap);
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return r;
}

int FUNC(str_assign)(ds_str_t *s, const void *data, size_t n) {
  int r;
  if (!s) return -1;
  if (!data && n) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  r = ds__ensure(s, n);
  if (r != 0) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  if (n) memcpy(s->buf, data, n);
  s->len = n;
  s->buf[s->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

int FUNC(str_pop)(ds_str_t *s, int *out_ch) {
  unsigned char c;
  if (!s) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  if (s->len == 0u) {
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return -1;
  }
  c = (unsigned char)s->buf[s->len - 1u];
  s->len -= 1u;
  s->buf[s->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  if (out_ch) *out_ch = (int)c;
  return 0;
}

ds_str_t *FUNC(str_slice)(ds_str_t *s, size_t pos, size_t n) {
  ds_str_t *out;
  size_t len, start, take;
  if (!s) return NULL;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  len = s->len;
  start = pos > len ? len : pos;
  take = (n > len - start) ? (len - start) : n;
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  out = FUNC(str_with_capacity)(take);
  if (!out) return NULL;

#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  len = s->len;
  if (start > len) start = len;
  if (take > len - start) take = len - start;
  if (take) memcpy(out->buf, s->buf + start, take);
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif

  out->len = take;
  out->buf[take] = '\0';
  return out;
}

char *FUNC(str_detach)(ds_str_t *s, size_t *len_out, size_t *cap_out) {
  char *p;
  if (!s) return NULL;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  p = s->buf;
  if (len_out) *len_out = s->len;
  if (cap_out) *cap_out = s->cap;
  s->buf = NULL;
  s->len = 0u;
  s->cap = 0u;
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return p;
}

int FUNC(str_adopt)(ds_str_t *s, char *buf, size_t len, size_t cap) {
  if (!s) return -1;
  if (!buf && (len != 0u || cap != 0u)) return -1;
  if (buf && cap < len) return -1;
#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  if (s->buf) s->deallocator(s->buf);
  s->buf = buf;
  s->len = len;
  s->cap = cap;
  if (s->buf) s->buf[s->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}

void FUNC(str_free_external)(ds_str_t *owner, void *p) {
  if (!owner || !p) return;
  owner->deallocator(p);
}

int FUNC(str_adopt_safe)(ds_str_t *s, char *buf, size_t len, size_t cap, int has_room_for_nul) {
  if (!s) return -1;
  if (!buf && (len != 0u || cap != 0u)) return -1;
  if (buf && cap < len) return -1;

#ifdef DS_THREAD_SAFE
  LOCK(s);
#endif
  if (!buf) {
    if (s->buf) s->deallocator(s->buf);
    s->buf = NULL; s->len = 0u; s->cap = 0u;
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return 0;
  }

  if (!has_room_for_nul && len == cap) {
    if (ds__ensure_exact(s, len) != 0) {
#ifdef DS_THREAD_SAFE
      UNLOCK(s);
#endif
      return -1;
    }
    if (len) memcpy(s->buf, buf, len);
    s->len = len;
    s->buf[len] = '\0';
#ifdef DS_THREAD_SAFE
    UNLOCK(s);
#endif
    return 0;
  }

  if (s->buf) s->deallocator(s->buf);
  s->buf = buf;
  s->len = len;
  s->cap = cap;
  s->buf[s->len] = '\0';
#ifdef DS_THREAD_SAFE
  UNLOCK(s);
#endif
  return 0;
}
