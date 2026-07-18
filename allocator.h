#ifndef CKIT_ALLOCATOR_H
#define CKIT_ALLOCATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// ====================
//  Macros
// ====================

#define KB * (1 << 10)
#define MB * (1 << 20)
#define GB * (1 << 30)

#define byte unsigned char // This is used primarily to avoid void* arithmetic 
#define CAST(type, ptr) ( (type) (ptr) ) // VERY useful when using a lot of pointer casting



// ====================
//  Arena
// ====================

typedef struct _arena {
    struct _arena* next; // Siblings
    byte* mem;
    size_t capacity;
    size_t used;
} Arena;

Arena* arena_new(size_t capacity) {

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



// ====================
//  Bump Allocator
// ====================

typedef struct __alloc_list_bump_node {
    struct __alloc_list_bump_node* prev;
} __Alloc_List_Bump_Node;


typedef struct _bump_allocator {
    size_t capacity;
    size_t used;
    byte* mem;
    __Alloc_List_Bump_Node* __alloc_list;
    struct _bump_allocator* next; // Siblings
} Bump_Allocator;

Bump_Allocator* bump_allocator_new(size_t capacity) {

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
}



// ====================
//  Pool
// ====================

typedef struct __free_list_pool_node {
    struct __free_list_pool_node* next;
} __Free_List_Pool_Node;

typedef struct _pool {
    unsigned int num_items;
    size_t chunk_size;
    byte* chunks;
    __Free_List_Pool_Node* __free_list;
    struct _pool* next; // Siblings
} Pool;

Pool* pool_new(unsigned int num_items, size_t item_size) {

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

    p->__free_list = NULL;

    // Prepending nodes num_items times
    __Free_List_Pool_Node* temp;
    for (unsigned int i = num_items; i > 0; i--) {
        temp = (__Free_List_Pool_Node*) ((p->chunks) + ((i-1) * item_size));
        temp->next = p->__free_list;
        p->__free_list = temp;
    }

    return p;
}

void* pool_alloc(Pool* p) {

    if (!p) return NULL;

    if (p->__free_list == NULL) {

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

    __Free_List_Pool_Node* ptr = p->__free_list;
    p->__free_list = p->__free_list->next;
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

    node_ptr->next = p->__free_list;
    p->__free_list = node_ptr;

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
}

#endif