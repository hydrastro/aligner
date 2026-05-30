#ifndef DS_UNICODE_TABLES_H
#define DS_UNICODE_TABLES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned long start;
  unsigned long end;
  unsigned long prop;
} ucd_range_t;

extern const unsigned long U_CCC_KEYS[];
extern const unsigned long U_CCC_VALS[];
extern const size_t U_CCC_LEN;

extern const unsigned long U_DECOMP_KEYS[];
extern const unsigned int U_DECOMP_CNT[];
extern const unsigned int U_DECOMP_OFF[];
extern const unsigned int U_DECOMP_ISCOMPAT[];
extern const unsigned long U_DECOMP_POOL[];
extern const size_t U_DECOMP_LEN;
extern const size_t U_DECOMP_POOL_LEN;

extern const unsigned long U_COMP_KEYS_A[];
extern const unsigned long U_COMP_KEYS_B[];
extern const unsigned long U_COMP_VALS[];
extern const size_t U_COMP_LEN;

extern const unsigned long U_TOLOWER_KEYS[];
extern const unsigned long U_TOLOWER_VALS[];
extern const size_t U_TOLOWER_LEN;
extern const unsigned long U_TOUPPER_KEYS[];
extern const unsigned long U_TOUPPER_VALS[];
extern const size_t U_TOUPPER_LEN;

extern const unsigned long U_FOLD_KEYS[];
extern const unsigned int U_FOLD_CNT[];
extern const unsigned int U_FOLD_OFF[];
extern const unsigned long U_FOLD_POOL[];
extern const size_t U_FOLD_LEN;
extern const size_t U_FOLD_POOL_LEN;

extern const ucd_range_t U_GB_RANGES[];
extern const size_t U_GB_LEN;
extern const ucd_range_t U_EP_RANGES[];
extern const size_t U_EP_LEN;

#define U_COMP_A U_COMP_KEYS_A
#define U_COMP_B U_COMP_KEYS_B
#define U_COMP_VAL U_COMP_VALS


#ifdef __cplusplus
}
#endif

#endif /* DS_UNICODE_TABLES_H */
