## v0.0.8 - August 17, 2026
#### Added:
- In allocator.h:
    - Debug and Error messages for `Pool`
    - `division_unsigned_ceil()` helper function
    - `__get_aligned_item_and_map_size()`
    - `__pre_section_first_header()`
    - `__section_free_space()`
    - `__pre_section_default()`
    - `__pool_append_new_page()`

#### Changed:
- In allocator.h:
    - Modularized and optimized `Pool` a lot
    - `Pool` struct is now a little bigger (64 bytes in 64-bit architecture)

## v0.0.7 - August 9, 2026
#### Added:
- In allocator.h
    - `CKIT_OK` and `CKIT_FAIL` macros for better signaling of function success/fail
    - Debug and Error messages for `Bump_Allocator`
    - `get_padding_size()` helper funcion
    - `CLEAN_DATA` macro for better code readability
    - `__bump_append_new_page()`

#### Changed:
- In allocator.h
    - Fixed bug in which debug and error messages would cause the terminal to be colored for every print afterwards
    - Fixed bug with alignment on Bump_Allocator and now it requires less bytes per alloc
    - Changed `CKIT_RAISE_ERROR` message color to red

#### Removed:
- In allocator.h
    - `__Alloc_Header_Bump` "class". Bump_Allocator now doesn't need allocation headers
    - `calc_padding_with_header()` helper function. Replaced with `get_padding_size()`
    - `NO_HEADER` macro

## v0.0.6 - August 6, 2026
#### Added:
- In allocator.h:
    - Reintroduced debug info (only for arena at the moment) enabled by defining `CKIT_DEBUG` before including allocator.h
    - Error raising/panic crashing program on errors (also only for arena) enabled by defining `CKIT_RAISE` before including allocator.h
    - `CKIT_DEBUG_MSG_0-3` macros for debug messaging
    - `CKIT_RAISE_ERROR_0-3` macros for error messaging
    - `NO_HEADER` macro for better code readability
    - `__arena_apend_new_page()`

#### Changed:
- Renamed jobs in test.yml
- Fixed logic bug with arena_alloc not doing the alloc_size math right
- Fixed unit testing (?)

## v0.0.5 - August 6, 2026
#### Added:
- Github Actions tests structure
- **[placeholder code]** arena.c
- **[placeholder code]** bump.c
- **[placeholder code]** pool.c
- In allocator.h
    - `calc_padding_with_header()` helper function

#### Changed:
- Fixed inconsistencies in CHANGELOG.md
- Every entry i CHANGELOG.md now states if it is either a Global Change or Local Change (specifying the file that was modified)
- In allocator.h
    - Renamed `__Alloc_Header_Bump`'s uint8_t from ***alignment_size*** to ***padding***
    - The header used for the `Bump_Allocator` (`__Alloc_Header_Bump`) is now located at the end of the memory requested by the user on alloc instead of the beggining due to alignment problems

#### Removed:
- In allocator.h
    - Dead commented out code 

## v0.0.4 - July 28, 2026
#### Added:
- In allocator.h
    - `PTR_INDEX_FWD` macro for pointer arithmetic
    - `PTR_INDEX_BWD` macro for pointer arithmetic
    - `DEFAULT_ALIGN` macro for pointer alignment
    - `MIN_ITEMS_PER_PAGE_POOL` macro (kinda explains itself)
    - `__Page_Header` "class"
    - `__Micro_Page_Header` "class"
    - `is_power_of_two()` helper function
    - `align_ptr_forward()` helper function
    - `ptr_diff()` helper function
    - `page_header_new()` helper function
    - `expected_alignment_change` helper funcion

#### Changed:
- All version numbers are now preceded with a 'v' in CHANGELOG.md and commits
- In allocator.h
    - __**[IMPORTANT]**__ Rewrote basically the entirety of allocator.h to cut out the middle man of memory allocation (malloc). Now all allocators ask the OS directly for memory (only work for POSIX architecture at the moment)

#### Removed:
- In allocator.h
    - Almost all comments (for now)
    - Debug info (for now)
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
- Every mention of a "class" in CHANGELOG.md is now highlighted
- In allocator.h
    - Renamed `Arena`'s byte array from ***items*** to ***mem***
    - `Pools` now require less minimum chunk data (size of pointer) but it uses part of each chunk for metadata while dealloc'd
    - `pool_alloc()` now return the pointer poiting to the very begining of chunk (`__Free_List_Pool_Node` region), eliminating metadata while using alloc'd memory

## v0.0.2 - July 16, 2026
#### Added:
- In allocator.h
    - Debug info enabled via define macro

#### Changed:
- In allocator.h
    - `Arenas` and `Pools` now request siblings if they run out of memory, creating a linked list of allocators

## v0.0.1 - July 15, 2026
#### Added:
- CHANGELOG.md
- TODO.md
- Makefile
- In allocator.h
    - Macros for KB, MB and GB
    - `byte` macro for pointer arithmetic
    - `CAST` macro for painless complex casting
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