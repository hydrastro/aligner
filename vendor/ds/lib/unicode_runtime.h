#ifndef DS_UNICODE_RUNTIME_H
#define DS_UNICODE_RUNTIME_H

#include "str.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct u32vec_like {
  unsigned long *p;
  size_t n, cap;
  int (*push)(struct u32vec_like *v, unsigned long x);
} u32vec_like;

int u32vec_push(u32vec_like *v, unsigned long x);
void u32vec_free(u32vec_like *v);

typedef enum {
  DS_MAP_NONE = 0,

  DS_MAP_DECOMPOSE = 1 << 0,
  DS_MAP_COMPOSE = 1 << 1,
  DS_MAP_COMPAT = 1 << 2,

  DS_MAP_CASEFOLD = 1 << 3,
  DS_MAP_TOLOWER = 1 << 4,
  DS_MAP_TOUPPER = 1 << 5,

  DS_MAP_IGNORE = 1 << 6,
  DS_MAP_STRIPCC = 1 << 7,
  DS_MAP_STRIPMARK = 1 << 8,

  DS_MAP_NLF2LF = 1 << 9,
  DS_MAP_NLF2LS = 1 << 10,
  DS_MAP_NLF2PS = 1 << 11,

  DS_MAP_CHARBOUND = 1 << 12,
  DS_MAP_LUMP = 1 << 13
} ds_u8_map_flags;

int ds__ucd_normalize(ds_str_t *dst, const ds_str_t *src, int compat,
                      int compose);
int ds__ucd_casefold(ds_str_t *dst, const ds_str_t *src);
int ds__ucd_tolower(ds_str_t *dst, const ds_str_t *src);
int ds__ucd_toupper(ds_str_t *dst, const ds_str_t *src);
long ds__ucd_grapheme_len(const ds_str_t *s);

enum {
  GB_Other = 0,
  GB_CR,
  GB_LF,
  GB_Control,
  GB_Extend,
  GB_ZWJ,
  GB_SpacingMark,
  GB_Prepend,
  GB_Regional_Indicator,
  GB_L,
  GB_V,
  GB_T,
  GB_LV,
  GB_LVT
};

typedef struct {
  const char *buf;
  size_t len;
  size_t i;
  size_t cur_start;
  int have_prev;

  unsigned long prev_prop;
  int ri_run_len;
  int prev_is_zwj_ign_ext;
  int last_base_is_EP;

  unsigned long cp, prop;
  int break_here;
  int prev_is_Control, cur_is_Control;
} ds__grapheme_iter_t;

int ds__grapheme_iter_init(ds__grapheme_iter_t *it, const char *buf,
                           size_t len);
int ds__grapheme_iter_next(ds__grapheme_iter_t *it, size_t *out_start,
                           size_t *out_len);

int ucd_do_init(void);
int ucd_init_once(void);
void ensure_ucd_once_lock(void);

int u8_next(const char *buf, size_t len, size_t *ioff, unsigned long *out_cp);
int u8_push(ds_str_t *s, unsigned long cp);

#define SBase 0xAC00ul
#define LBase 0x1100ul
#define VBase 0x1161ul
#define TBase 0x11A7ul
#define LCount 19ul
#define VCount 21ul
#define TCount 28ul
#define NCount (VCount * TCount)
#define SCount (LCount * NCount)

int is_Hangul_S(unsigned long cp);
int is_Hangul_L(unsigned long cp);
int is_Hangul_V(unsigned long cp);
int is_Hangul_T(unsigned long cp);
void decompose_Hangul(unsigned long s, void *out_u32vec);
int compose_Hangul(unsigned long *LVT, unsigned long next, size_t *consumed);

unsigned long get_ccc(unsigned long cp);

int decompose_cp(unsigned long cp, int compat, void *out_u32vec);
void reorder_ccc(void *u32vec);
void compose_vec(void *u32vec);

int fold_cp(unsigned long cp, void *out_u32vec);

unsigned long gb_get(unsigned long cp);
int is_EP(unsigned long cp);

int ds__is_control_cc(unsigned long cp);
int ds__is_default_ignorable(unsigned long cp);
unsigned long ds__lump(unsigned long cp);
int ds__map_newlines(ds_str_t *out, const ds_str_t *src, unsigned int flags);
int ds__is_mark(unsigned long cp);
int ds__filter_and_lump(ds_str_t *out, const ds_str_t *src, unsigned int flags);
int ds__insert_charbound(ds_str_t *out, const ds_str_t *src);

int ds__pair_cmp(unsigned long a1, unsigned long b1, unsigned long a2,
                 unsigned long b2);
unsigned long ds__map_simple(const unsigned long *keys,
                             const unsigned long *vals, size_t n,
                             unsigned long cp);

size_t ds__comp_find_pair(unsigned long a, unsigned long b);


#ifdef __cplusplus
}
#endif

#endif /* DS_UNICODE_RUNTIME_H */
