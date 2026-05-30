/* Regression: a function returning a pointer to a string literal must
   keep working after the aligner. The original "EOF" has static storage; if
   the aligner lifts it as a function-local plain char[], the returned pointer
   dangles. Fix is to emit `static const char sXXX[N]`. */
#include <stdio.h>

const char *name_for(int code) {
    switch (code) {
        case 0:  return "EOF";
        case 1:  return "IDENT";
        case 2:  return "INT_LIT";
        case 3:  return "PUNCT";
        default: return "UNKNOWN";
    }
}

const char *greeting(void) {
    return "hello";
}

int main(void) {
    /* Use each return value through one stack frame's distance — the
       point is to make sure the pointer is valid AFTER the function
       returns. */
    const char *a = name_for(0);
    const char *b = name_for(1);
    const char *c = name_for(2);
    const char *d = name_for(99);
    const char *g = greeting();
    printf("%s %s %s %s %s\n", a, b, c, d, g);
    return 0;
}
