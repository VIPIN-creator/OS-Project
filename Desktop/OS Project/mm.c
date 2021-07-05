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

            // pointer to the priority list of free data blocks 
            init_glthread(&first_vm_page_for_families->vm_page_family[0].free_block_priority_list_head); 
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
    init_glthread(&vm_page_family_curr->free_block_priority_list_head);

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

    // node of the priority list of free data blocks
    init_glthread(&vm_page->block_meta_data.priority_thread_glue);

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

//  comparison function for comparing two free meta data blocks
static int
free_blocks_comparison_function(void *_block_meta_data1, 
                                void *_block_meta_data2 ){
                                    
                block_meta_data_t *block_meta_data1 = (block_meta_data_t *)_block_meta_data1;
                block_meta_data_t *block_meta_data2 = (block_meta_data_t *)_block_meta_data2;

                if(block_meta_data1->block_size > block_meta_data2->block_size)
                    return -1;
                else if(block_meta_data1->block_size < block_meta_data2->block_size)
                    return 1;
                return 0;
}

// Function splitting the free data block
static vm_bool_t
mm_split_free_data_block_for_allocation(
        vm_page_family_t *vm_page_family,
        block_meta_data_t *block_meta_data,
        uint32_t size){

    block_meta_data_t *next_block_meta_data = NULL;

    // check if block meta data is free or not
    assert(block_meta_data->is_free == MM_TRUE);

    // if available size is greater than req size
    if(block_meta_data->block_size < size){
        return MM_FALSE;
    }

    // reamining size after splitting the meta data block
    uint32_t remaining_size =
        block_meta_data->block_size - size; 

    block_meta_data->is_free = MM_FALSE;
    block_meta_data->block_size = size;
    /*Unchanged*/
    /*block_meta_data->offset =  ??*/

    /* Since this block of memory is going to be allocated to the application, 
     * remove it from priority list of free blocks*/
    remove_glthread(&block_meta_data->priority_thread_glue);

    /*Case 1 : No Split*/
    if(!remaining_size){
        /*No need to repair linkages, they do not change*/
        //mm_bind_blocks_for_allocation(block_meta_data, next_block_meta_data);
        return MM_TRUE;
    }

    /*Case 3 : Partial Split : Soft Internal Fragmentation*/
    else if(sizeof(block_meta_data_t) < remaining_size && 
        remaining_size < (sizeof(block_meta_data_t) + vm_page_family->struct_size)){
        /*New Meta block is to be created*/
        next_block_meta_data = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
        next_block_meta_data->is_free = MM_TRUE;
        next_block_meta_data->block_size =
            remaining_size - sizeof(block_meta_data_t);
        next_block_meta_data->offset = block_meta_data->offset +
            sizeof(block_meta_data_t) + block_meta_data->block_size;
        init_glthread(&next_block_meta_data->priority_thread_glue);
        mm_add_free_block_meta_data_to_free_block_list(
                vm_page_family, next_block_meta_data);
        mm_bind_blocks_for_allocation(block_meta_data, next_block_meta_data);
    }

    /*Case 3 : Partial Split : Hard Internal Fragmentation*/
    else if(remaining_size < sizeof(block_meta_data_t)){
        //next_block_meta_data = block_meta_data->next_block;
        /*No need to repair linkages, they do not change*/
        //mm_bind_blocks_for_allocation(block_meta_data, next_block_meta_data);
    }

     /*Case 2 : Full Split  : New Meta block is Created*/
    else {
        /*New Meta block is to be created*/
        next_block_meta_data = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
        next_block_meta_data->is_free = MM_TRUE;
        next_block_meta_data->block_size =
            remaining_size - sizeof(block_meta_data_t);
        next_block_meta_data->offset = block_meta_data->offset +
            sizeof(block_meta_data_t) + block_meta_data->block_size;
        init_glthread(&next_block_meta_data->priority_thread_glue);
        mm_add_free_block_meta_data_to_free_block_list(
                vm_page_family, next_block_meta_data);
        mm_bind_blocks_for_allocation(block_meta_data, next_block_meta_data);
    }

    return MM_TRUE;

}

// fucntion to add free meta block to the priority list
static void 
mm_add_free_block_meta_data_to_free_block_list(
        vm_page_family_t *vm_page_family, 
        block_meta_data_t *free_block
) {     
        assert(free_block->is_free == MM_TRUE);

        glthread_priority_insert(&vm_page_family->free_block_priority_list_head, 
                        &free_block->priority_thread_glue, 
                        free_blocks_comparison_function, 
                        offset_of(block_meta_data_t, priority_thread_glue));
}



// function to find the biggest free meta block from PQ and if the size of biggest block is less than 
// the memory req than request a new data vm page

static block_meta_data_t *
mm_allocate_free_data_block(
        vm_page_family_t *vm_page_family,
        uint32_t req_size){ 

    vm_bool_t status = MM_FALSE;
    vm_page_t *vm_page = NULL;
    block_meta_data_t *block_meta_data = NULL;


    block_meta_data_t *biggest_block_meta_data = 
        mm_get_biggest_free_block_page_family(vm_page_family); 

    if(!biggest_block_meta_data || 
        biggest_block_meta_data->block_size < req_size){

        /*Time to add a new page to Page family to satisfy the request*/
        vm_page = mm_family_new_page_add(vm_page_family);

        /*Allocate the free block from this page now*/
        status = mm_split_free_data_block_for_allocation(vm_page_family, 
                    &vm_page->block_meta_data, req_size);

        if(status)
            return &vm_page->block_meta_data;

        return NULL;
    }

    /*Step 3*/
    /*The biggest block meta data can satisfy the request*/

    if(biggest_block_meta_data){
        status = mm_split_free_data_block_for_allocation(vm_page_family, 
                biggest_block_meta_data, req_size);
    }

    if(status)
        return biggest_block_meta_data;
    
    return NULL;


}


// The public function invoked by the application for Dynamic Mem. Alloc.

void *
xcalloc(char* struct_name, int units){

    vm_page_family_t *pg_family = lookup_page_family_by_name(struct_name);

    if(!pg_family){
        printf("Error: Structure %s not registered with Memory Manager\n", 
                struct_name);

        return NULL;
    }

     if(units * pg_family->struct_size > MAX_PAGE_ALLOCATABLE_MEMORY(1)){
        
        printf("Error : Memory Requested Exceeds Page Size\n");
        assert(0);
        return NULL;
    }

     /*Find the page which can satisfy the request*/
    block_meta_data_t *free_block_meta_data = NULL;
    
    free_block_meta_data = mm_allocate_free_data_block(
                            pg_family, units * pg_family->struct_size);

    if(free_block_meta_data){
        memset((char *)(free_block_meta_data + 1), 0, free_block_meta_data->block_size);
        return  (void *)(free_block_meta_data + 1);
    }

    return NULL;
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