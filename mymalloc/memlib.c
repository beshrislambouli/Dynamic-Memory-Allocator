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

/*
 * memlib.c - a module that simulates the memory system.  Needed because it
 *            allows us to interleave calls from the student's malloc package
 *            with the system's malloc package in libc.
 */
#include "./memlib.h"

#include <assert.h>
#include <errno.h>
#include <immintrin.h>  // _mm_malloc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "./config.h"

/* private variables */
static char* mem_start_brk; /* points to first byte of heap */
static char* mem_brk;       /* points to first byte after the end of the heap */
static char* mem_max_addr;  /* largest legal heap address */

/*
 * mem_init - initialize the memory system model
 */
void mem_init(void) {
  /* allocate the storage we will use to model the available VM */
  if ((mem_start_brk = (char*)_mm_malloc(MAX_HEAP, 4096)) == NULL) {
    fprintf(stderr, "mem_init_vm: malloc error\n");
    exit(1);
  }

  mem_max_addr = mem_start_brk + MAX_HEAP; /* max legal heap address */
  mem_brk = mem_start_brk;                 /* heap is empty initially */

  memset(mem_start_brk, 0,
         MAX_HEAP); /* Zero out memory to prevent page faults */
}

/*
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void) { _mm_free(mem_start_brk); }

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk(void) { mem_brk = mem_start_brk; }

/*
 * mem_sbrk - simple model of the sbrk function. Extends the heap
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 */
void* mem_sbrk(unsigned int incr) {
  if ((mem_brk + incr) > mem_max_addr) {
    errno = ENOMEM;
    fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory... (%ld)\n",
            mem_heapsize());
    return (void*)-1;
  }
  char* old_brk = mem_brk;
  mem_brk += incr;
  return (void*)old_brk;
}

/*
 * mem_heap_lo - return address of the first heap byte
 */
void* mem_heap_lo(void) { return (void*)mem_start_brk; }

/*
 * mem_heap_hi - returns the address of the last byte in the heap.
 */
void* mem_heap_hi(void) { return (void*)(mem_brk - 1); }

/*
 * mem_heapsize() - returns the heap size in bytes
 */
size_t mem_heapsize(void) { return (size_t)(mem_brk - mem_start_brk); }

/*
 * mem_pagesize() - returns the page size of the system
 */
size_t mem_pagesize(void) { return (size_t)getpagesize(); }
