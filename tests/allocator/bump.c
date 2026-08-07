#include <stdio.h>
#include "allocator.h"

int main() {
    Bump_Allocator b;
    b.page_size = 0;
    printf("Hello World %zu\n", b.page_size);
    return 0;
}