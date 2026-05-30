#include "unicode_runtime.h"
#include "str_unicode.h"
#include "str.h"
#include "str_io.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "unicode_tables.h"

int ucd_do_init(void) { return 0; }
int ucd_init_once(void) { return 0; }
void ensure_ucd_once_lock(void) {}

int u8_next(const char *buf, size_t len, size_t *ioff, unsigned long *out_cp) {
  return FUNC(str_u8_next)(buf, len, ioff, out_cp, DS_U8_STRICT);
}
int u8_push(ds_str_t *s, unsigned long cp) { return FUNC(str_u8_push_cp)(s, cp); }

int is_Hangul_S(unsigned long cp){ return (cp>=SBase && cp<SBase+SCount); }
int is_Hangul_L(unsigned long cp){ return (cp>=LBase && cp<LBase+LCount); }
int is_Hangul_V(unsigned long cp){ return (cp>=VBase && cp<VBase+VCount); }
int is_Hangul_T(unsigned long cp){ return (cp>TBase && cp<=TBase+TCount-1); }

void decompose_Hangul(unsigned long s, void *out_vec_void) {
  u32vec_like *out = (u32vec_like*)out_vec_void;
  unsigned long SIndex = s - SBase;
  unsigned long L = LBase + SIndex / NCount;
  unsigned long V = VBase + (SIndex % NCount) / TCount;
  unsigned long T = TBase + (SIndex % TCount);
  out->push(out, L);
  out->push(out, V);
  if (T != TBase) out->push(out, T);
}

int compose_Hangul(unsigned long *LVT, unsigned long next, size_t *consumed){
  unsigned long s = *LVT;
  if (is_Hangul_L(s) && is_Hangul_V(next)) {
    unsigned long LIndex = s - LBase;
    unsigned long VIndex = next - VBase;
    *LVT = SBase + (LIndex * VCount + VIndex) * TCount;
    *consumed = 1u; return 1;
  }
  if (is_Hangul_S(s) && ((next > TBase) && (next <= TBase + TCount - 1))) {
    unsigned long TIndex = next - TBase;
    *LVT = s + TIndex; *consumed = 1u; return 1;
  }
  return 0;
}

int u32vec_push(u32vec_like *v, unsigned long x) {
  unsigned long *np;
  size_t nc;
  if (v->n == v->cap) {
    nc = v->cap ? (v->cap * 2u) : 8u;
    np = (unsigned long*)realloc(v->p, nc * sizeof(unsigned long));
    if (!np) return -1;
    v->p = np; v->cap = nc;
  }
  v->p[v->n++] = x;
  return 0;
}
void u32vec_free(u32vec_like *v) {
  if (v->p) free(v->p);
  v->p = NULL; v->n = 0; v->cap = 0;
}

unsigned long get_ccc(unsigned long cp) {
  size_t lo = 0, hi = U_CCC_LEN;
  while (lo < hi) {
    size_t mid = lo + ((hi - lo) >> 1);
    unsigned long k = U_CCC_KEYS[mid];
    if (cp < k) hi = mid;
    else if (cp > k) lo = mid + 1;
    else return U_CCC_VALS[mid];
  }
  return 0ul;
}

int decompose_cp(unsigned long cp, int compat, void *out_vec_void) {
  u32vec_like *out = (u32vec_like*)out_vec_void;
  size_t lo, hi;
  if (is_Hangul_S(cp)) { decompose_Hangul(cp, out); return 0; }

  lo = 0; hi = U_DECOMP_LEN;
  while (lo < hi) {
    size_t mid = lo + ((hi - lo) >> 1);
    unsigned long k = U_DECOMP_KEYS[mid];
    if (cp < k) hi = mid;
    else if (cp > k) lo = mid + 1;
    else {
      unsigned int cnt = U_DECOMP_CNT[mid];
      unsigned int is_compat = U_DECOMP_ISCOMPAT[mid];
      unsigned int off = U_DECOMP_OFF[mid];
      unsigned int i;
      if (!compat && is_compat) {
        return out->push(out, cp);
      }
      for (i = 0; i < cnt; ++i) {
        unsigned long d = U_DECOMP_POOL[off + i];
        if (decompose_cp(d, compat, out) != 0) return -1;
      }
      return 0;
    }
  }
  return out->push(out, cp);
}

