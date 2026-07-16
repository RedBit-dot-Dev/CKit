#ifndef SYNTH_ALLOCATOR_H
#define SYNTH_ALLOCATOR_H

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
    byte* items;
    size_t capacity;
    size_t used;
} Arena;

Arena* arena_new(size_t capacity) {

    Arena* a = calloc(capacity + sizeof(Arena), sizeof(byte));
    if (!a) return NULL;

    a->items = ( (byte*) a) + sizeof(Arena);  //(byte*) calloc(capacity, 1);

    a->capacity = capacity;
    a->used = 0;

    return a;
}

void* arena_alloc(Arena* a, size_t alloc_size) {

    if (!a || a->used + alloc_size > a->capacity || alloc_size < 1) return NULL;

    void* ptr = (void*) ((a->items) + (a->used));
    a->used += alloc_size;

    return ptr;
}

bool arena_free(Arena* a) {

    if (!a) return false;

    free(a);
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
} Pool;

Pool* pool_new(unsigned int num_items, size_t item_size) {
    
    size_t actual_size = item_size + sizeof(__Free_List_Pool_Node);

    Pool* p = calloc( (num_items * actual_size) + sizeof(Pool), sizeof(byte));
    if (!p) return NULL;

    p->num_items = num_items;
    p->chunk_size = item_size;
    p->chunks = ( (byte*) p) + sizeof(Pool);

    p->__free_list = NULL;

    // Prepending nodes num_items times
    __Free_List_Pool_Node* temp;
    for (unsigned int i = num_items; i > 0; i--) {
        temp = (__Free_List_Pool_Node*) ((p->chunks) + ((i-1) * actual_size));
        temp->next = p->__free_list;
        p->__free_list = temp;
    }

    return p;
}

void* pool_alloc(Pool* p) {

    if (!p || p->__free_list == NULL) return NULL;

    __Free_List_Pool_Node* ptr = p->__free_list;
    p->__free_list = p->__free_list->next;
    ptr->next = NULL;

    return CAST( void*, CAST(byte*, ptr) + sizeof(__Free_List_Pool_Node) );
}

bool pool_dealloc(Pool* p, void* ptr) {

    if (!p || !ptr) return false;

    // Separated this because it was about 165 chars long and very difficult to read
    size_t actual_size = p->chunk_size + sizeof(__Free_List_Pool_Node);
    byte* upper_bound = ( CAST(byte*, p->chunks) + (p->num_items * actual_size ) );

    // Making sure the pointer is valid to dealloc
    if ( CAST(byte*, ptr) < CAST(byte*, p->chunks) || CAST(byte*, ptr) > upper_bound)
        return false;

    memset(ptr, 0, p->chunk_size);

    __Free_List_Pool_Node* node_ptr = CAST( __Free_List_Pool_Node*, CAST(byte*, ptr) - sizeof(__Free_List_Pool_Node) );

    node_ptr->next = p->__free_list;
    p->__free_list = node_ptr;

    return true;
}

bool pool_free(Pool* p) {

    if (!p) return false;

    free(p);
    return true;
}

#endif