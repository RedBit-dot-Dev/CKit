#include <stdio.h>
#include "allocator.h"

int main() {
    Arena a;
    a.page_size = 0;
    printf("Hello World %zu\n", a.page_size);
    return 0;
}