#include <stdio.h>

const char *greeting = "hello";
static int counter = 0;

struct Point { int x; int y; };

typedef struct {
    int a, b;
} Pair;

static int helper(int x) {
    if (x > 0) {
        return x * 2;
    } else {
        return 0;
    }
}

int main(int argc, char **argv) {
    int x = 5;
    {
        int nested = 7;
        printf("inner %d\n", nested);
    }
    return helper(x);
}
