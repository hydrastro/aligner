#ifndef DS_STR_ALGO_H
#define DS_STR_ALGO_H

#include "common.h"
#include "str.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ds__in_set(unsigned char c, const unsigned char *set, size_t n);
void ds__maximal_suffix(const unsigned char *x, size_t m, int *ms, int *p);
void ds__maximal_suffix_rev(const unsigned char *x, size_t m, int *ms, int *p);

long FUNC(str_find_bmh)(ds_str_t *s, const void *needle, size_t n,
                        size_t start);
long FUNC(str_find_twoway)(ds_str_t *s, const void *needle, size_t n,
                           size_t start);

int FUNC(str_ltrim_set)(ds_str_t *s, const unsigned char *set, size_t set_n);
int FUNC(str_rtrim_set)(ds_str_t *s, const unsigned char *set, size_t set_n);
int FUNC(str_trim_set)(ds_str_t *s, const unsigned char *set, size_t set_n);

typedef int (*ds_byte_pred)(unsigned char c);
int FUNC(str_ltrim_pred)(ds_str_t *s, ds_byte_pred pred);
int FUNC(str_rtrim_pred)(ds_str_t *s, ds_byte_pred pred);
int FUNC(str_trim_pred)(ds_str_t *s, ds_byte_pred pred);

typedef void (*ds_str_token_cb)(const char *ptr, size_t len, void *ud);
void FUNC(str_split_each_c)(ds_str_t *s, unsigned char sep, ds_str_token_cb cb,
                            void *ud);
void FUNC(str_split_each_substr)(ds_str_t *s, const void *sep, size_t nsep,
                                 ds_str_token_cb cb, void *ud);

int FUNC(str_join_c)(ds_str_t *dst, ds_str_t **parts, size_t count,
                     const void *sep, size_t nsep);

int FUNC(str_starts_with)(ds_str_t *s, const void *prefix, size_t n);
int FUNC(str_ends_with)(ds_str_t *s, const void *suffix, size_t n);
long FUNC(str_count)(ds_str_t *s, const void *needle, size_t n);
int FUNC(str_replace_all)(ds_str_t *s, const void *from, size_t nfrom,
                          const void *to, size_t nto);
int FUNC(str_replace_all_builder)(ds_str_t *s, const void *from, size_t nfrom,
                                  const void *to, size_t nto);

#ifdef __cplusplus
}
#endif

#endif /* DS_STR_ALGO_H */
