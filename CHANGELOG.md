## v0.0.4 - July 28, 2026
#### Added:
- `PTR_INDEX_FWD` macro for pointer arithmetic
- `PTR_INDEX_BWD` macro for pointer arithmetic
- `DEFAULT_ALIGN` macro for pointer alignment
- `MIN_ITEMS_PER_PAGE_POOL` macro (kinda explains itself)
- In allocator.h
    - `__Page_Header` "class"
    - `__Micro_Page_Header` "class"
    - `is_power_of_two()` helper function
    - `align_ptr_forward()` helper function
    - `ptr_diff()` helper function
    - `page_header_new()` helper function
    - `expected_alignment_change` helper funcion

#### Changed:
- All version numbers are now preceded with a 'v' in CHANGELOG.md and commits
- __**[IMPORTANT]**__ Rewrote basically the entirety of allocator.h to cut out the middle man of memory allocation (malloc). Now all allocators ask the OS directly for memory (only work for POSIX architecture at the moment)

#### Removed:
- Almost all comments in allocator.h (for now)
- Debug info in allocator.h (for now)
- `byte` macro

## v0.0.3 - July 18, 2026
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

## v0.0.2 - July 16, 2026
#### Added:
- Debug info enabled via define macro

#### Changed:
- `Arenas` and `Pools` now request siblings if they run out of memory, creating a linked list of allocators

## v0.0.1 - July 15, 2026
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

## v0.0.0 (Initial commit) - July 7, 2026
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