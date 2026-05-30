/* Many flavors of char-array initialization with strings. */
#include <stdio.h>

const char  a[]   = "implicit-size";
const char  b[20] = "fixed";
const char  c[]   = "a" "djacent";
static char d[]   = "abc";

int main(void) {
    char local[]  = "hello";
    char fixed[6] = "world";   /* fits exactly with null */
    printf("%s %s %s %s\n", a, b, c, d);
    printf("%s %s\n", local, fixed);
    return 0;
}
