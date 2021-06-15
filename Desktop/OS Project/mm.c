#include<stdio.h>
#include<memory.h>
#include<unistd.h>
#include<sys/mman.h>
#include"mm.h"
#include<assert.h>

static vm_page_for_families_t *first_vm_page_for_families = NULL;
static size_t SYSTEM_PAGE_SIZE = 0;

void 
mm_init(){
    SYSTEM_PAGE_SIZE = getpagesize();
}

static void*
mm_get_new_vm_page_from_kernel(int units){

    char *vm_page = mmap(
        0, 
        units*SYSTEM_PAGE_SIZE,
        PROT_READ|PROT_WRITE|PROT_EXEC,
        MAP_ANON|MAP_PRIVATE,
        0, 0
    );

    if(vm_page == MAP_FAILED){
        printf("Error: VM Page allocation failed\n");
        return NULL;
    }
    memset(vm_page, 0, units * SYSTEM_PAGE_SIZE);
    return (void *)vm_page;

}

static void
mm_return_vm_page_to_kernel(void *vm_page, int units){
    if(munmap(vm_page, units*SYSTEM_PAGE_SIZE)){
        printf("Error: Could not munmap VM page to kernel");
    }

}

// phase  - 2

// Page family instantiate algo
void 
mm_instantiate_new_page_family(
    char *struct_name,
    uint32_t struct_size){

        vm_page_family_t *vm_page_family_curr = NULL;
        vm_page_for_families_t *new_vm_page_for_families = NULL;

        if(struct_size > SYSTEM_PAGE_SIZE){
            printf("Error : %s() Structure Size exceeds system page size\n",
            __FUNCTION__);
            return;
        }

    // if there is no vm page allocated to LMM.
        if(!first_vm_page_for_families){

            new_vm_page_for_families = (vm_page_for_families_t *)mm_get_new_vm_page_from_kernel(1);
            first_vm_page_for_families = new_vm_page_for_families;
            first_vm_page_for_families->next = NULL;
            strncpy(first_vm_page_for_families->vm_page_family[0].struct_name, struct_name, MM_MAX_STRUCT_NAME);
            first_vm_page_for_families->vm_page_family[0].struct_size = struct_size;
            return;
        }
    
    // find the count of vm_page_family in the current vm_page_for_families_t
    uint32_t count = 0;

    ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families, vm_page_family_curr){

        if(strncmp(struct_name, vm_page_family_curr->struct_name, MM_MAX_STRUCT_NAME) != 0){
            count++;
            continue;
        }

        assert(0);

    }ITERATE_PAGE_FAMILIES_END(first_vm_page_for_families, vm_page_family_curr)

    // if count is equal to max families per vm page create a new vm page for LMM
    if(count == MAX_FAMILIES_PER_VM_PAGE){

        new_vm_page_for_families = (vm_page_for_families_t *)mm_get_new_vm_page_from_kernel(1);
        new_vm_page_for_families->next = first_vm_page_for_families;
        first_vm_page_for_families = new_vm_page_for_families;

        vm_page_family_curr = &first_vm_page_for_families->vm_page_family[0];

    } 

    // allocate the values to the current vm_page_family
    strncpy(vm_page_family_curr->struct_name, struct_name, MM_MAX_STRUCT_NAME);
    vm_page_family_curr->struct_size = struct_size;

    }

void
mm_print_registered_page_families(){

    vm_page_family_t *vm_page_family_curr = NULL;
    vm_page_for_families_t *vm_page_for_families_curr = NULL;

    for(vm_page_for_families_curr = first_vm_page_for_families;
            vm_page_for_families_curr;
            vm_page_for_families_curr = vm_page_for_families_curr->next){


        ITERATE_PAGE_FAMILIES_BEGIN(vm_page_for_families_curr, vm_page_family_curr){


            printf("Page Family : %s, Size = %u\n", vm_page_family_curr->struct_name, vm_page_family_curr->struct_size);


        } ITERATE_PAGE_FAMILIES_END(vm_page_for_families_curr, vm_page_family_curr);

    }

}

