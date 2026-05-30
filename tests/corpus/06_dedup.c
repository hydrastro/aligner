/* Multiple functions referring to the same literal — exercises per-function
   dedup tables; the same string in different functions should produce two
   separate sXXX entries (one per scope). */
#include <stdio.h>

static void greet(void) {
    printf("hello\n");
    printf("world\n");
    printf("hello\n");   /* dedup within greet */
}

static void chat(void) {
    printf("world\n");   /* same content as greet's, but separate scope */
    printf("again\n");
}

int main(void) {
    greet();
    chat();
    return 0;
}
