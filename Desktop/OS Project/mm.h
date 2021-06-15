#ifndef __MM__
#define __MM__

#include <stdint.h>

typedef enum{
    MM_FALSE, 
    MM_TRUE
} vm_bool_t;

#define MM_MAX_STRUCT_NAME 32

// phase - 3

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

// phase- 5

typedef struct vm_page_{
    struct vm_page_ *next;
    struct vm_page_ *prev;
    struct vm_page_family_ *pg_family;  // back pointer
    block_meta_data_t block_meta_data;
    char page_memory[0];  // first data block in vm page 
} vm_page_t;


// phase - 2

typedef struct vm_page_family_{
    char struct_name[MM_MAX_STRUCT_NAME];
    vm_page_t *first_page;
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

// phase - 3


// #define offset_of(container_structure, field_name) \
//         (int)(&(((container_structure *)0)->field_name))

#define offset_of(container_structure, field_name)  \
    ((size_t)&(((container_structure *)0)->field_name))


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


// phase - 4 

#define mm_bind_blocks_for_allocation(allocated_meta_block, free_meta_block)  \
          free_meta_block->prev_block = allocated_meta_block;        \
          free_meta_block->next_block = allocated_meta_block->next_block;    \
          allocated_meta_block->next_block = free_meta_block;                \
          if (free_meta_block->next_block)\
          free_meta_block->next_block->prev_block = free_meta_block

// phase- 5


vm_bool_t
mm_is_vm_page_empty(vm_page_t *vm_page);

#define MARK_VM_PAGE_EMPTY(vm_page_t_ptr) \
        vm_page_t_ptr->block_meta_data.is_free = MM_TRUE ; \
        vm_page_t_ptr->block_meta_data.next_block = 0; \
        vm_page_t_ptr->block_meta_data.prev_block = 0

#define ITERATE_VM_PAGE_BEGIN(vm_page_family_ptr, curr)   \
{                                             \
    curr = vm_page_family_ptr->first_page;    \
    vm_page_t *next = NULL;                   \
    for(; curr; curr = next){           \
        next = curr->next;

#define ITERATE_VM_PAGE_END(vm_page_family_ptr, curr)   \
    }}

#define ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page_ptr, curr)    \
{                                                              \
    curr = &vm_page_ptr->block_meta_data;                      \
    block_meta_data_t *next = NULL;                            \
    for( ; curr; curr = next){                                 \
        next = NEXT_META_BLOCK(curr);

#define ITERATE_VM_PAGE_ALL_BLOCKS_END(vm_page_ptr, curr)      \
    }}

vm_page_t*
allocate_vm_page(vm_page_family_t *vm_page_family);

void 
mm_vm_page_delete_and_free(vm_page_t *vm_page);

#endif /**/