vm_page_family_t *
lookup_page_family_by_name(char *struct_name){

    vm_page_family_t *vm_page_family_curr = NULL;
    vm_page_for_families_t *vm_page_for_families_curr = NULL;

    for(vm_page_for_families_curr = first_vm_page_for_families;        
    vm_page_for_families_curr; vm_page_for_families_curr = vm_page_for_families_curr->next){

        ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families, vm_page_family_curr){

            if(strncmp(vm_page_family_curr->struct_name,

                struct_name,

                MM_MAX_STRUCT_NAME) == 0){

                return vm_page_family_curr;

            }

        } ITERATE_PAGE_FAMILIES_END(first_vm_page_for_families, vm_page_family_curr);

    }

    return NULL;

}

// phase - 4

// here we merge two blocks whose upper and lower blocks are either not present or they are not free.
static void 
mm_union_free_blocks(block_meta_data_t *first, block_meta_data_t *second){

    // if firrst or second is not free then return  
    assert(first->is_free == MM_TRUE && second->is_free == MM_TRUE);

    // update the size of the block after merging them
    first->block_size += sizeof(block_meta_data_t) + second->block_size;

    // update the next ptr of block after merging them
    first->next_block = second->next_block;

    // update the prev ptr of block next to second block
    if(second->next_block)
        second->next_block->prev_block = first;
}

// phase - 5

vm_bool_t 
mm_is_vm_page_empty(vm_page_t *vm_page){
    if(vm_page->block_meta_data.is_free == MM_TRUE 
       && vm_page->block_meta_data.next_block == 0
       && vm_page->block_meta_data.prev_block == 0) return MM_TRUE;

       return MM_FALSE;
}

static inline uint32_t
mm_max_page_allocatable_memory(int units){
    return (uint32_t)((SYSTEM_PAGE_SIZE*units) - offset_of(vm_page_t, page_memory)); 
}

vm_page_t*
allocate_vm_page(vm_page_family_t *vm_page_family){
    vm_page_t *vm_page = mm_get_new_vm_page_from_kernel(1);

    // initialize lower most meta data block of the VM page
    MARK_VM_PAGE_EMPTY(vm_page);

    vm_page->prev = vm_page->next = 0;

    // set the back pointer to the page family
    vm_page->pg_family = vm_page_family;

    // insert new VM page to the head of linked list
    vm_page->next = vm_page_family->first_page;
    if(vm_page_family->first_page) vm_page_family->first_page->prev = vm_page;
    vm_page_family->first_page = vm_page;

    return vm_page;

}

void 
mm_vm_page_delete_and_free(vm_page_t *vm_page){

    vm_page_family_t *vm_page_family = vm_page->pg_family;

    assert(vm_page_family->first_page);

    // if the first vm_page is current or the vm paeg is the head of the list
    if(vm_page_family->first_page == vm_page){
        vm_page_family->first_page = vm_page->next;
        
        if(vm_page->next) 
            vm_page->next->prev = 0;
        
        vm_page->next = vm_page->prev = 0;

        mm_return_vm_page_to_kernel((void *)vm_page, 1);

        return ;
    }

    // if the vm_page is middle or last node
    if(vm_page->next){
        vm_page->prev->next = vm_page->next;
        vm_page->next->prev = vm_page->prev;
    }

    mm_return_vm_page_to_kernel((void *)vm_page, 1);

    return ;
  
}




// int
// main(int argc, char **argv){
//     mm_init();
//     printf("VM Page size = %lu\n", SYSTEM_PAGE_SIZE);
//     void *addr1 = mm_get_new_vm_page_from_kernel(1);
//     void *addr2 = mm_get_new_vm_page_from_kernel(1);
   
//     printf("page 1  = %p, page 2 = %p\n", addr1, addr2);
//     return 0;
// }