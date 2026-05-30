/* Regression: arrays of `char *` initialized from string literals must
   become arrays of pointers to lifted const arrays, NOT arrays initialized
   from byte-list brace-inits. (The declarator inspection should detect
   the `*` and choose VAR_REF, not BRACE_INIT.) */
#include <stdio.h>

static const char *const PUNCT_4[] = { "%:%:", NULL };
static const char *const PUNCT_3[] = { "<<=", ">>=", "...", "->*", NULL };
static const char *const PUNCT_2[] = {
    "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "++", "--", NULL
};

int main(void) {
    for (int i = 0; PUNCT_4[i]; i++) printf("4: %s\n", PUNCT_4[i]);
    for (int i = 0; PUNCT_3[i]; i++) printf("3: %s\n", PUNCT_3[i]);
    int count = 0;
    for (int i = 0; PUNCT_2[i]; i++) count++;
    printf("2: %d items\n", count);
    return 0;
}
