#include "unicode_tables.h"

#include "ccc_keys.inc"
#include "ccc_vals.inc"
#include "decomp_keys.inc"
#include "decomp_cnt.inc"
#include "decomp_compat.inc"
#include "decomp_off.inc"
#include "decomp_pool.inc"
#include "comp_keys_a.inc"
#include "comp_keys_b.inc"
#include "comp_vals.inc"
#include "fold_keys.inc"
#include "fold_off.inc"
#include "fold_cnt.inc"
#include "fold_pool.inc"
#include "tolower_keys.inc"
#include "tolower_vals.inc"
#include "toupper_keys.inc"
#include "toupper_vals.inc"
#include "gb_ranges.inc"
#include "ep_ranges.inc"

const unsigned long *U_CCC_KEYS = (const unsigned long*)ccc_keys_bin;
const unsigned long *U_CCC_VALS = (const unsigned long*)ccc_vals_bin;
const size_t         U_CCC_LEN  = sizeof(ccc_keys_bin)/sizeof(unsigned long);

const unsigned long *U_DECOMP_KEYS     = (const unsigned long*)decomp_keys_bin;
const unsigned int  *U_DECOMP_CNT      = (const unsigned int*)decomp_cnt_bin;
const unsigned int  *U_DECOMP_ISCOMPAT = (const unsigned int*)decomp_compat_bin;
const unsigned int  *U_DECOMP_OFF      = (const unsigned int*)decomp_off_bin;
const unsigned long *U_DECOMP_POOL     = (const unsigned long*)decomp_pool_bin;
const size_t         U_DECOMP_LEN      = sizeof(decomp_keys_bin)/sizeof(unsigned long);

const unsigned long *U_COMP_KEYS_A = (const unsigned long*)comp_keys_a_bin;
const unsigned long *U_COMP_KEYS_B = (const unsigned long*)comp_keys_b_bin;
const unsigned long *U_COMP_VALS   = (const unsigned long*)comp_vals_bin;
const size_t         U_COMP_LEN    = sizeof(comp_vals_bin)/sizeof(unsigned long);

const unsigned long *U_FOLD_KEYS = (const unsigned long*)fold_keys_bin;
const unsigned int  *U_FOLD_OFF  = (const unsigned int*)fold_off_bin;
const unsigned int  *U_FOLD_CNT  = (const unsigned int*)fold_cnt_bin;
const unsigned long *U_FOLD_POOL = (const unsigned long*)fold_pool_bin;
const size_t         U_FOLD_LEN  = sizeof(fold_keys_bin)/sizeof(unsigned long);

const unsigned long *U_TOLOWER_KEYS = (const unsigned long*)tolower_keys_bin;
const unsigned long *U_TOLOWER_VALS = (const unsigned long*)tolower_vals_bin;
const size_t         U_TOLOWER_LEN  = sizeof(tolower_keys_bin)/sizeof(unsigned long);

const unsigned long *U_TOUPPER_KEYS = (const unsigned long*)toupper_keys_bin;
const unsigned long *U_TOUPPER_VALS = (const unsigned long*)toupper_vals_bin;
const size_t         U_TOUPPER_LEN  = sizeof(toupper_keys_bin)/sizeof(unsigned long);

const struct U_GB_Range3 *U_GB_RANGES = (const struct U_GB_Range3*)gb_ranges_bin;
const size_t              U_GB_LEN    = sizeof(gb_ranges_bin)/sizeof(struct U_GB_Range3);

const struct U_EP_Range2 *U_EP_RANGES = (const struct U_EP_Range2*)ep_ranges_bin;
const size_t              U_EP_LEN    = sizeof(ep_ranges_bin)/sizeof(struct U_EP_Range2);
