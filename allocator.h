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

#define DEFAULT_ALIGN sizeof(uint8_t*)
#define MIN_ITEMS_PER_PAGE_POOL 8



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

// Thank you Ginger Bill
size_t calc_padding_with_header(void* ptr, uintptr_t alignment, size_t header_size) {

    uintptr_t p, a, modulo, padding, needed_space;

    assert(is_power_of_two(alignment));

    p = (uintptr_t) ptr;
    a = alignment;
    modulo = p & (a-1);

    padding = 0;
    needed_space = 0;

    if (modulo) p = a - modulo;

    needed_space = (uintptr_t) header_size;

    if (padding < needed_space) {

        needed_space -= padding;

        if ((needed_space & (a-1)))
            padding += a * (1 + (needed_space/a));

        else
            padding += a * (needed_space/a);

    }

    return (size_t) padding;
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
    if (!a) return NULL;

    a->total_size = page_size;
    a->total_used = sizeof(Arena);
    a->page_size = page_size;

    void* unaligned_ptr = PTR_INDEX_FWD(a, a->total_used);
    __Page_Header* ph = align_ptr_forward(unaligned_ptr , DEFAULT_ALIGN);
    a->last_header = a->first_header = ph;
    a->total_used += sizeof(__Page_Header);

    ph->next = NULL;
    ph->size = a->total_size;
    ph->used = sizeof(__Page_Header);

    return a;
}

void* arena_alloc(Arena* a, size_t alloc_size) {

    if (!a || alloc_size < 1) return NULL;

    void* unaligned_ptr;
    size_t padding;
    void* ptr;

    __Page_Header* ph = a->first_header;
    while (ph) {

        unaligned_ptr = PTR_INDEX_FWD(ph, ph->used);
        padding = calc_padding_with_header(unaligned_ptr, DEFAULT_ALIGN, 0);

        if (ph->used + padding + alloc_size < ph->size) {

            ptr = PTR_INDEX_FWD(unaligned_ptr, padding);

            ph->used += alloc_size + padding;
            a->total_used += alloc_size + padding;

            return ptr;
        }

        ph = ph->next;
    }

    // Second argument is integer division rounded up
    // TODO: Check if it's ok around overflow values
    unsigned int num_pages = (alloc_size + (a->page_size - 1) ) / a->page_size;
    ph = page_header_new(a->last_header, num_pages, a->page_size);
    if (!ph) return NULL;

    a->last_header->next = ph;
    a->last_header = ph;

    unaligned_ptr = PTR_INDEX_FWD(ph, ph->used);
    padding = calc_padding_with_header(unaligned_ptr, DEFAULT_ALIGN, 0);

    ptr = PTR_INDEX_FWD(unaligned_ptr, padding);

    ph->used += alloc_size + padding;
    a->total_used += alloc_size + padding;

    return ptr;
}

bool arena_free(Arena* a) {

    if (!a) return false;

    while (a->first_header->next) {

        __Page_Header* ph = a->first_header->next;
        a->first_header->next = ph->next;
        munmap(ph, ph->size);

    }

    munmap(a, a->first_header->size);

    return true;
}



// ====================
//  Bump Allocator
// ====================

typedef struct __alloc_header_bump{
    uint8_t padding;
} __Alloc_Header_Bump;

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

    if (!b) return NULL;

    b->total_size = page_size;
    b->total_used = sizeof(Bump_Allocator);
    b->page_size = page_size;

    void* unaligned_ptr = PTR_INDEX_FWD(b, b->total_used);
    __Page_Header* ph = align_ptr_forward(unaligned_ptr , DEFAULT_ALIGN);
    b->last_header = ph;
    b->total_used += sizeof(__Page_Header);

    ph->next = NULL;
    ph->size = b->total_size;
    ph->used = sizeof(__Page_Header);

    return b;
}

