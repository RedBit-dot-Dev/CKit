#ifndef CKIT_ALLOCATOR_H
#define CKIT_ALLOCATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <assert.h>

// ====================
//  Macros
// ====================

#define KB * (1 << 10)
#define MB * (1 << 20)
#define GB * (1 << 30)

#define CAST(type, ptr) ( (type) (ptr) ) // VERY useful when using a lot of pointer casting
#define PTR_INDEX_FWD(ptr, index) ( (void*) ( ((uintptr_t) ptr) + index ) )
#define PTR_INDEX_BWD(ptr, index) ( (void*) ( ((uintptr_t) ptr) - index ) )

#ifndef CKIT_DEBUG_MSG
    #define CKIT_DEBUG_MSG_0(msg) fprintf(stdout, "\033[35m[DEBUG] " msg "\033[37m\n");
    #define CKIT_DEBUG_MSG_1(msg, arg_1) fprintf(stdout, "\033[35m[DEBUG] " msg "\033[37m\n", arg_1);
    #define CKIT_DEBUG_MSG_2(msg, arg_1, arg_2) fprintf(stdout, "\033[35m[DEBUG] " msg "\033[37m\n", arg_1, arg_2);
    #define CKIT_DEBUG_MSG_3(msg, arg_1, arg_2, arg_3) fprintf(stdout, "\033[35m[DEBUG] " msg "\033[37m\n", arg_1, arg_2, arg_3);
#endif

#ifndef CKIT_RAISE_ERROR
    #define CKIT_RAISE_ERROR_0(msg) fprintf(stderr, "\033[31m[ERROR] " msg "\033[37m\n"); exit(1);
    #define CKIT_RAISE_ERROR_1(msg, arg_1) fprintf(stderr, "\033[31m[ERROR] " msg "\033[37m\n", arg_1); exit(1);
    #define CKIT_RAISE_ERROR_2(msg, arg_1, arg_2) fprintf(stderr, "\033[31m[ERROR] " msg "\033[37m\n", arg_1, arg_2); exit(1);
    #define CKIT_RAISE_ERROR_3(msg, arg_1, arg_2, arg_3) fprintf(stderr, "\033[12m[ERROR] " msg "\033[37m\n", arg_1, arg_2, arg_3); exit(1);
#endif

#define DEFAULT_ALIGN sizeof(uint8_t*)
#define MIN_ITEMS_PER_PAGE_POOL 8
#define CLEAN_DATA 0

#ifndef CKIT_OK
    #define CKIT_OK 1
#endif

#ifndef CKIT_FAIL
    #define CKIT_FAIL 0
#endif



// ====================
//  Global Structs
// ====================

typedef struct __page_header {
    struct __page_header* next;
    size_t size;
    size_t used;
} __Page_Header;

typedef struct _micro_page_header {
    struct _micro_page_header* next;
} __Micro_Page_Header;



// ====================
//  Helper Functions
// ====================

// Thank you Ginger Bill
bool is_power_of_two(uintptr_t x) {
	return (x & (x-1)) == 0;
}

// Thank you Ginger Bill
void* align_ptr_forward(const void* ptr, size_t align) {

    uintptr_t p, a, modulo;

    assert(is_power_of_two(align));

    p = (uintptr_t) ptr;
    a = (uintptr_t) align;

    modulo = p & (a-1);

    if (modulo) p += a - modulo;

    return (void*) p;
}

size_t get_padding_size(const void* ptr, const uintptr_t alignment) {

    uintptr_t p, a, modulo;

    assert(is_power_of_two(alignment));

    p = (uintptr_t) ptr;
    a = alignment;
    modulo = p & (a-1);

    if (modulo) modulo = a - modulo;

    return (size_t) modulo;
}

size_t ptr_diff(const void* bigger_ptr, const void* smaller_ptr) {

    assert(bigger_ptr && smaller_ptr);

    return ((uintptr_t) bigger_ptr) - ((uintptr_t) smaller_ptr);
}

