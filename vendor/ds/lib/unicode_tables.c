const unsigned long U_CCC_KEYS[] = { #include "ccc_keys.inc" };
const unsigned long U_CCC_VALS[] = { #include "ccc_vals.inc" };
const size_t U_CCC_LEN = sizeof(U_CCC_KEYS)/sizeof(U_CCC_KEYS[0]);

struct U_Pair { unsigned long a, b; };

const struct U_Pair U_COMP_KEYS[] = { #include "comp_keys_pair.inc" };
const unsigned long U_COMP_VALS[]  = { #include "comp_vals.inc" };
const size_t        U_COMP_LEN     = sizeof(U_COMP_KEYS)/sizeof(U_COMP_KEYS[0]);

struct U_GB_Range3 { unsigned long start, end, prop; };
struct U_EP_Range2 { unsigned long start, end; };

const struct U_GB_Range3 U_GB_RANGES[] = { #include "gb_ranges.inc" };
const size_t             U_GB_LEN      = sizeof(U_GB_RANGES)/sizeof(U_GB_RANGES[0]);

const struct U_EP_Range2 U_EP_RANGES[] = { #include "ep_ranges.inc" };
const size_t             U_EP_LEN      = sizeof(U_EP_RANGES)/sizeof(U_EP_RANGES[0]);
