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

size_t expected_alignment_change(const void* ptr, size_t align) {
    return ptr_diff( align_ptr_forward(ptr, align), ptr );
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
    size_t change;
    void* ptr;

    __Page_Header* ph = a->first_header;
    while (ph) {

        unaligned_ptr = PTR_INDEX_FWD(ph, ph->used);
        change = expected_alignment_change(unaligned_ptr, DEFAULT_ALIGN);

        if (ph->size - ( ph->used + change) >= alloc_size) {

            ptr = PTR_INDEX_FWD(unaligned_ptr, change);

            ph->used += alloc_size + change;
            a->total_used += alloc_size + change;

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
    change = expected_alignment_change(unaligned_ptr, DEFAULT_ALIGN);

    ptr = PTR_INDEX_FWD(unaligned_ptr, change);

    ph->used += alloc_size + change;
    a->total_used += alloc_size + change;

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

/* Arena* arena_new(size_t capacity) {

    Arena* a = calloc(capacity + sizeof(Arena), sizeof(byte));
    if (!a) return NULL;

    a->mem = ( (byte*) a) + sizeof(Arena);

    a->capacity = capacity;
    a->used = 0;
    a->next = NULL;

    return a;
}

void* arena_alloc(Arena* a, size_t alloc_size) {

    if (!a || alloc_size < 1) return NULL;

    if (alloc_size > a->capacity) {

        #ifdef CKIT_DEBUG
        fprintf(stderr, "[DEBUG] Requested alloc size (%zu bytes) for Arena at %p is bigger "
            "than it's capacity (%zu bytes)\n",
            alloc_size, (void*) a, a->capacity
        );
        #endif

        return NULL;
    }

    if (a->used + alloc_size > a->capacity) {

        if (!(a->next)) {
            a->next = arena_new(a->capacity);

            #ifdef CKIT_DEBUG
            fprintf(stderr, "[DEBUG] Arena at address %p didn't have enough memory for alloc, "
                "requested new sibling\n",
                (void*) a
            );
            #endif
        }

        if (a->next) return arena_alloc(a->next, alloc_size);

        return NULL;
    }

    void* ptr = (void*) ((a->mem) + (a->used));
    a->used += alloc_size;

    return ptr;
}

bool arena_free(Arena* a) {

    if (!a) return false;

    #ifdef CKIT_DEBUG
        unsigned int CKIT_DEBUG_ARENA_SIBLING_COUNT = 0;
    #endif

    while (a->next) {
        Arena* n = a->next;
        a->next = a->next->next;
        free(n);

        #ifdef CKIT_DEBUG
            CKIT_DEBUG_ARENA_SIBLING_COUNT++;
        #endif

    }

    #ifdef CKIT_DEBUG
        fprintf(stderr, "[DEBUG] Arena at address %p free'd with %u sibling(s)\n",
            (void*) a, CKIT_DEBUG_ARENA_SIBLING_COUNT
        );
    #endif
    
    free(a);

    return true;
}
 */


// ====================
//  Bump Allocator
// ====================

typedef struct __alloc_header_bump{
    uint8_t alignment_size;
} __Alloc_Header_Bump;

typedef struct _bump_allocator {
    __Page_Header* last_header; // Every next page header is pior mapped allocation
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
    size_t change = expected_alignment_change(unaligned_ptr, DEFAULT_ALIGN);
    void* ptr;

    if (ph->size - ( ph->used + change) >= alloc_size + sizeof(__Alloc_Header_Bump)) {

        ptr = PTR_INDEX_FWD(unaligned_ptr, change);

        __Alloc_Header_Bump* ahb = ptr;
        ahb->alignment_size = change;

        ph->used += alloc_size + change;
        b->total_used += alloc_size + change;

        return PTR_INDEX_FWD(ptr, sizeof(__Alloc_Header_Bump));
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
    change = expected_alignment_change(unaligned_ptr, DEFAULT_ALIGN);

    ptr = PTR_INDEX_FWD(unaligned_ptr, change);
    __Alloc_Header_Bump* ahb = ptr;
    ahb->alignment_size = change;

    ph->used += alloc_size + change;
    b->total_used += alloc_size + change;

    return PTR_INDEX_FWD(ptr, sizeof(__Alloc_Header_Bump));
}

bool bump_dealloc(Bump_Allocator* b, void* ptr) {

    if (!b || !ptr) return false;

    if (ptr < PTR_INDEX_FWD(b->last_header, sizeof(__Page_Header))
     || ptr > PTR_INDEX_FWD(b->last_header, b->last_header->used)) {
        return false;
    }

    __Page_Header* ph = b->last_header;
    __Alloc_Header_Bump* ahb = PTR_INDEX_BWD(ptr, sizeof(__Alloc_Header_Bump));

    void* ptr_to_dealloc = PTR_INDEX_BWD(ahb, ahb->alignment_size);
    size_t alloc_size = ptr_diff( PTR_INDEX_FWD(ph, ph->used) , ptr_to_dealloc);
    memset(ptr_to_dealloc, 0, alloc_size);

    ph->used -= alloc_size;
    b->total_used -= alloc_size;

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

/* Bump_Allocator* bump_allocator_new(size_t capacity) {

    Bump_Allocator* b = calloc(capacity + sizeof(Bump_Allocator), sizeof(byte));
    if (!b) return NULL;

    b->mem = ( (byte*) b) + sizeof(Bump_Allocator);

    b->capacity = capacity;
    b->used = 0;
    b->__alloc_list = NULL;
    b->next = NULL;

    return b;
}

void* bump_alloc(Bump_Allocator* b, size_t alloc_size) {

    if (!b || alloc_size < 1) return NULL;

    #ifdef CKIT_DEBUG
    if ( (alloc_size / sizeof(__Alloc_List_Bump_Node)) <= 3 )
        fprintf(stderr, "[DEBUG] Requested alloc for Bump_Allocator creation is inefficient, "
            "metadata required for each alloc (%zu bytes) / alloc_size (%zu bytes) = %.2lf%%\n",
            sizeof(__Alloc_List_Bump_Node), alloc_size,
            100 * ( CAST(double, sizeof(__Alloc_List_Bump_Node)) / CAST(double, alloc_size)) );
    #endif

    if (alloc_size + sizeof(__Alloc_List_Bump_Node) > b->capacity) {

        #ifdef CKIT_DEBUG
        fprintf(stderr, "[DEBUG] Requested alloc size + metadata (%zu bytes) for Bump_Allocator "
            "at %p is bigger than it's capacity (%zu bytes)\n",
            alloc_size, (void*) b, b->capacity + sizeof(__Alloc_List_Bump_Node));
        #endif

        return NULL;
    }

    if (b->used + alloc_size + sizeof(__Alloc_List_Bump_Node) > b->capacity) {

        if (!(b->next)) {
            b->next = bump_allocator_new(b->capacity);

            #ifdef CKIT_DEBUG
            fprintf(stderr, "[DEBUG] Bump_Allocator at address %p didn't have enough memory for alloc, "
                "requested new sibling\n", (void*) b);
            #endif

        }

        if (b->next) return bump_alloc(b->next, alloc_size);

        return NULL;
    }

    __Alloc_List_Bump_Node* ptr = (__Alloc_List_Bump_Node*) CAST( void*, (b->mem) + (b->used) );
    ptr->prev = b->__alloc_list;
    b->__alloc_list = ptr;

    b->used += alloc_size + sizeof(__Alloc_List_Bump_Node);

    return CAST( void*, CAST(byte*, (ptr)) + sizeof(__Alloc_List_Bump_Node) );
}

bool bump_dealloc(Bump_Allocator* b) {

    if (!b) return false;

    if (b->next && b->next->__alloc_list) {

        #ifdef CKIT_DEBUG
        fprintf(stderr, "[DEBUG] Bump_Allocator at address %p doesn't have the last alloc'd ptr in "
            "its memory, checked siblings for dealloc\n",
            (void*) b
        );
        #endif

        return bump_dealloc(b->next);
    }

    if (!(b->__alloc_list)) return false;

    __Alloc_List_Bump_Node* ptr = b->__alloc_list;
    b->__alloc_list = b->__alloc_list->prev;

    size_t size_to_dealloc = CAST( size_t, ((b->mem) + (b->used)) - (CAST(byte*, ptr)) );
    b->used -= size_to_dealloc;

    memset( CAST( void*, (b->mem) + (b->used) ), 0, size_to_dealloc);

    return true;
}

bool bump_free(Bump_Allocator* b) {

    if (!b) return false;

    #ifdef CKIT_DEBUG
    unsigned int CKIT_DEBUG_BUMP_SIBLING_COUNT = 0;
    #endif

    while (b->next) {
        Bump_Allocator* n = b->next;
        b->next = b->next->next;
        free(n);

        #ifdef CKIT_DEBUG
        CKIT_DEBUG_BUMP_SIBLING_COUNT++;
        #endif

    }

    #ifdef CKIT_DEBUG
    fprintf(stderr, "[DEBUG] Bump_Allocator at address %p free'd with %u sibling(s)\n",
        (void*) b, CKIT_DEBUG_BUMP_SIBLING_COUNT
    );
    #endif

    free(b);

    return true;
} */



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

    size_t alignment_size = expected_alignment_change((void*) item_size, DEFAULT_ALIGN);
    size_t actual_item_size = item_size + alignment_size;
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

/* Pool* pool_new(unsigned int num_items, size_t item_size) {

    if (item_size < sizeof(__Free_List_Pool_Node)) {

        item_size = sizeof(__Free_List_Pool_Node);

        #ifdef CKIT_DEBUG
        fprintf(stderr, "[DEBUG] Requested item_size for Pool creation is less than minimum "
            "required for metadata, updated it's value to the minimum\n"
        );
        #endif
    }

    Pool* p = calloc( (num_items * item_size) + sizeof(Pool), sizeof(byte));
    if (!p) return NULL;

    p->num_items = num_items;
    p->chunk_size = item_size;
    p->chunks = ( (byte*) p) + sizeof(Pool);
    p->next = NULL;

    p->free_list = NULL;

    // Prepending nodes num_items times
    __Free_List_Pool_Node* temp;
    for (unsigned int i = num_items; i > 0; i--) {
        temp = (__Free_List_Pool_Node*) ((p->chunks) + ((i-1) * item_size));
        temp->next = p->free_list;
        p->free_list = temp;
    }

    return p;
}

void* pool_alloc(Pool* p) {

    if (!p) return NULL;

    if (p->free_list == NULL) {

        if (!(p->next)) {
            p->next = pool_new(p->num_items, p->chunk_size);

            #ifdef CKIT_DEBUG
            fprintf(stderr, "[DEBUG] Pool at address %p didn't have enough memory for alloc, "
                "requested new sibling\n",
                (void*) p
            );
            #endif
        }

        if (p->next) return pool_alloc(p->next);

        return NULL;
    }

    __Free_List_Pool_Node* ptr = p->free_list;
    p->free_list = p->free_list->next;
    ptr->next = NULL;

    return (void*) ptr;
}

bool pool_dealloc(Pool* p, void* ptr) {

    if (!p || !ptr) return false;

    // Separated this because it was about 165 chars long and very difficult to read
    //size_t actual_size = p->chunk_size + sizeof(__Free_List_Pool_Node);
    byte* upper_bound = ( (p->chunks) + (p->chunk_size * p->num_items) );

    // Making sure the pointer is valid to dealloc. If not, check next pools if any exists
    if ( CAST(byte*, ptr) < p->chunks || CAST(byte*, ptr) >  upper_bound) {

        if (p->next) {

            #ifdef CKIT_DEBUG
            fprintf(stderr, "[DEBUG] Pool at address %p doesn't have ptr %p in its memory chunks, "
                "checked sibling pools for dealloc\n",
                (void*) p, ptr
            );
            #endif

            return pool_dealloc(p->next, ptr);
        }

        return false;
    }

    __Free_List_Pool_Node* node_ptr = (__Free_List_Pool_Node*) ptr;

    if (p->chunk_size > sizeof(__Free_List_Pool_Node)) memset(ptr, 0, p->chunk_size);

    node_ptr->next = p->free_list;
    p->free_list = node_ptr;

    return true;
}

bool pool_free(Pool* p) {

    if (!p) return false;

    #ifdef CKIT_DEBUG
    unsigned int CKIT_DEBUG_POOL_SIBLING_COUNT = 0;
    #endif

    while (p->next) {
        Pool* n = p->next;
        p->next = p->next->next;
        free(n);

        #ifdef CKIT_DEBUG
        CKIT_DEBUG_POOL_SIBLING_COUNT++;
        #endif

    }

    #ifdef CKIT_DEBUG
    fprintf(stderr, "[DEBUG] Pool at address %p free'd with %u sibling(s)\n",
        (void*) p,
        CKIT_DEBUG_POOL_SIBLING_COUNT
    );
    #endif
    
    free(p);

    return true;
} */

#endif