/* Strings inside struct initializers — exercises array-init detection
   in nested brace contexts. */
#include <stdio.h>
#include <string.h>

struct Person {
    char name[32];
    int  age;
};

struct Person alice = {"Alice", 30};
struct Person bob   = {"Bob",   25};

int main(void) {
    struct Person carol;
    strcpy(carol.name, "Carol");
    carol.age = 40;
    printf("%s %d\n", alice.name, alice.age);
    printf("%s %d\n", bob.name,   bob.age);
    printf("%s %d\n", carol.name, carol.age);
    return 0;
}
