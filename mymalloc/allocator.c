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

#define NUM_BINS 26

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


// init - Initialize the malloc package.  Called once before any other
// calls are made.  Since this is a very simple implementation, we just
// return success.
int my_init() {
  for (int i = 0; i < NUM_BINS; i++) {
    freelists[i] = NULL;
  }
  return 0; 
}

//  malloc - Allocate a block by incrementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.
void* my_malloc(size_t size) {
  // We allocate a little bit of extra memory so that we can store the
  // size of the block we've allocated.  Take a look at realloc to see
  // one example of a place where this can come in handy.
  int aligned_size = ALIGN(size + SIZE_T_SIZE);

  size_t index = 0;
  int power = 1;
  while (power < aligned_size) {
    power *= 2;
    index++;
  }
  size_t goal = index;
  while (index < NUM_BINS && freelists [index] == NULL) {
    index ++;
  }

  if (index < NUM_BINS) {
    node* block = freelists[index];
    freelists[index] = block->next;
    node* ans = NULL;
    
    ans = block;
    *(size_t*)((char*)ans - SIZE_T_SIZE) = goal;
    *((size_t*)((char*)ans - SIZE_T_SIZE) + 1) = 0 ;

    block = ((char*)block + (1<<goal));

    for (int i = goal ; i < index ; i ++ ) {
      *(size_t*)((char*)block - SIZE_T_SIZE) = i;
      *((size_t*)((char*)block - SIZE_T_SIZE) + 1) = 1 ;


      node* new_node = (node*)block;
      new_node->prev = NULL;
      new_node->next = freelists [i];
      if (freelists [i] != NULL) freelists [i]->prev = new_node;
      freelists [i] = new_node;


      block = ((char*)block + (1<<i));
    }

    return (void*)ans;
  }
  
  

  // Expands the heap by the given number of bytes and returns a pointer to
  // the newly-allocated area.  This is a slow call, so you will want to
  // make sure you don't wind up calling it on every malloc.


  void* p = mem_sbrk((1<<goal));

  if (p == (void*)-1) {
    // Whoops, an error of some sort occurred.  We return NULL to let
    // the client code know that we weren't able to allocate memory.
    return NULL;
  } else {
    // We store the size of the block we've allocated in the first
    // SIZE_T_SIZE bytes.
    *(size_t*)p = goal;
    *((size_t*)p + 1) = 0;

    // Then, we return a pointer to the rest of the block of memory,
    // which is at least size bytes long.  We have to cast to uint8_t
    // before we try any pointer arithmetic because voids have no size
    // and so the compiler doesn't know how far to move the pointer.
    // Since a uint8_t is always one byte, adding SIZE_T_SIZE after
    // casting advances the pointer by SIZE_T_SIZE bytes.
    return (void*)((char*)p + SIZE_T_SIZE);
  }
}

// free - Freeing a block does nothing.
void my_free(void* p) {
  size_t index = *(size_t*)((char*)p - SIZE_T_SIZE);
  
  while (1) {
    node* goal = (node*)((char*)p + (1 << index));
    if ( *((size_t*)((char*)goal - SIZE_T_SIZE) + 1) == 1 && *((size_t*)((char*)goal - SIZE_T_SIZE)) == index ) {

      *((size_t*)((char*)goal - SIZE_T_SIZE) + 1) = 0;

      if (goal == freelists [index]) freelists [index] = goal -> next;

      if (goal->next != NULL) goal->next->prev = goal->prev;

      if (goal->prev != NULL) goal->prev->next = goal->next;

      index ++;
    }
    else break;
  }

  *(size_t*)((char*)p - SIZE_T_SIZE) = index;
  *((size_t*)((char*)p - SIZE_T_SIZE) + 1) = 1 ;


  node* new_node = (node*)p;
  new_node->prev = NULL;
  new_node->next = freelists [index];
  if (freelists [index] != NULL)
        freelists [index]->prev = new_node;
  freelists [index] = new_node;
}


// realloc - Implemented simply in terms of malloc and free
void* my_realloc(void* ptr, size_t size) {
  if (!ptr) return my_malloc(size);

  void* newptr;
  size_t copy_size;

  // Allocate a new chunk of memory, and fail if that allocation fails.
  newptr = my_malloc(size);
  if (NULL == newptr) {
    return NULL;
  }

  // Get the size of the old block of memory.  Take a peek at my_malloc(),
  // where we stashed this in the SIZE_T_SIZE bytes directly before the
  // address we returned.  Now we can back up by that many bytes and read
  // the size.
  copy_size = *(size_t*)((uint8_t*)ptr - SIZE_T_SIZE);

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
  return newptr;
}