void* bump_alloc(Bump_Allocator* b, size_t alloc_size) {

    if (!b || alloc_size < 1) return NULL;

    __Page_Header* ph = b->last_header;

    void* unaligned_ptr = PTR_INDEX_FWD(ph, ph->used);
    size_t padding = calc_padding_with_header(unaligned_ptr, DEFAULT_ALIGN, sizeof(__Alloc_Header_Bump));
    void* ptr;

    if (ph->used + padding + alloc_size + sizeof(__Alloc_Header_Bump) < ph->size) {

        ptr = PTR_INDEX_FWD(unaligned_ptr, padding);

        __Alloc_Header_Bump* ahb = PTR_INDEX_FWD(ptr, alloc_size);
        ahb->padding = padding;

        ph->used += alloc_size + padding;
        b->total_used += alloc_size + padding;

        return ptr;
    }

    // Second argument is integer division rounded up
    // TODO: Check if it's ok around overflow values
    unsigned int num_pages = (alloc_size + (b->page_size - 1) ) / b->page_size;
    ph = page_header_new(b->last_header, num_pages, b->page_size);
    if (!ph) return NULL;
    b->total_size += ph->size;

    ph->next = b->last_header;
    b->last_header = ph;

    unaligned_ptr = PTR_INDEX_FWD(ph, ph->used);
    padding = calc_padding_with_header(unaligned_ptr, DEFAULT_ALIGN, sizeof(__Alloc_Header_Bump));

    ptr = PTR_INDEX_FWD(unaligned_ptr, padding);
    __Alloc_Header_Bump* ahb = PTR_INDEX_FWD(ptr, alloc_size);
    ahb->padding = padding;

    ph->used += alloc_size + padding;
    b->total_used += alloc_size + padding;

    return ptr;
}

bool bump_dealloc(Bump_Allocator* b, void* ptr) {

    if (!b || !ptr) return false;

    if (ptr < PTR_INDEX_FWD(b->last_header, sizeof(__Page_Header))
     || ptr > PTR_INDEX_FWD(b->last_header, b->last_header->used)) {
        return false;
    }

    __Page_Header* ph = b->last_header;
    void* end_of_ptr = PTR_INDEX_FWD(ph, ph->used);
    __Alloc_Header_Bump* ahb = PTR_INDEX_BWD(end_of_ptr, sizeof(__Alloc_Header_Bump));
    size_t alloc_size = ptr_diff(ahb, ptr);

    memset(ptr, 0, alloc_size + sizeof(__Alloc_Header_Bump));

    ph->used -= alloc_size + ahb->padding;
    b->total_used -= alloc_size + ahb->padding;

    if (ph->used <= sizeof(__Page_Header) && ph->next) {
        b->last_header = ph->next;
        munmap(ph, ph->size);
    }

    return true;
}

bool bump_free(Bump_Allocator* b) {

    if (!b) return false;

    while (b->last_header->next) {

        __Page_Header* ph = b->last_header->next;
        b->last_header->next = ph->next;
        munmap(ph, ph->size);

    }

    munmap(b, b->last_header->size);

    return true;
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

    size_t padding = calc_padding_with_header((void*) item_size, DEFAULT_ALIGN, 0);
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

        memset(ptr, 0, sizeof(__Free_List_Pool_Node));

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

    memset(ptr, 0, sizeof(__Free_List_Pool_Node));

    return ptr;
}

bool pool_dealloc(Pool* p, void* ptr) {

    if (!p || !ptr) return NULL;

    __Micro_Page_Header* mph = p->first_header;
    while (mph) {

        void* end_of_map = mph == p->first_header ?
                       PTR_INDEX_FWD(mph, p->map_size - sizeof(mph) - sizeof(Pool)) :
                       PTR_INDEX_FWD(mph, p->map_size - sizeof(mph));

        void* lower_bound = PTR_INDEX_FWD(mph, sizeof(__Micro_Page_Header));
        void* upper_bound = PTR_INDEX_BWD(end_of_map, p->item_size_aligned);
        if (ptr >= lower_bound && ptr <= upper_bound) {

            memset(ptr, 0, p->item_size_aligned);

            __Free_List_Pool_Node* list_node = ptr;
            list_node->next = p->free_list;
            p->free_list = list_node;

            return true;
        }

        mph = mph->next;
    }

    // ptr out of scope
    return false;
}

bool pool_free(Pool* p) {

    if (!p) return false;

    __Micro_Page_Header* mph = p->first_header;
    while (mph->next) {

        __Micro_Page_Header* to_unmap = mph->next;
        mph->next = to_unmap->next;

        munmap(to_unmap, p->map_size);

    }

    munmap(p, p->map_size);

    return true;
}

#endif