void reorder_ccc(void *vec_void) {
  u32vec_like *v = (u32vec_like*)vec_void;
  size_t i;
  for (i = 1; i < v->n; ++i) {
    unsigned long x = v->p[i], cx = get_ccc(x);
    size_t j = i;
    while (j > 0) {
      unsigned long y = v->p[j-1], cy = get_ccc(y);
      if (cy <= cx) break;
      v->p[j] = y; --j;
    }
    v->p[j] = x;
  }
}

#ifndef U_COMP_KEYS_A
# ifdef U_COMP_A
#  define U_COMP_KEYS_A U_COMP_A
#  define U_COMP_KEYS_B U_COMP_B
# endif
#endif

int ds__pair_cmp(unsigned long a1, unsigned long b1,
                 unsigned long a2, unsigned long b2) {
  if (a1 < a2){ return -1; }if (a1 > a2){ return 1;}
  if (b1 < b2){ return -1; }if (b1 > b2){ return 1;}
  return 0;
}

size_t ds__comp_find_pair(unsigned long a, unsigned long b) {
  size_t lo = 0, hi = U_COMP_LEN;
  while (lo < hi) {
    size_t mid = lo + ((hi - lo) >> 1);
    unsigned long ka = U_COMP_KEYS_A[mid];
    unsigned long kb = U_COMP_KEYS_B[mid];
    int cmp = ds__pair_cmp(a, b, ka, kb);
    if (cmp < 0) hi = mid;
    else if (cmp > 0) lo = mid + 1;
    else return mid;
  }
  return (size_t)-1;
}


void compose_vec(void *vec_void) {
  u32vec_like *v = (u32vec_like*)vec_void;
  size_t r, w;

  if (v->n == 0) return;

  r = 0; w = 0;
  v->p[w++] = v->p[r++];

  while (r < v->n) {
    unsigned long b = v->p[r];
    unsigned long cccb = get_ccc(b);

    {
      size_t consumed = 0u;
      unsigned long a0 = v->p[w-1];
      if (compose_Hangul(&a0, b, &consumed)) {
        v->p[w-1] = a0;
        r += consumed;
        continue;
      }
    }

    if (cccb == 0ul) {
      unsigned long a = v->p[w-1];
      size_t idx = ds__comp_find_pair(a, b);
      if (idx != (size_t)-1) {
        v->p[w-1] = U_COMP_VALS[idx];
        r++;
        continue;
      }
    } else {
      size_t j = w;
      while (j > 0) {
        unsigned long a2 = v->p[j-1];
        unsigned long ccca2 = get_ccc(a2);
        if (ccca2 == 0ul) {
          size_t idx = ds__comp_find_pair(a2, b);
          if (idx != (size_t)-1) {
            v->p[j-1] = U_COMP_VALS[idx];
            r++;
            goto next_char;
          }
          break;
        }
        if (ccca2 >= cccb) break;
        --j;
      }
    }

    v->p[w++] = v->p[r++];
  next_char:
    ;
  }

  v->n = w;
}

int fold_cp(unsigned long cp, void *out_vec_void) {
  u32vec_like *out = (u32vec_like*)out_vec_void;
  size_t lo = 0, hi = U_FOLD_LEN;
  while (lo < hi) {
    size_t mid = lo + ((hi - lo) >> 1);
    unsigned long k = U_FOLD_KEYS[mid];
    if (cp < k) hi = mid;
    else if (cp > k) lo = mid + 1;
    else {
      unsigned int cnt = U_FOLD_CNT[mid];
      unsigned int off = U_FOLD_OFF[mid];
      unsigned int i;
      for (i=0;i<cnt;++i) {
        if (out->push(out, U_FOLD_POOL[off + i]) != 0) return -1;
      }
      return 0;
    }
  }
  return out->push(out, cp);
}

unsigned long ds__map_simple(const unsigned long *keys, const unsigned long *vals, size_t n, unsigned long cp) {
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = lo + ((hi - lo) >> 1);
    unsigned long k = keys[mid];
    if (cp < k) hi = mid;
    else if (cp > k) lo = mid + 1;
    else return vals[mid];
  }
  return cp;
}

unsigned long gb_get(unsigned long cp) {
  size_t lo = 0, hi = U_GB_LEN;
  while (lo < hi) {
    size_t mid = lo + ((hi - lo) >> 1);
    if (cp < U_GB_RANGES[mid].start) hi = mid;
    else if (cp > U_GB_RANGES[mid].end) lo = mid + 1;
    else return U_GB_RANGES[mid].prop;
  }
  return GB_Other;
}

