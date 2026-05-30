/* Heavy char-literal use — every '\n' etc. becomes a decimal integer. */
#include <stdio.h>
#include <ctype.h>

static int is_vowel(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'
        || c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
}

static int classify(char c) {
    if (c >= '0' && c <= '9') return 1;
    if (c == ' ' || c == '\t' || c == '\n') return 2;
    if (is_vowel(c)) return 3;
    return 0;
}

int main(void) {
    char input[] = "Hello, World!\n";
    int counts[4] = {0, 0, 0, 0};
    for (int i = 0; input[i]; i++) {
        counts[classify(input[i])]++;
    }
    printf("other=%d digit=%d ws=%d vowel=%d\n",
           counts[0], counts[1], counts[2], counts[3]);
    return 0;
}
