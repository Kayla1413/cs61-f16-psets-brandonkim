#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

typedef struct m61_statistics stats_struct;
typedef struct m61_statistics_metadata stats_meta;
typedef struct m61_tail {
  unsigned int tl;
} m61_tail;


static stats_struct stats_global = {0, 0, 0, 0, 0, 0, 0, 0 };

stats_meta* global_meta;

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    stats_meta* meta_ptr;
    void* ret_ptr;
    void* end_ptr;  // pointer to end of region, for heap_max

    if (sizeof(stats_meta) + sz >= sz) {
        meta_ptr = base_malloc(sz + sizeof(stats_meta) + sizeof(m61_tail)); // add space for metadata
    } else {
        meta_ptr = NULL; //gets rid of always-true warning
    }

    if (meta_ptr == NULL) {
        stats_global.nfail++;
        stats_global.fail_size += (unsigned long long) sz;
        return meta_ptr;
    }

    end_ptr = ((char*) meta_ptr) + sz + sizeof(stats_meta);
    m61_tail tail = {0xFEEDFEED};
    memmove(((char*) meta_ptr) + sz + sizeof(stats_meta),&tail, sizeof(m61_tail));

    if ((meta_ptr <= (stats_meta*) stats_global.heap_min) || stats_global.heap_min == 0) {
        stats_global.heap_min = (char*) meta_ptr;
    }

    if (end_ptr >= (void*) stats_global.heap_max) {
        stats_global.heap_max = (char*) end_ptr;
    }

    meta_ptr->cur = meta_ptr;
    meta_ptr->prv = NULL;
    meta_ptr->nxt = NULL;
    meta_ptr->deadbeef = 0x0CAFEBABE;
    meta_ptr->alloc_size = sz;
    meta_ptr->file = file;
    meta_ptr->line = line;
    meta_ptr->size = sz;
    // simply updating stats

    if (global_meta) {
        global_meta->cur->nxt = meta_ptr;
        meta_ptr->prv = global_meta->cur;
        global_meta = meta_ptr;
    } else {
        global_meta = meta_ptr;
    }

    stats_global.nactive++;
    stats_global.active_size += (unsigned long long) sz;
    stats_global.ntotal++;
    stats_global.total_size += (unsigned long long) sz;
    ret_ptr = meta_ptr + 1;
    return ret_ptr;
}


/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc and friends. If
///    `ptr == NULL`, does nothing. The free was called at location
///    `file`:`line`.

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    unsigned long long size;
    stats_meta* meta_ptr = ((stats_meta*) ptr) - 1;
    stats_meta* actv_global = global_meta;
    if (ptr == NULL) return;
    if (ptr > (void*) stats_global.heap_max || ptr < (void*) stats_global.heap_min) {
        printf("MEMORY BUG: %s:%d: invalid free of pointer %p, not in heap\n", file, line, ptr);
        abort();
    }
    while ((actv_global != NULL) && (ptr != actv_global->cur + 1)){
        if (((void*) (actv_global->cur + 1) < ptr) && (ptr < (void*) (actv_global->cur + 1 + actv_global->size))) {
            printf("MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n  %s:%d: %p: %p is %d bytes inside a %zu byte region allocated here\n", 
                   file, line , ptr, actv_global->file, actv_global->line, actv_global, ptr, (int) (ptr - ((void*) (actv_global->cur + 1))),actv_global->size);
            abort();ß
        }
        actv_global = actv_global->prv;
    }
    m61_tail* tail = (m61_tail*) ((char*) ptr + (meta_ptr->alloc_size));
    if (meta_ptr->deadbeef == 0x0DEADBEEF) {
        printf("MEMORY BUG: %s:%d: invalid free of pointer %p, double free ya dingus\n", file, line, ptr);
        abort();
    }
    if (tail->tl != 0xFEEDFEED) {
        printf("MEMORY BUG %s:%d: detected wild write during free of pointer %p",file, line, ptr);
        abort();
    }
    if (meta_ptr->deadbeef != 0x0CAFEBABE) {
        printf("MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n",file, line, ptr);
        abort();
    }
    meta_ptr->deadbeef = 0x0DEADBEEF;

    if (meta_ptr->nxt)
        meta_ptr->nxt->prv = meta_ptr->prv;

    if (meta_ptr->prv) {
        meta_ptr->prv->nxt = meta_ptr->nxt;
        if (global_meta == meta_ptr)
            global_meta = meta_ptr->prv;
    }

    if (!(meta_ptr->nxt) && !(meta_ptr->prv))
        global_meta = NULL;

    size = meta_ptr->alloc_size;
    stats_global.active_size -= (unsigned long long) size;
    stats_global.nactive--;
    base_free(ptr);
}


/// m61_realloc(ptr, sz, file, line)
///    Reallocate the dynamic memory pointed to by `ptr` to hold at least
///    `sz` bytes, returning a pointer to the new block. If `ptr` is NULL,
///    behaves like `m61_malloc(sz, file, line)`. If `sz` is 0, behaves
///    like `m61_free(ptr, file, line)`. The allocation request was at
///    location `file`:`line`.

void* m61_realloc(void* ptr, size_t sz, const char* file, int line) {
    (void) file, (void) line;
    void* new_ptr = NULL;
    stats_meta* meta_ptr = ((stats_meta*) ptr) - 1;
    if (sz)
        new_ptr = m61_malloc(sz, file, line);
    if (ptr && new_ptr) {
        // Copy the data from `ptr` into `new_ptr`.
        // To do that, we must figure out the size of allocation `ptr`.
        // Your code here (to fix test012).
        size_t old_sz = meta_ptr->alloc_size;
        if (old_sz < sz) {
            memcpy(new_ptr,ptr,meta_ptr->alloc_size);
        } else {
            memcpy(new_ptr,ptr,sz);
        }

    }
    //if (meta_ptr->alloc_size > sz) {
    //    printf("MEMORY BUG: %s:%d: invalid realloc of pointer %p, size too big\n", file, line, ptr);
    //    abort();
    //}
    m61_free(ptr, file, line);
    return new_ptr;
}


/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. The memory
///    is initialized to zero. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.

void* m61_calloc(size_t nmemb, size_t sz, const char* file, int line) {
    // Your code here (to fix test014).
    void* ptr = NULL;
    if (nmemb * sz >= nmemb) {
        ptr = m61_malloc(nmemb * sz, file, line);
        memset(ptr, 0, nmemb * sz);
    } else {
        stats_global.nfail++;
        stats_global.fail_size += sz * nmemb;
    }
    return ptr;
}



/// m61_getstatistics(stats)
///    Store the current memory statistics in `*stats`.

void m61_getstatistics(struct m61_statistics* stats) {
    // Stub: set all statistics to enormous numbers
    *stats = stats_global;
    // Your code here.
}


/// m61_printstatistics()
///    Print the current memory statistics.

void m61_printstatistics(void) {
    struct m61_statistics stats;
    m61_getstatistics(&stats);

    printf("malloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("malloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_printleakreport()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_printleakreport(void) {
    stats_meta* actv_global = global_meta;
    while (actv_global != NULL) {
        printf("LEAK CHECK: %s:%d: allocated object %p with size %zu\n", actv_global->file, actv_global->line, actv_global + 1, actv_global->size);
        actv_global = actv_global->prv;
    };
}