int is_EP(unsigned long cp) {
  size_t lo = 0, hi = U_EP_LEN;
  while (lo < hi) {
    size_t mid = lo + ((hi - lo) >> 1);
    if (cp < U_EP_RANGES[mid].start) hi = mid;
    else if (cp > U_EP_RANGES[mid].end) lo = mid + 1;
    else return 1;
  }
  return 0;
}


int ds__ucd_normalize(ds_str_t *dst, const ds_str_t *src, int compat, int compose) {
  u32vec_like tmp;
  size_t i;
  unsigned long cp;

  if (!dst || !src) return -1;

  tmp.p = NULL; tmp.n = 0; tmp.cap = 0; tmp.push = u32vec_push;

  i = 0;
  while (u8_next(src->buf, src->len, &i, &cp) == 1) {
    if (decompose_cp(cp, compat, &tmp) != 0) { u32vec_free(&tmp); return -1; }
  }
  reorder_ccc(&tmp);
  if (compose) compose_vec(&tmp);

  FUNC(str_clear)(dst);
  for (i=0;i<tmp.n;++i) if (u8_push(dst, tmp.p[i]) != 0) { u32vec_free(&tmp); return -1; }
  u32vec_free(&tmp);
  return 0;
}

int ds__ucd_casefold(ds_str_t *dst, const ds_str_t *src) {
  u32vec_like out;
  size_t i;
  unsigned long cp;

  if (!dst || !src) return -1;

  out.p = NULL; out.n = 0; out.cap = 0; out.push = u32vec_push;

  i = 0;
  while (u8_next(src->buf, src->len, &i, &cp)==1) {
    if (fold_cp(cp, &out) != 0) { u32vec_free(&out); return -1; }
  }
  FUNC(str_clear)(dst);
  for (i=0;i<out.n;++i) if (u8_push(dst, out.p[i]) != 0) { u32vec_free(&out); return -1; }
  u32vec_free(&out);
  return 0;
}

int ds__ucd_tolower(ds_str_t *dst, const ds_str_t *src) {
  size_t i = 0;
  unsigned long cp;

  if (!dst || !src) return -1;

  FUNC(str_clear)(dst);
  while (u8_next(src->buf, src->len, &i, &cp)==1) {
    cp = ds__map_simple(U_TOLOWER_KEYS, U_TOLOWER_VALS, U_TOLOWER_LEN, cp);
    if (u8_push(dst, cp) != 0) return -1;
  }
  return 0;
}

int ds__ucd_toupper(ds_str_t *dst, const ds_str_t *src) {
  size_t i = 0;
  unsigned long cp;

  if (!dst || !src) return -1;

  FUNC(str_clear)(dst);
  while (u8_next(src->buf, src->len, &i, &cp)==1) {
    cp = ds__map_simple(U_TOUPPER_KEYS, U_TOUPPER_VALS, U_TOUPPER_LEN, cp);
    if (u8_push(dst, cp) != 0) return -1;
  }
  return 0;
}

long ds__ucd_grapheme_len(const ds_str_t *s) {
  ds__grapheme_iter_t it;
  long n = 0;
  size_t a, l;
  int r;

  if (!s) return -1;
  if (ds__grapheme_iter_init(&it, s->buf, s->len) != 0) return -1;
  while ((r = ds__grapheme_iter_next(&it, &a, &l)) == 1) ++n;
  if (r < 0) return -1;
  return n;
}


int ds__grapheme_iter_init(ds__grapheme_iter_t *it, const char *buf, size_t len) {
  if (!it) return -1;
  it->buf = buf;
  it->len = buf ? len : 0;
  it->i = 0;
  it->cur_start = 0;
  it->have_prev = 0;
  it->prev_prop = GB_Other;
  it->ri_run_len = 0;
  it->prev_is_zwj_ign_ext = 0;
  it->last_base_is_EP = 0;
  it->cp = 0;
  it->prop = GB_Other;
  it->break_here = 0;
  it->prev_is_Control = 0;
  it->cur_is_Control = 0;
  return 0;
}

