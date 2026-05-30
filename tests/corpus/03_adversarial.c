/* block comment with " quote and ' apostrophe and // line-comment look-alike */
// line comment with /* block-look-alike */ inside
#include <stdio.h>
#include <wchar.h>

#define LONG_MACRO "a" \
                  "b" \
                  "c"

int main(void) {
    char a = 'A';
    double x = 0x1F.8p+2;          /* hex float */
    long long y = 0xDEADBEEFULL;
    char *s = "embedded \" \\ \n \x41 \101 ?? end";
    wchar_t *w = L"wide string";
    (void)w;

    printf("a=%c x=%.1f y=%llx\n", a, x, y);
    printf("first byte of s = %d\n", (int)(unsigned char)s[0]);
    printf("macro = %s\n", LONG_MACRO);
    return 0;
}