__Page_Header* page_header_new(void* hint_ptr, unsigned int num_pages, size_t page_size) {

    __Page_Header* ph = mmap(
        hint_ptr,
        num_pages * page_size,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (!ph) return NULL;

    ph->next = NULL;
    ph->size = num_pages * page_size;
    ph->used = sizeof(__Page_Header);

    return ph;
}



// ====================
//  Arena
// ====================

typedef struct _arena {
    __Page_Header* first_header;
    __Page_Header* last_header; // Just for O(1) append
    size_t total_size;
    size_t total_used;
    size_t page_size;
} Arena;

Arena* arena_new() {

    size_t page_size = sysconf(_SC_PAGESIZE);

    Arena* a = (Arena*) mmap(
        NULL,
        page_size,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (!a) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_0("Couldn't map Arena")
        #endif

        return NULL;
    }

    a->total_size = page_size;
    a->total_used = sizeof(Arena);
    a->page_size = page_size;

    void* unaligned_ptr = PTR_INDEX_FWD(a, a->total_used);
    size_t padding = get_padding_size(unaligned_ptr, DEFAULT_ALIGN);
    __Page_Header* ph = PTR_INDEX_FWD(unaligned_ptr, padding); //align_ptr_forward(unaligned_ptr , DEFAULT_ALIGN);

    a->first_header = a->last_header = ph;
    a->total_used += padding + sizeof(__Page_Header);

    ph->next = NULL;
    ph->size = a->total_size;
    ph->used = sizeof(Arena) + padding + sizeof(__Page_Header);

    return a;
}

__Page_Header* __arena_append_new_page(Arena* a, size_t alloc_size) {

    // TODO: Check if it's ok around overflow values
    unsigned int num_pages = (alloc_size + sizeof(__Page_Header) + (a->page_size - 1) ) / a->page_size;

    // Second argument is integer division rounded up
    __Page_Header* ph = page_header_new(a->last_header, num_pages, a->page_size);
    if (!ph) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_1(
                "Arena located at %p could not get more memory pages for allocation",
                (void*) a
            )
        #endif

        return NULL;
    }

    a->total_size += ph->size;
    a->total_used += ph->used;

    a->last_header->next = ph;
    a->last_header = ph;

    #ifdef CKIT_DEBUG
        CKIT_DEBUG_MSG_3(
            "Arena located at %p had no memory for allocation of %zu bytes and requested %u memory page(s)",
            (void*) a,
            alloc_size,
            num_pages
        )
    #endif

    return ph;
}

// TODO: Make this better
void* arena_alloc(Arena* a, size_t alloc_size) {

    if (!a || alloc_size < 1) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_0("No Arena was passed or alloc size was less than 1 byte at arena_alloc")
        #endif

        return NULL;
    }

    void* unaligned_ptr;
    size_t padding;
    void* ptr;

    __Page_Header* ph = a->first_header;
    while (ph) {

        unaligned_ptr = PTR_INDEX_FWD(ph, ph->used);
        padding = get_padding_size(unaligned_ptr, DEFAULT_ALIGN);

        if (ph->used + padding + alloc_size <= ph->size) {

            ptr = PTR_INDEX_FWD(unaligned_ptr, padding);

            ph->used += alloc_size + padding;
            a->total_used += alloc_size + padding;

            return ptr;
        }

        ph = ph->next;
    }

    ph = __arena_append_new_page(a, alloc_size);
    if (!ph) {
        return NULL;
    }

    unaligned_ptr = PTR_INDEX_FWD(ph, ph->used);
    padding = get_padding_size(unaligned_ptr, DEFAULT_ALIGN);

    ptr = PTR_INDEX_FWD(unaligned_ptr, padding);

    ph->used += alloc_size + padding;
    a->total_used += alloc_size + padding;

    return ptr;
}