int ds__grapheme_iter_next(ds__grapheme_iter_t *it, size_t *out_start, size_t *out_len) {
  while (1) {
    size_t cp_start;
    unsigned long prop;
    int break_here = 1;
    int r;

    if (!it || !it->buf) return -1;

    if (it->i >= it->len) {
      if (it->cur_start < it->len) {
        if (out_start) *out_start = it->cur_start;
        if (out_len)   *out_len   = it->len - it->cur_start;
        it->cur_start = it->len;
        return 1;
      }
      return 0;
    }

    cp_start = it->i;
    r = u8_next(it->buf, it->len, &it->i, &it->cp);
    if (r != 1) return -1;
    prop = gb_get(it->cp);

    if (!it->have_prev) {
      it->have_prev = 1;
      it->prev_prop = prop;
      it->cur_start = cp_start;

      it->last_base_is_EP = is_EP(it->cp);
      it->prev_is_zwj_ign_ext = 0;
      it->ri_run_len = (prop == GB_Regional_Indicator) ? 1 : 0;
      continue;
    }

    it->prev_is_Control = (it->prev_prop == GB_Control || it->prev_prop == GB_CR || it->prev_prop == GB_LF);
    it->cur_is_Control  = (prop == GB_Control || prop == GB_CR || prop == GB_LF);

    if (it->prev_prop == GB_CR && prop == GB_LF) break_here = 0;
    else if (it->prev_is_Control || it->cur_is_Control) break_here = 1;
    else if (it->prev_prop == GB_L && (prop == GB_L || prop == GB_V || prop == GB_LV || prop == GB_LVT)) break_here = 0;
    else if ((it->prev_prop == GB_LV || it->prev_prop == GB_V) && (prop == GB_V || prop == GB_T)) break_here = 0;
    else if ((it->prev_prop == GB_LVT || it->prev_prop == GB_T) && (prop == GB_T)) break_here = 0;
    else if (prop == GB_Extend) break_here = 0;
    else if (prop == GB_SpacingMark) break_here = 0;
    else if (it->prev_prop == GB_Prepend) break_here = 0;
    else if (is_EP(it->cp) && it->prev_is_zwj_ign_ext && it->last_base_is_EP) break_here = 0;
    else if (it->prev_prop == GB_Regional_Indicator && prop == GB_Regional_Indicator) {
      if ((it->ri_run_len % 2) == 1) break_here = 0;
    }

    if (break_here) {
      if (out_start) *out_start = it->cur_start;
      if (out_len)   *out_len   = cp_start - it->cur_start;

      it->cur_start = cp_start;
      it->prev_prop = prop;

      if (prop == GB_Regional_Indicator) it->ri_run_len += 1;
      else it->ri_run_len = 0;

      if (prop == GB_Extend) {
      } else if (prop == GB_ZWJ) {
        it->prev_is_zwj_ign_ext = 1;
      } else {
        it->last_base_is_EP = is_EP(it->cp);
        it->prev_is_zwj_ign_ext = 0;
      }
      return 1;
    }

    it->prev_prop = prop;
    if (prop == GB_Regional_Indicator) it->ri_run_len += 1;
    else it->ri_run_len = 0;

    if (prop == GB_Extend) {
    } else if (prop == GB_ZWJ) {
      it->prev_is_zwj_ign_ext = 1;
    } else {
      it->last_base_is_EP = is_EP(it->cp);
      it->prev_is_zwj_ign_ext = 0;
    }
  }
}


int ds__is_control_cc(unsigned long cp) {
  if ((cp <= 0x001Ful) || (cp >= 0x007Ful && cp <= 0x009Ful)) {
    return (cp != 0x09ul && cp != 0x0Aul && cp != 0x0Dul);
  }
  return 0;
}

int ds__is_default_ignorable(unsigned long cp) {
  if (cp == 0x00ADul || cp == 0x034Ful || cp == 0x061Cul ||
      cp == 0x115Ful || cp == 0x1160ul || cp == 0x17B4ul || cp == 0x17B5ul ||
      cp == 0x180Eul || (cp >= 0xFE00ul && cp <= 0xFE0Ful) || cp == 0xFEFFul || cp == 0xE0001ul) return 1;

  if ((cp >= 0x200Bul && cp <= 0x200Ful) ||
      (cp >= 0x202Aul && cp <= 0x202Eul) ||
      (cp >= 0x2060ul && cp <= 0x206Ful) ||
      (cp >= 0xFFF0ul && cp <= 0xFFFBul) ||
      (cp >= 0x1BCA0ul && cp <= 0x1BCA3ul) ||
      (cp >= 0x1D173ul && cp <= 0x1D17Aul) ||
      (cp >= 0xE0020ul && cp <= 0xE007Ful)) return 1;

  return 0;
}

