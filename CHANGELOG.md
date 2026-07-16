## 0.0.1 - July 15, 2026
#### Added:
- Macros for KB, MB and GB
- `byte` macro for pointer arithmetic
- `CAST` macro for painless complex casting
- CHANGELOG.md
- TODO.md
- Makefile
- In allocator.h
    - `Arena`
      - `arena_new()`
      - `arena_alloc()`
      - `arena_free()`
    - `Pool`
      - `pool_new()`
      - `pool_alloc()`
      - `pool_dealloc()`
      - `pool_free()`

#### Changed:
- Updated .gitignore
- Moved implementation TODOS from header files to TODO.md

## 0.0.0 (Initial commit) - July 7, 2026
#### Added:
- .gitignore
- README.md
- allocator.h
- graph.h
- hash.h
- heap.h
- list.h
- queue.h
- sort.h
- stack.h
- str.h
- tree.h
- union_find.h
- vector.h