bool arena_free(Arena* a) {

    if (!a) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_0("No arena was passed at arena_free")
        #endif

        return CKIT_FAIL;
    }

    #ifdef CKIT_DEBUG
        unsigned int CKIT_DEBUG_ARENA_PAGE_HEADER_COUNT = 0;
    #endif

    while (a->first_header->next) {

        #ifdef CKIT_DEBUG
            CKIT_DEBUG_ARENA_PAGE_HEADER_COUNT++;
        #endif

        __Page_Header* ph = a->first_header->next;
        a->first_header->next = ph->next;
        munmap(ph, ph->size);

    }

    #ifdef CKIT_DEBUG
        CKIT_DEBUG_ARENA_PAGE_HEADER_COUNT++;
        CKIT_DEBUG_MSG_3("Arena located at %p was free'd with %u page headers and %zu bytes used in total",
            (void*) a,
            CKIT_DEBUG_ARENA_PAGE_HEADER_COUNT,
            a->total_used
        )
    #endif

    munmap(a, a->first_header->size);

    return CKIT_OK;
}



// ====================
//  Bump Allocator
// ====================

typedef struct _bump_allocator {
    __Page_Header* last_header; // Every next page header is the previous mapped allocation
    size_t total_size;
    size_t total_used;
    size_t page_size;
} Bump_Allocator;

Bump_Allocator* bump_allocator_new() {

    size_t page_size = sysconf(_SC_PAGESIZE);

    Bump_Allocator* b = mmap(
        NULL,
        page_size,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );

    if (!b) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_0("Couldn't map Bump_Allocator")
        #endif

        return NULL;
    }

    b->total_size = page_size;
    b->total_used = sizeof(Bump_Allocator);
    b->page_size = page_size;

    void* unaligned_ptr = PTR_INDEX_FWD(b, b->total_used);
    size_t padding = get_padding_size(unaligned_ptr, DEFAULT_ALIGN);
    __Page_Header* ph = PTR_INDEX_FWD(unaligned_ptr, padding);
    b->last_header = ph;
    b->total_used += padding + sizeof(__Page_Header);

    ph->next = NULL;
    ph->size = b->total_size;
    ph->used = b->total_used;

    return b;
}

bool __bump_append_new_page(Bump_Allocator* b, size_t alloc_size) {

    // TODO: Check if it's ok around overflow values
    unsigned int num_pages = (alloc_size + (b->page_size - 1) ) / b->page_size;

    // Second argument is integer division rounded up
    __Page_Header* ph = page_header_new(b->last_header, num_pages, b->page_size);
    if (!ph) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_1(
                "Bump_Allocator located at %p could not get more memory pages for allocation",
                (void*) b
            )
        #endif

        return CKIT_FAIL;
    }

    b->total_size += ph->size;
    b->total_used += ph->used;

    ph->next = b->last_header;
    b->last_header = ph;

    #ifdef CKIT_DEBUG
        CKIT_DEBUG_MSG_3(
            "Bump_Allocator located at %p had no memory for allocation of %zu bytes and requested %u memory page(s)",
            (void*) b,
            alloc_size,
            num_pages
        )
    #endif

    return CKIT_OK;
}

void* bump_alloc(Bump_Allocator* b, size_t alloc_size) {

    if (!b || alloc_size < 1) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_0("No Bump_Allocator was passed or alloc size was less than 1 byte at bump_alloc")
        #endif

        return NULL;
    }

    __Page_Header* ph = b->last_header;

    void* unaligned_ptr = PTR_INDEX_FWD(ph, ph->used);
    size_t padding = get_padding_size(unaligned_ptr, DEFAULT_ALIGN);
    void* ptr;

    if (ph->used + padding + alloc_size <= ph->size) {

        ptr = PTR_INDEX_FWD(unaligned_ptr, padding);

        ph->used += padding + alloc_size;
        b->total_used += padding + alloc_size;

        return ptr;
    }

    if (__bump_append_new_page(b, alloc_size) == CKIT_FAIL) return NULL;

    return bump_alloc(b, alloc_size);
}