unsigned long ds__lump(unsigned long cp) {
  switch (cp) {
    case 0x2018ul: case 0x2019ul: case 0x201Aul: case 0x2039ul: return '\'';
    case 0x201Cul: case 0x201Dul: case 0x201Eul: case 0x00ABul: case 0x00BBul: return '"';
    case 0x2010ul: case 0x2011ul: case 0x2012ul: case 0x2013ul: case 0x2014ul: case 0x2015ul: return '-';
    case 0x2026ul: return '.';
    case 0x00B7ul: case 0x2022ul: case 0x2219ul: return '*';
    case 0x2215ul: case 0x29F8ul: case 0xFF0Ful: case 0x2044ul: return '/';
    case 0x29F9ul: case 0xFF3Cul: return '\\';
    default: return cp;
  }
}

int ds__map_newlines(ds_str_t *out, const ds_str_t *src, unsigned int flags) {
  size_t i;
  int rc;
  int to_lf = (flags & DS_MAP_NLF2LF) != 0;
  int to_ls = (flags & DS_MAP_NLF2LS) != 0;
  int to_ps = (flags & DS_MAP_NLF2PS) != 0;
  unsigned long target = to_ls ? 0x2028ul : (to_ps ? 0x2029ul : 0x0Aul);

  if (!(to_lf || to_ls || to_ps)) {
    return FUNC(str_assign)(out, src->buf, src->len);
  }

  i = 0;
  rc = 0;
  FUNC(str_clear)(out);

  while (1) {
    size_t prev_i = i;
    unsigned long cp = 0;
    int r = FUNC(str_u8_next)(src->buf, src->len, &i, &cp, DS_U8_STRICT);
    if (r < 0) return -1;
    if (r == 0) break;

    if (cp == 0x0Dul) {
      size_t j = i;
      unsigned long next = 0;
      unsigned char enc[4]; size_t k;

      (void)FUNC(str_u8_next)(src->buf, src->len, &j, &next, DS_U8_STRICT);
      k = ds__encode_one(target, enc);
      if (next == 0x0Aul) { if (FUNC(str_append)(out, enc, k) != 0) return -1; i = j; continue; }
      else { if (FUNC(str_append)(out, enc, k) != 0) return -1; continue; }
    }

    if (cp == 0x0085ul || cp == 0x2028ul || cp == 0x2029ul) {
      unsigned char enc2[4]; size_t k2 = ds__encode_one(target, enc2);
      if (FUNC(str_append)(out, enc2, k2) != 0) return -1;
      continue;
    }

    if (prev_i < i) {
      if (FUNC(str_append)(out, src->buf + prev_i, i - prev_i) != 0) return -1;
    }
  }
  return rc;
}

int ds__is_mark(unsigned long cp) { return get_ccc(cp) != 0ul; }

int ds__filter_and_lump(ds_str_t *out, const ds_str_t *src, unsigned int flags) {
  size_t j = 0;
  FUNC(str_clear)(out);

  while (j < src->len) {
    size_t adv;
    unsigned long cp;
    if (ds__decode_one((const unsigned char*)src->buf + j, src->len - j, &adv, &cp, DS_U8_STRICT) <= 0) {
      unsigned char enc[4]; size_t k = ds__encode_one(DS_U8_REPLACEMENT_CHAR, enc);
      if (FUNC(str_append)(out, enc, k) != 0) return -1;
      j += 1;
      continue;
    }

    if ((flags & DS_MAP_IGNORE) && ds__is_default_ignorable(cp)) { j += adv; continue; }
    if ((flags & DS_MAP_STRIPCC) && ds__is_control_cc(cp)) { j += adv; continue; }
    if ((flags & DS_MAP_STRIPMARK) && ds__is_mark(cp)) { j += adv; continue; }

    if (flags & DS_MAP_LUMP) cp = ds__lump(cp);

    {
      unsigned char enc2[4]; size_t k2 = ds__encode_one(cp, enc2);
      if (FUNC(str_append)(out, enc2, k2) != 0) return -1;
    }
    j += adv;
  }
  return 0;
}

int ds__insert_charbound(ds_str_t *out, const ds_str_t *src) {
  ds_u8_grapheme_iter_t git;
  size_t a, n;
  int r;
  FUNC(str_clear)(out);
  if (FUNC(str_u8_grapheme_iter_init)(&git, src) != 0) return -1;
  while ((r = FUNC(str_u8_grapheme_next)(&git, &a, &n)) == 1) {
    unsigned char enc[4]; size_t k = ds__encode_one(0xFFul, enc);
    if (FUNC(str_append)(out, enc, k) != 0) return -1;
    if (FUNC(str_append)(out, src->buf + a, n) != 0) return -1;
  }
  return (r < 0) ? -1 : 0;
}
