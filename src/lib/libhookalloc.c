#include <bits/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/syscall.h>

#include "mcap.h"

static int reentrancy_guard = 0;
static int log_open = 0;
static int log_file = -1;

static void *(*sys_malloc)(size_t) = NULL;
static void *(*sys_calloc)(size_t, size_t) = NULL;
static void *(*sys_realloc)(void*, size_t) = NULL;
static void (*sys_free)(void*) = NULL;

static uint64_t get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static void get_proc_name(char *name, size_t buflen) {
    int file = open("/proc/self/comm", O_RDONLY);
    if (file == -1) {
        name[0] = '\0';
        return;
    }
    size_t bytes = read(file, name, buflen);
    name[bytes - 1] = '\0';
    close(file);
}

static void get_proc_cmd(char *cmd, size_t buflen) {
    int file = open("/proc/self/cmdline", O_RDONLY);
    if (file == -1) {
        cmd[0] = '\0';
        return;
    }
    size_t bytes = read(file, cmd,buflen - 1);
    cmd[bytes] = '\0';
    for(size_t i = 0; i < bytes; i++) {
        if(cmd[i] == '\0')
            cmd[i] = ' ';
    }
    close(file);
}

static void init_logging() {
    if(log_open)
        return;
    const char *log_filename = getenv("HOOK_LOG_FILE");
    if(log_filename != NULL) {
        log_file = open(log_filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    } 
    if(log_file == -1) { // fallback to default filename if envvar is not set or fopen fails
        log_file = open("memory_events.mcap", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    }
    log_open = 1;
    char pname[32];
    get_proc_name(pname, 32);
    struct mcap_meta_entry_header pname_hdr = {
        .type = MCAP_PROCESS_NAME,
        .length = strlen(pname)
    };
    char pcmd[64];
    get_proc_cmd(pcmd, 64);
    struct mcap_meta_entry_header pcmd_hdr = {
        .type = MCAP_COMMAND,
        .length = strlen(pcmd)
    };
    struct mcap_file_header fh = {
        .magic = MCAP_MAGIC,
        .start_time = get_time(),
        .version_major = 1,
        .version_minor = 0,
        .metadata_len = sizeof(pname_hdr) + strlen(pname) + sizeof(pcmd_hdr) + strlen(pcmd)
    };

    write(log_file, &fh, sizeof(fh));
    write(log_file, &pname_hdr, sizeof(pname_hdr));
    write(log_file, &pname, strlen(pname));
    write(log_file, &pcmd_hdr, sizeof(pcmd_hdr));
    write(log_file, &pcmd, strlen(pcmd));
    //fflush(log_file);
}

static void hook_malloc() {
    if(sys_malloc)
        return;

    // Load system function
    sys_malloc = dlsym(RTLD_NEXT, "malloc");
}
void *malloc(size_t size) {
    if (reentrancy_guard) { // Don't recurse
        return sys_malloc(size);
    }
    hook_malloc();
    reentrancy_guard = 1;
    void *addr = sys_malloc(size);
    init_logging();
    struct mcap_event_header eh = {
        .type = MCAP_ALLOC,
        .timestamp = get_time(),
        .length = sizeof(struct mcap_alloc)
    };
    struct mcap_alloc alloc_e = {
        .addr = (uint64_t)addr,
        .size = size,
        .tid = syscall(SYS_gettid),
        .type_pad = 0
    };
    write(log_file, &eh, sizeof(eh));
    write(log_file, &alloc_e, sizeof(alloc_e));
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
    if (reentrancy_guard) { // Don't recurse
        return sys_calloc(n, size);
    }
    hook_calloc();
    reentrancy_guard = 1;
    void *addr = sys_calloc(n, size);
    init_logging();
    struct mcap_event_header eh = {
        .type = MCAP_ALLOC,
        .timestamp = get_time(),
        .length = sizeof(struct mcap_alloc)
    };
    struct mcap_alloc alloc_e = {
        .addr = (uint64_t)addr,
        .size = size * n,
        .tid = syscall(SYS_gettid),
        .type_pad = 1
    };
    write(log_file, &eh, sizeof(eh));
    write(log_file, &alloc_e, sizeof(alloc_e));
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
    if (reentrancy_guard) { // Don't recurse
        return sys_realloc(p, size);
    }
    hook_realloc();
    reentrancy_guard = 1;
    void *addr = sys_realloc(p, size);
    init_logging();
    struct mcap_event_header eh = {
        .type = MCAP_REALLOC,
        .timestamp = get_time(),
        .length = sizeof(struct mcap_realloc)
    };
    struct mcap_realloc realloc_e = {
        .addr_source = (uint64_t)p,
        .addr_dest = (uint64_t)addr,
        .size = size,
        .tid = syscall(SYS_gettid),
    };
    write(log_file, &eh, sizeof(eh));
    write(log_file, &realloc_e, sizeof(realloc_e));
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
    if (reentrancy_guard) { // Don't recurse
        return sys_free(p);
    }
    hook_free();
    reentrancy_guard = 1;
    sys_free(p);
    init_logging();
    struct mcap_event_header eh = {
        .type = MCAP_FREE,
        .timestamp = get_time(),
        .length = sizeof(struct mcap_free)
    };
    struct mcap_free free_e = {
        .addr = (uint64_t)p,
        .tid = syscall(SYS_gettid),
    };
    write(log_file, &eh, sizeof(eh));
    write(log_file, &free_e, sizeof(free_e));
    reentrancy_guard = 0;
}