bool bump_dealloc(Bump_Allocator* b, void* ptr) {

    if (!b || !ptr) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_0("No Bump_Allocator or ptr was passed at bump_dealloc")
        #endif

        return CKIT_FAIL;
    }

    if (ptr < PTR_INDEX_FWD(b->last_header, sizeof(__Page_Header))
     || ptr > PTR_INDEX_FWD(b->last_header, b->last_header->used)) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_2(
                "ptr %p passed to bump_dealloc isn't in the last page allocated by Bump_Allocator located at %p",
                ptr,
                (void*) b
            )
        #endif

        return CKIT_FAIL;
    }

    __Page_Header* ph = b->last_header;
    void* end_of_ptr = PTR_INDEX_FWD(ph, ph->used);

    size_t alloc_size = ptr_diff(end_of_ptr, ptr);

    memset(ptr, CLEAN_DATA, alloc_size);

    ph->used -= alloc_size;
    b->total_used -= alloc_size;

    if (ph->used <= sizeof(__Page_Header) && ph->next) {

        #ifdef CKIT_DEBUG
            CKIT_DEBUG_MSG_1(
                "Bump_Allocator located at %p unmapped it's last page, for it was empty of allocations",
                (void*) b
            )
        #endif

        b->last_header = ph->next;
        munmap(ph, ph->size);
    }

    return CKIT_OK;
}

bool bump_free(Bump_Allocator* b) {

    if (!b) {

        #ifdef CKIT_RAISE
            CKIT_RAISE_ERROR_0("No Bump_Allocator was passed bump_free")
        #endif

        return CKIT_FAIL;
    }

    #ifdef CKIT_DEBUG
        unsigned int CKIT_DEBUG_BUMP_PAGE_HEADER_COUNT = 0;
    #endif

    while (b->last_header->next) {

        #ifdef CKIT_DEBUG
            CKIT_DEBUG_BUMP_PAGE_HEADER_COUNT++;
        #endif

        __Page_Header* ph = b->last_header->next;
        b->last_header->next = ph->next;
        munmap(ph, ph->size);

    }

    #ifdef CKIT_DEBUG
        CKIT_DEBUG_BUMP_PAGE_HEADER_COUNT++;
        CKIT_DEBUG_MSG_3("Bump_Allocator located at %p was free'd with %u page headers and %zu bytes used in total",
            (void*) b,
            CKIT_DEBUG_BUMP_PAGE_HEADER_COUNT,
            b->total_used
        )
    #endif

    munmap(b, b->last_header->size);

    return CKIT_OK;
}



// ====================
//  Pool
// ====================

typedef struct __free_list_pool_node {
    struct __free_list_pool_node* next;
} __Free_List_Pool_Node;

typedef struct _pool {
    __Micro_Page_Header* first_header;
    __Micro_Page_Header* last_header;
    __Free_List_Pool_Node* free_list;
    unsigned int current_num_items;
    size_t item_size_aligned;
    size_t map_size;
} Pool;

