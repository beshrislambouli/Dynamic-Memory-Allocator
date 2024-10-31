/**
 * Copyright (c) 2015 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

#include "./allocator.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./allocator_interface.h"
#include "./memlib.h"

// Don't call libc malloc!
// #define malloc(...) (USE_MY_MALLOC)
// #define free(...) (USE_MY_FREE)
// #define realloc(...) (USE_MY_REALLOC)

// check - This checks our invariant that the size_t header before every
// block points to either the beginning of the next block, or the end of the
// heap.

#define NUM_BINS 27

int my_check() {
  char* p;
  char* lo = (char*)mem_heap_lo();
  char* hi = (char*)mem_heap_hi() + 1;
  size_t size = 0;

  p = lo;
  while (lo <= p && p < hi) {
    size = ALIGN(*(size_t*)p + SIZE_T_SIZE);
    p += size;
  }

  if (p != hi) {
    printf("Bad headers did not end at heap_hi!\n");
    printf("heap_lo: %p, heap_hi: %p, size: %lu, p: %p\n", lo, hi, size, p);
    return -1;
  }

  return 0;
}

typedef struct node {
  struct node *next;
  struct node *prev;
} node;

struct node *freelists[NUM_BINS];

void* last;

#define h(p) ((void*)((char*)p - SIZE_T_SIZE))
#define f(p,sz) ((void*)((char*)p + sz - 2*SIZE_T_SIZE))
#define MIN_BLOCK 32

// init - Initialize the malloc package.  Called once before any other
// calls are made.  Since this is a very simple implementation, we just
// return success.
int my_init() {
  for (int i = 0; i < NUM_BINS; i++) {
    freelists[i] = NULL;
  }
  mem_sbrk (12);
  last = NULL;
  return 0; 
}

int get_idx (int sz) {
  int n = 0; 
  while ((1U << n) <= sz) { 
    n++; 
  } 
  return n - 1;
}

void ins (void* p, int sz) {

  int index = get_idx (sz);
  *(int*)h(p) = sz;
  *(int*)f(p,sz) = sz;

  node* new_node = (node*)p;
  new_node->prev = NULL;
  new_node->next = freelists [index];
  if (freelists [index] != NULL) freelists [index]->prev = new_node;
  freelists [index] = new_node;
}

void del (node* p, int sz) {
  
  int index = get_idx (sz);
  
  if (freelists [index] == NULL || p == NULL) return;

  *(int*)f(p,sz) = -1;
  
  if (p == freelists [index]) freelists [index] = p -> next;

  if (p->next != NULL) p->next->prev = p->prev;

  if (p->prev != NULL) p->prev->next = p->next;
  p->next = NULL;
  p->prev = NULL;
}

node* best_fit (int sz) {
  int index = get_idx (sz);
  node* ptr_node = NULL;
  while (index<NUM_BINS) {
    if (freelists [index]==NULL) {
      index ++;
      continue;
    }

    node* cur= freelists [index];
    int mn = (1<<30);
    while (cur != NULL) {
      int cur_sz = *(int*)h(cur);
      if (cur_sz >= sz && cur_sz < mn) {
        mn = cur_sz;
        ptr_node = cur;
        if (mn == (1<<index)) break;
      }
      cur = cur->next;
    }
    if (ptr_node) break;
    index ++;
  }
  return ptr_node;
}

void* normal_sbrk (int aligned_size) {
  void* p = mem_sbrk(aligned_size);
  
  if (p == (void*)-1) {
    return NULL;
  } else {
    last = (void*)((char*)p + SIZE_T_SIZE);

    *(int*)p = aligned_size;
    *(int*)((char*)p + aligned_size - SIZE_T_SIZE) = -1;
    return (void*)((char*)p + SIZE_T_SIZE);
  }
}

void* new_sbrk (int aligned_size) {
  int sz = *(int*)h(last);
  del (last,sz);
  int delta = aligned_size - sz;
  void* p = mem_sbrk(delta);

  *(int*)h(last) = aligned_size;
  *(int*)f(last,aligned_size) = -1;
  return last;
}
//  malloc - Allocate a block by incrementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.
void* my_malloc(size_t size) {
  // printf ("start of maloc\n");
  //printf ("%ld\n",size);
  // We allocate a little bit of extra memory so that we can store the
  // size of the block we've allocated.  Take a look at realloc to see
  // one example of a place where this can come in handy.
  int aligned_size = ALIGN(size + 2 * SIZE_T_SIZE);
  if (aligned_size < MIN_BLOCK ) aligned_size = MIN_BLOCK;

  //printf("a\n");
  node* ptr_node = best_fit (aligned_size);
  if (ptr_node) {
    int old_size = *(int*)h(ptr_node);
    int delta = old_size - aligned_size;
    del (ptr_node,old_size);
    void* ptr = (void*)ptr_node;
    //printf("b\n");
    if (delta >= MIN_BLOCK) {
      void* new_ptr = (char*)ptr + aligned_size;

      *(int*)h(ptr) = aligned_size;
      *(int*)f(ptr,aligned_size) = -1;

      *(int*)h(new_ptr) = delta;
      *(int*)f(ptr,old_size) = -1;
      //printf("c\n");
      my_free (new_ptr);
    }
    // printf ("b ");
    return ptr;
  }
  //printf("e\n");
  

  // Expands the heap by the given number of bytes and returns a pointer to
  // the newly-allocated area.  This is a slow call, so you will want to
  // make sure you don't wind up calling it on every malloc.

  if (last == NULL) {
    // printf ("a ");
    return normal_sbrk (aligned_size);
  }
  else {
    //printf("f\n");
    int sz = *(int*)h(last);
    //printf ("%d ",sz);
    int is_free = *(int*)f(last,sz);
    if (is_free>0) {
      // printf("a");
      //printf("g\n");
      return new_sbrk (aligned_size);
    } 
    else return normal_sbrk (aligned_size);
  }
  
}

void my_free(void* p) {
  if (p == NULL) return ;
  if ( (uint64_t) p > (uint64_t)last) last = p ;
  // printf ("my_Free in\n");

  node* cur = (node*)p;
  int sz = *(int*)h(p);
  int Tot1 = sz;
  int flag_last = (p == last);

  if (((char*)cur + Tot1) <= mem_heap_hi()){ 
    node* goal = (node*)((char*)cur + Tot1);
    sz = *(int*)h(goal);
    int is_free = *(int*)(f(goal,sz));
    if ( is_free > 0) {      
      if (goal == last) flag_last = 1;
      del (goal,sz);
      Tot1 += sz;
    }
  
  }
  
  int Tot2 = 0 ;
  
  if ( ((char*)cur - Tot2 - 2*SIZE_T_SIZE) >= mem_heap_lo () ) {
    int is_free = *(int*)((char*)cur - Tot2 - 2*SIZE_T_SIZE);
    if ( is_free > 0 ) {
      node* goal = (node*)((char*)cur - Tot2 - is_free);
      int sz2 = *(int*)h(goal);
      //if (sz2 != is_free) printf ("wergerg\n");
      del(goal,sz2);
      Tot2 += sz2;
    }
  }

  int Tot = Tot1 + Tot2;
  p = (char*)p - Tot2;
  if (flag_last) last = p;
  ins ((void*)p,Tot);
  // printf ("my_free out\n");
}


// realloc - Implemented simply in terms of malloc and free
void* my_realloc(void* ptr, size_t size) {
  if (!ptr) return my_malloc(size);

  int old_size = *(int*)h(ptr);
  int new_size = ALIGN(size + 2 * SIZE_T_SIZE);
  
  if (old_size >= new_size) {
    int delta = old_size - new_size;
    if (delta < MIN_BLOCK) return ptr;

    void* new_ptr = (char*)ptr + new_size;

    *(int*)h(new_ptr) = delta;
    *(int*)f(ptr,old_size) = -1;

    *(int*)h(ptr) = new_size;
    *(int*)f(ptr,new_size) = -1;
    

    my_free (new_ptr);
    return ptr;
  }
  if (ptr == last) {
    int delta = new_size - old_size;
    mem_sbrk(delta);
    *(int*)h(ptr) = new_size;
    *(int*)f(ptr,new_size) = -1;
    return ptr;
  }
  if ( ((char*)ptr + old_size) <= mem_heap_hi() ) { 

    node* goal = (node*)((char*)ptr + old_size);
    int next_sz = *(int*)h(goal);
    int is_free = *(int*)(f(goal,next_sz));
    if ( is_free > 0 && old_size + next_sz >= new_size ) {
      if (goal == last) last = ptr;
      del (goal,next_sz);
      int delta = (old_size + next_sz) - new_size;
      if (delta < MIN_BLOCK) {
        *(int*)h(ptr) = old_size + next_sz;
        return ptr;
      }

      void* new_ptr = (char*)ptr + new_size;

      *(int*)h(new_ptr) = delta;
      *(int*)f(ptr,old_size) = -1;

      *(int*)h(ptr) = new_size;
      *(int*)f(ptr,new_size) = -1;

      my_free(new_ptr);
      return ptr;
    }
    if (is_free > 0 && goal == last) {
      last = ptr;
      del (goal, next_sz);
      int delta = new_size - (old_size + next_sz);
      mem_sbrk(delta);
      *(int*)h(ptr) = new_size;
      *(int*)f(ptr, new_size) = -1;
      return ptr;
    }
  }

  //printf ("start of realloc\n");
  void* newptr;
  int copy_size;

  // Allocate a new chunk of memory, and fail if that allocation fails.
  //printf ("%ld\n",size);
  newptr = my_malloc(size);
  if (NULL == newptr) {
    return NULL;
  }

  // Get the size of the old block of memory.  Take a peek at my_malloc(),
  // where we stashed this in the SIZE_T_SIZE bytes directly before the
  // address we returned.  Now we can back up by that many bytes and read
  // the size.
  copy_size = *(int*)h(ptr);

  // If the new block is smaller than the old one, we have to stop copying
  // early so that we don't write off the end of the new block of memory.
  if (size < copy_size) {
    copy_size = size;
  }

  // This is a standard library call that performs a simple memory copy.
  memcpy(newptr, ptr, copy_size);

  // Release the old block.
  my_free(ptr);

  // Return a pointer to the new block.
  //printf ("end of realloc\n");
  return newptr;
}
