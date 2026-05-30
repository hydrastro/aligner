#include <stdio.h>
#include <wchar.h>

int main(void) {
    char a = 'A';
    char nl = '\n';
    int hex = '\x41';
    int oct = '\101';
    char *s = "hex \x41 oct \101 newline\n";
    wchar_t *w = L"wide";
    char *u = u8"utf8";
    (void)w;
    printf("a=%d nl=%d hex=%d oct=%d\n", a, nl, hex, oct);
    printf("s=%s", s);          /* trailing \n already in s */
    printf("u=%s\n", u);
    return 0;
}