Pool* pool_new(size_t item_size) {

    if (item_size < sizeof(__Free_List_Pool_Node)) {
        item_size = sizeof(__Free_List_Pool_Node);
    }

    size_t page_size = sysconf(_SC_PAGESIZE);

    size_t padding = get_padding_size((void*) item_size, DEFAULT_ALIGN);
    size_t actual_item_size = item_size + padding;
    size_t min_num_item_size = actual_item_size * MIN_ITEMS_PER_PAGE_POOL;
    unsigned int num_pages = (min_num_item_size + sizeof(__Micro_Page_Header) + (page_size - 1) ) / page_size;
    size_t map_size = num_pages * page_size;

    Pool* p = mmap(
        NULL,
        map_size,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (!p) return NULL;

    p->item_size_aligned = actual_item_size;
    p->current_num_items = 0;
    p->map_size = map_size;

    void* unaligned_ptr = PTR_INDEX_FWD(p, sizeof(Pool));
    __Micro_Page_Header* mph = align_ptr_forward(unaligned_ptr, DEFAULT_ALIGN);
    mph->next = NULL;
    p->first_header = p->last_header = mph;

    unsigned int num_items_to_skip = sizeof(Pool) + sizeof(mph) + (actual_item_size - 1) / actual_item_size;
    unsigned int num_available_items = ( ( map_size - sizeof(__Micro_Page_Header) ) / actual_item_size ) - num_items_to_skip;

    void* end_of_map = PTR_INDEX_FWD(p, p->map_size);

    __Free_List_Pool_Node* temp;
    for (unsigned int i = 1; i < num_available_items + 1; i++) {
        temp = PTR_INDEX_BWD(end_of_map, i * actual_item_size);
        temp->next = p->free_list;
        p->free_list = temp;
        p->current_num_items++;
    }

    return p;
}

void* pool_alloc(Pool* p) {

    if (!p) return NULL;

    if (p->free_list) {

        __Free_List_Pool_Node* ptr = p->free_list;
        p->free_list = p->free_list->next;

        memset(ptr, CLEAN_DATA, sizeof(__Free_List_Pool_Node));

        return ptr;
    }

    __Micro_Page_Header* mph = mmap(
        p->last_header,
        p->map_size,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (!mph) return NULL;

    p->last_header->next = mph;
    p->last_header = mph;

    unsigned int num_available_items = ( ( p->map_size - sizeof(__Micro_Page_Header) ) / p->item_size_aligned );
    void* end_of_map = mph == p->first_header ?
                       PTR_INDEX_FWD(mph, p->map_size - sizeof(mph) - sizeof(Pool)) :
                       PTR_INDEX_FWD(mph, p->map_size - sizeof(mph));

    __Free_List_Pool_Node* temp;
    for (size_t i = 1; i < num_available_items + 1; i++) {
        temp = PTR_INDEX_BWD(end_of_map, i * p->item_size_aligned);
        temp->next = p->free_list;
        p->free_list = temp;
        p->current_num_items++;
    }

    __Free_List_Pool_Node* ptr = p->free_list;
    p->free_list = p->free_list->next;

    memset(ptr, CLEAN_DATA, sizeof(__Free_List_Pool_Node));

    return ptr;
}

bool pool_dealloc(Pool* p, void* ptr) {

    if (!p || !ptr) return CKIT_FAIL;

    __Micro_Page_Header* mph = p->first_header;
    while (mph) {

        void* end_of_map = mph == p->first_header ?
                       PTR_INDEX_FWD(mph, p->map_size - sizeof(mph) - sizeof(Pool)) :
                       PTR_INDEX_FWD(mph, p->map_size - sizeof(mph));

        void* lower_bound = PTR_INDEX_FWD(mph, sizeof(__Micro_Page_Header));
        void* upper_bound = PTR_INDEX_BWD(end_of_map, p->item_size_aligned);
        if (ptr >= lower_bound && ptr <= upper_bound) {

            memset(ptr, CLEAN_DATA, p->item_size_aligned);

            __Free_List_Pool_Node* list_node = ptr;
            list_node->next = p->free_list;
            p->free_list = list_node;

            return CKIT_OK;
        }

        mph = mph->next;
    }

    // ptr out of scope
    return CKIT_FAIL;
}

bool pool_free(Pool* p) {

    if (!p) return CKIT_FAIL;

    __Micro_Page_Header* mph = p->first_header;
    while (mph->next) {

        __Micro_Page_Header* to_unmap = mph->next;
        mph->next = to_unmap->next;

        munmap(to_unmap, p->map_size);

    }

    munmap(p, p->map_size);

    return CKIT_OK;
}

#endif