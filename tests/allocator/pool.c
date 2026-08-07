#include <stdio.h>
#include "allocator.h"

int main() {
    Pool p;
    p.map_size = 0;
    printf("Hello World %zu\n", p.map_size);
    return 0;
}