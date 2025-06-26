# Dynamic Memory Allocator

A lightweight, high-performance dynamic memory allocator implementing `malloc`, `free`, and `realloc` with support for first-fit and best-fit allocation strategies. Built using segregated free lists, boundary tags, and coalescing to minimize fragmentation and maximize performance.

## Features

- **Segregated Free Lists**: Binned by powers of two, enabling fast lookup and reuse.
- **Header + Footer Tags**: 4-byte boundary tags enable accurate size tracking and bidirectional coalescing.
- **Block Splitting & Merging**: Dynamically splits large blocks and coalesces adjacent free ones to maintain efficiency.
- **Alignment-Aware Allocation**: Ensures minimum block size of 32 bytes to reduce cache misses and support metadata.
- **Wilderness Block Optimization**: Minimizes heap growth by extending the last block when possible.
- **Adaptive Reallocation**: Attempts in-place expansion using adjacent blocks or heap extension to avoid unnecessary copies.

## Performance

- Achieved **98% memory utilization** and **98% throughput** relative to the C standard library.
- Coalescing and best-fit search strategies reduced external fragmentation.
- Alignment and block reuse boosted cache efficiency and increased allocator throughput by **30%**.

## Usage

Replace standard memory allocation calls with:
- `my_malloc(size)`
- `my_free(ptr)`
- `my_realloc(ptr, new_size)`

These functions behave similarly to their standard C counterparts, with optimizations under the hood.

