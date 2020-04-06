#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

#include "hmalloc.h"

/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/

void*
map(size_t size){
    return mmap(0,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
}

typedef struct node{
    size_t size;
    struct node* next;
}node;

const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.
static node* head;

long
free_list_length()
{
    // TODO: Calculate the length of the free list.
    long ret_val = 0;
    node* current = head;
    while(current != NULL){
        ret_val++;
        current = current->next;
    }
    return ret_val;
}

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void
insert(node* n){
    //If empty
    if(head == NULL){
        head = n;
        return;
    }
    node* curr = head;
    node* prev = NULL;
    //Loop through list
    while(curr != NULL){
        if((void*)curr>(void*)n){
            size_t prev_size = 0;
            if(prev != NULL){
                prev_size = prev->size;
            }
            //Combine prev, n, and curr
            if((void*)prev+prev_size == (void*)n && (void*)n + n->size == (void*)curr){
                prev->size = prev->size + n->size + curr->size;
                prev->next = curr->next;
                break;
            }
            //Combine prev and n
            if((void*)prev+prev_size == (void*)n){
                prev->size = prev->size + n->size;
                break;
            }
            //Combine n and curr
            if((void*)n+n->size == (void*)curr){
                n->size = n->size + curr->size;
                if(prev != NULL){
                    prev->next = n;
                }
                n->next = curr->next;
                break;
            }
            //Insert n between prev and n
            if(prev != NULL){
                prev->next = n;
            }
            n->next = curr;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    //If n is head
    if(prev==NULL){
        head = n;
    }
}

void*
hmalloc(size_t size)
{
    stats.chunks_allocated += 1;
    size += sizeof(size_t);
    node* chunk = NULL;
    // TODO: Actually allocate memory with mmap and a free list.
    //If size is less than a page
    if(size < PAGE_SIZE){
        node* curr = head;
        node* prev = NULL;
        //Loop through list until slot opens up
        while(curr != NULL){
            if(curr->size >= size){
                chunk = curr;
                if(prev!=NULL){
                    prev->next = curr->next;
                }
                else{
                    head = curr->next;
                }
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        //If no slot was open, give it a page
        if(chunk == NULL){
            chunk = map(PAGE_SIZE);
            ++stats.pages_mapped;
            chunk->size = PAGE_SIZE;
        }
        //If the open slot was too big, put the extra space back in
        if(chunk->size > size && (chunk->size - size) >= sizeof(node)){
            node* extra = (node*)((void*)chunk + size);
            extra->size = chunk->size - size;
            insert(extra);
            chunk->size = size;
        }
    }
    //If bigger than a page
    else{
        size_t pages = div_up(size,PAGE_SIZE);
        chunk = map(pages*PAGE_SIZE);
        stats.pages_mapped += pages;
        chunk->size = pages*PAGE_SIZE;
        //The instructions don't say to do this, but shouldn't
        //we try to insert the extra again?
        //I tried to insert the below code, but it messed up
        //my test cases.
        /*if(chunk->size > size && (chunk->size - size) >= sizeof(node)){
            node* extra = (node*)((void*)chunk + size);
            extra->size = chunk->size - size;
            insert(extra);
            chunk->size = size;
        }*/
    }
    return (void*)chunk + sizeof(size_t);
}

void
hfree(void* item)
{
    stats.chunks_freed += 1;

    // TODO: Actually free the item.
    node* chunk = item - sizeof(size_t);
    if(chunk->size < PAGE_SIZE){
        insert(chunk);
    }
    else{
        size_t pages = div_up(chunk->size,PAGE_SIZE);
        munmap(chunk, chunk->size);
        stats.pages_unmapped += pages;
    }
}

