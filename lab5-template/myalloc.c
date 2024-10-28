#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "myalloc.h"
#include <stddef.h>

void* _arena_start;
size_t _arena_size;
int statusno = ERR_UNINITIALIZED;

int myinit(size_t size) {
    printf("Initializing arena:\n");
    printf("...requested size %zu bytes\n", size);

    // Check for invalid size
    if (size <= 0 || size > MAX_ARENA_SIZE) {
        fprintf(stderr, "...error: requested size is invalid\n");
        return ERR_BAD_ARGUMENTS;
    }

    size_t pagesize = getpagesize();
    printf("...pagesize is %zu bytes\n", pagesize);

    // Adjust size to be a multiple of the page size
    if (size % pagesize != 0) {
        size = ((size / pagesize) + 1) * pagesize;
    }
    printf("...adjusting size with page boundaries\n");
    printf("...adjusted size is %zu bytes\n", size);

    // Request memory using mmap
    _arena_start = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (_arena_start == MAP_FAILED) {
        perror("mmap");
        return ERR_SYSCALL_FAILED;
    }
    _arena_size = size;
    printf("...mapping arena with mmap()\n");
    printf("...arena starts at %p\n", _arena_start);
    printf("...arena ends at %p\n", (char*)_arena_start + size);

    return size; // Return the adjusted size to indicate success
}

int mydestroy() {
    if (_arena_start == NULL) {
        fprintf(stderr, "Destroying Arena:\n...error: cannot destroy uninitialized arena. Setting error status\n");
        return ERR_UNINITIALIZED;
    }

    printf("Destroying Arena:\n");
    printf("...unmapping arena with munmap()\n");

    if (munmap(_arena_start, _arena_size) == -1) {
        perror("munmap");
        return ERR_SYSCALL_FAILED;
    }

    // Reset state variables
    _arena_start = NULL;
    _arena_size = 0;

    return 0; // Return 0 to indicate success
}

void* myalloc(size_t size) {
    if (_arena_start == NULL) {
        statusno = ERR_UNINITIALIZED;
        return NULL;
    }

    node_t* block = (node_t*)_arena_start;
    if (block->size == 0) {
        block->size = _arena_size - sizeof(node_t);
        block->is_free = 1;
        block->fwd = NULL;
        block->bwd = NULL;
    }

    if (size <= 0) {
        return NULL;
    }
    if (_arena_size == 0) {
        statusno = ERR_UNINITIALIZED;
        return NULL;
    }
    if (size + sizeof(node_t) > _arena_size) {
        statusno = ERR_OUT_OF_MEMORY;
        return NULL;
    }

    node_t* prev = NULL;

    while (block != NULL && (block->size < size || block->is_free == 0)) {
        prev = block;
        block = block->fwd;
    }
    if (block == NULL) {
        if ((char*) prev + prev->size + size + sizeof(node_t) < (char*) _arena_start + _arena_size) {
            block = (node_t*) ((char*) prev + prev->size);
            block->size = size;
            block->is_free = 0;
            block->fwd = NULL;
            block->bwd = prev;
            prev->fwd = block;
            statusno = 0;
            return (void*) ((char*) block + sizeof(node_t));
        } else {
            statusno = ERR_OUT_OF_MEMORY;
            return NULL;
        }
    }
    else {
        if (block->size > size + sizeof(node_t)) {
            node_t* new_block = (node_t*)((char*)block + sizeof(node_t) + size);
            new_block->size = block->size - size - sizeof(node_t);
            new_block->is_free = 1;
            new_block->fwd = block->fwd;
            new_block->bwd = block;

            if (block->fwd != NULL) {
            block->fwd->bwd = new_block;
            }

            block->fwd = new_block;
            block->size = size;
        }

        block->is_free = 0;
        statusno = 0;
        return (void*)((char*)block + sizeof(node_t));
    }
}

void myfree(void* ptr) {
    node_t* header = (node_t*)((void*)ptr - sizeof(node_t));
    header->is_free = 1;

    // coalescing

    char changed = 1;
    while (changed) {
        changed = 0;
        node_t* block = _arena_start;
        while (block != NULL && block->fwd != NULL) {
            if (block->is_free) {
                if (block->fwd->is_free) {
                    changed = 1;
                    block->size += block->fwd->size + sizeof(node_t);
                    node_t* temp = block->fwd;
                    block->fwd = block->fwd->fwd;
                    temp->fwd = NULL;
                    temp->bwd = NULL;
                    if (block->fwd != NULL) {
                        block->fwd->bwd = block;
                    }
                }
            }
            block = block->fwd;
        }
    }
}