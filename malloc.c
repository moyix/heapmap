#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

struct block_meta {
  size_t size;
  struct block_meta *next;
  int free;
  int magic;
};

unsigned int ctr = 0;

void __attribute__((destructor)) cleanup();

#define META_SIZE sizeof(struct block_meta)
void *global_base = NULL;

static char* hexchars[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f" };

void print_hexnum(int fd, uint64_t n) {
    for (int i = 7; i >= 0; i--) {
        unsigned char hi_nibble  = (n >>(i*8 + 4)) & 0xf;
        unsigned char low_nibble = (n >>(i*8)) & 0xf;
        assert(hi_nibble < 16);
        assert(low_nibble < 16);
        write(fd, hexchars[hi_nibble], 1);
        write(fd, hexchars[low_nibble], 1);
    }
}

void walk() {
    write(2, "WALK:\n", 6);
    struct block_meta *head = global_base;
    struct block_meta *last;
    while (head) {
        if (!head->free) {
            print_hexnum(2, (uintptr_t)(head+1) - (uintptr_t)global_base);
            write(2, " ", 1);
            print_hexnum(2, (uintptr_t)(head->size));
            write(2, "\n", 1);
        }
        last = head;
        head = head->next;
    }

    write(2, "EDGES:\n", 7);
    for (unsigned char *p = (unsigned char *)global_base;
        p < ((unsigned char *)last + sizeof(*last) + last->size); 
        p += 8) {
        uint64_t value = *(uint64_t *)p;
        if ((uintptr_t)global_base <= value &&
            value < (uintptr_t)((unsigned char *)last + sizeof(*last) + last->size)) {
            print_hexnum(2, (uintptr_t)p - (uintptr_t)global_base);
            write(2, " ", 1);
            print_hexnum(2, value - (uintptr_t)global_base);
            write(2, "\n", 1);
        }
    }
}

void cleanup() {
    walk();
}

struct block_meta *find_free_block(struct block_meta **last, size_t size) {
  struct block_meta *current = global_base;
  while (current && !(current->free && current->size >= size)) {
    *last = current;
    current = current->next;
  }
  return current;
}

struct block_meta *request_space(struct block_meta* last, size_t size) {
  struct block_meta *block;
  block = sbrk(0);
  void *request = sbrk(size + META_SIZE);
  assert((void*)block == request); // Not thread safe.
  if (request == (void*) -1) {
    return NULL; // sbrk failed.
  }

  if (last) { // NULL on first request.
    last->next = block;
  }
  block->size = size;
  block->next = NULL;
  block->free = 0;
  block->magic = 0x12345678;
  return block;
}

void *malloc(size_t size) {
  struct block_meta *block;
  // TODO: align size?

  if (size <= 0) {
    return NULL;
  }

  if (!global_base) { // First call.
    block = request_space(NULL, size);
    if (!block) {
      return NULL;
    }
    global_base = block;
  } else {
    struct block_meta *last = global_base;
    block = find_free_block(&last, size);
    if (!block) { // Failed to find free block.
      block = request_space(last, size);
      if (!block) {
        return NULL;
      }
    } else {      // Found free block
      // TODO: consider splitting block here.
      block->free = 0;
      block->magic = 0x77777777;
    }
  }

  if (++ctr % 10 == 0) walk();

  return(block+1);
}

struct block_meta *get_block_ptr(void *ptr) {
  return (struct block_meta*)ptr - 1;
}

void free(void *ptr) {
  if (!ptr) {
    return;
  }

  // TODO: consider merging blocks once splitting blocks is implemented.
  struct block_meta* block_ptr = get_block_ptr(ptr);
  assert(block_ptr->free == 0);
  assert(block_ptr->magic == 0x77777777 || block_ptr->magic == 0x12345678);
  block_ptr->free = 1;
  block_ptr->magic = 0x55555555;
}

void *realloc(void *ptr, size_t size) {
  if (!ptr) {
    // NULL ptr. realloc should act like malloc.
    return malloc(size);
  }

  struct block_meta* block_ptr = get_block_ptr(ptr);
  if (block_ptr->size >= size) {
    // We have enough space. Could free some once we implement split.
    return ptr;
  }

  // Need to really realloc. Malloc new space and free old space.
  // Then copy old data to new space.
  void *new_ptr;
  new_ptr = malloc(size);
  if (!new_ptr) {
    return NULL; // TODO: set errno on failure.
  }
  memcpy(new_ptr, ptr, block_ptr->size);
  free(ptr);
  return new_ptr;
}

void *calloc(size_t nelem, size_t elsize) {
  size_t size = nelem * elsize; // TODO: check for overflow.
  void *ptr = malloc(size);
  memset(ptr, 0, size);
  return ptr;
}
