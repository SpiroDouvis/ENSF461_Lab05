#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "myalloc.h"
#include <stddef.h>

void* _arena_start;
size_t _arena_size;

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