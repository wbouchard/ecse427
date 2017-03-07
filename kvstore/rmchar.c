#include <stdio.h>
#include <string.h>

main() {
    // remove any slashes in storeName
    char test[] = "t/e/s/t";
    char *old = test;
    char new[strlen(test)];
    int newPos = 0;
    printf("value of slash is %d\n\n", '/');

    printf("start:  %s\n", old);
    while (*old) {
        if (*old != '/')
            new[newPos++] = *old;
        *old++;
    }
    new[newPos] = '\0';
    printf("result: %s\n", new);
}
