#include <stdio.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include <syslog.h>

static int reentrancy_guard = 0;
static int syslog_open = 0;

static void *(*sys_malloc)(size_t) = NULL;
static void *(*sys_calloc)(size_t, size_t) = NULL;
static void *(*sys_realloc)(void*, size_t) = NULL;
static void (*sys_free)(void*) = NULL;

static void init_syslog() {
    if(syslog_open)
        return;
    openlog("malloc_hook", LOG_PID, LOG_LOCAL0);
    syslog_open = 1;
}

static void hook_malloc() {
    if(sys_malloc)
        return;

    // Load system function
    sys_malloc = dlsym(RTLD_NEXT, "malloc");
}
void *malloc(size_t size) {
    init_syslog();
    hook_malloc();
    if (reentrancy_guard) { // Don't recurse
        return sys_malloc(size);
    }
    reentrancy_guard = 1;
    void *addr = sys_malloc(size);
    syslog(LOG_NOTICE, "Called malloc, got addr: %p, for size: %zu", addr, size);
    reentrancy_guard = 0;
    return addr;
}

static void hook_calloc() {
    if(sys_calloc)
        return;

    // Load system function
    sys_calloc = dlsym(RTLD_NEXT, "calloc");
}
void *calloc(size_t n, size_t size) {
    init_syslog();
    hook_calloc();
    if (reentrancy_guard) { // Don't recurse
        return sys_calloc(n, size);
    }
    reentrancy_guard = 1;
    void *addr = sys_calloc(n, size);
    syslog(LOG_NOTICE, "Called calloc, got addr: %p, for %zu elements of size %zu", addr, n, size);
    reentrancy_guard = 0;
    return addr;
}

static void hook_realloc() {
    if(sys_realloc)
        return;

    // Load system function
    sys_realloc = dlsym(RTLD_NEXT, "realloc");
}
void *realloc(void* p, size_t size) {
    init_syslog();
    hook_realloc();
    if (reentrancy_guard) { // Don't recurse
        return sys_realloc(p, size);
    }
    reentrancy_guard = 1;
    void *addr = sys_realloc(p, size);
    syslog(LOG_NOTICE, "Called realloc, got addr: %p, for prior addr %p, resizing to %zu", addr, p, size);
    reentrancy_guard = 0;
    return addr;
}

static void hook_free() {
    if(sys_free)
        return;

    // Load system function
    sys_free = dlsym(RTLD_NEXT, "free");
}
void free(void* p) {
    init_syslog();
    hook_free();
    if (reentrancy_guard) { // Don't recurse
        return sys_free(p);
    }
    reentrancy_guard = 1;
    sys_free(p);
    syslog(LOG_NOTICE, "Called free on addr %p", p);
    reentrancy_guard = 0;
}
