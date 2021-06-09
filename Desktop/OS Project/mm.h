#ifndef __MM__
#define __MM__

#include <stdint.h>

typedef enum{
    MM_FALSE, 
    MM_TRUE
} vm_bool_t;

#define MM_MAX_STRUCT_NAME 32

typedef struct vm_page_family_{
    char struct_name[MM_MAX_STRUCT_NAME];
    uint32_t struct_size;
} vm_page_family_t;

typedef struct vm_page_for_families_
{
    struct vm_page_for_families_ *next;
    vm_page_family_t vm_page_family[0];
} vm_page_for_families_t;

#define MAX_FAMILIES_PER_VM_PAGE \
    (SYSTEM_PAGE_SIZE - sizeof(vm_page_for_families_t *))/sizeof(vm_page_family_t)

#define ITERATE_PAGE_FAMILIES_BEGIN(vm_page_for_families_ptr, curr)             \
{                                                                               \
    uint32_t count = 0;                                                         \
    for(curr = (vm_page_family_t *)&vm_page_for_families_ptr->vm_page_family[0]; \
        curr->struct_size && count < MAX_FAMILIES_PER_VM_PAGE ;                 \
        curr++, count++ ) {

#define ITERATE_PAGE_FAMILIES_END(vm_page_for_families_ptr, curr) }}

vm_page_family_t *
lookup_page_family_by_name(char *struct_name);

typedef struct block_meta_data_
{
    vm_bool_t is_free; // Free or allocated
     
    uint32_t block_size; // Size of block

    // ptr to next meta block downward in Data Vm page 
    struct block_meta_data_ *prev_block;

    // ptr to next meta block upward in Data Vm page 
    struct block_meta_data_ *next_block;

    // displacement of meta block from bottom of Vm page
    uint32_t offset;

} block_meta_data_t ; 


#define offset_of(container_structure, field_name) \
        (int)(&(((container_structure *)0)->field_name))


//  block_meta_data_ptr is the pointer to some metadata block present in VM page. The macro must return the start address of the VM page.
#define MM_GET_PAGE_FROM_META_BLOCK(block_meta_data_ptr)    \
        (void *)((char *)block_meta_data_ptr - block_meta_data_ptr->offset)


// The macro must return the starting address of next metablock present in VM page (towards higher address)
#define NEXT_META_BLOCK(block_meta_data_ptr)    \
        (block_meta_data_ptr->next_block)


#define NEXT_META_BLOCK_BY_SIZE(block_meta_data_ptr) \
        (block_meta_data_t *)((char *)(block_meta_data_ptr + 1) \
         + block_meta_data_ptr->block_size)


//  The macro must return the starting address of prev metablock present in VM page (towards lower address)
#define PREV_META_BLOCK(block_meta_data_ptr) \
         (block_meta_data_ptr->prev_block)


#endif /**/