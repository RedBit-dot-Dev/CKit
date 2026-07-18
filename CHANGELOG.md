## 0.0.3 - July 18, 2026
#### Added:
- In allocator.h
    - `Bump_Allocator`
      - `bump_allocator_new()`
      - `bump_alloc()`
      - `bump_dealloc()`
      - `bump_free()`

#### Changed:
- Renamed repo to __**CKit**__
- Renamed `Arena`'s byte array from ***items*** to ***mem***
- `Pools` now require less minimum chunk data (size of pointer) but it uses part of each chunk for metadata while dealloc'd
- `pool_alloc` now return the pointer poiting to the very begining of chunk (`__Free_List_Pool_Node` region), eliminating metadata while using alloc'd memory
- Every mention of a "class" in CHANGELOG.md is now highlighted

## 0.0.2 - July 16, 2026
#### Added:
- Debug info enabled via define macro

#### Changed:
- `Arenas` and `Pools` now request siblings if they run out of memory, creating a linked list of allocators

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