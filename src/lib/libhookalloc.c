#include <bits/time.h>
#include <stddef.h>
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
#include <linux/userfaultfd.h>
#include <pthread.h>

#include "mcap.h"

static int reentrancy_guard = 0;

static void *(*sys_malloc)(size_t) = NULL;
static void *(*sys_calloc)(size_t, size_t) = NULL;
static void *(*sys_realloc)(void*, size_t) = NULL;
static void (*sys_free)(void*) = NULL;

typedef struct tracked_region {
    uint64_t addr;
    uint64_t size;
    void *data;
    uint64_t snapshot_num;
    int is_userfault_registered;
    struct tracked_region *next;
} tracked_region_t;

typedef struct cached_write {
    uint64_t offset;
    uint64_t size;
    void *data;
    struct cached_write *next;
} cached_write_t;

typedef struct write_batch {
    tracked_region_t *reg;
    cached_write_t *write_h;
    uint32_t write_num;
    struct write_batch *next;
} write_batch_t;

static struct {
    int mcap_fd;
    uint64_t start_time;
    int userfaultfd;
    pthread_t userfault_th;
    
    pthread_mutex_t alloc_lock;
    tracked_region_t *reg_h;
    int reg_count;

    pthread_mutex_t write_lock;
    tracked_region_t *write_h;
} global_state = {
    .write_lock = PTHREAD_MUTEX_INITIALIZER,
    .alloc_lock = PTHREAD_MUTEX_INITIALIZER,
    .mcap_fd = -1
};

static inline uint64_t get_time() {
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

static void mcap_write_hdr() {
    global_state.start_time = get_time();
    struct mcap_file_header hdr = {
        .magic = MCAP_MAGIC,
        .version_major = 1,
        .version_minor = 0,
        .start_time = global_state.start_time,
        .metadata_len = 0
    };
    
    off_t hdr_pos = lseek(global_state.mcap_fd, 0, SEEK_CUR);
    write(global_state.mcap_fd, &hdr, sizeof(hdr));

    pid_t pid = getpid();
    struct mcap_meta_entry_header pid_hdr;

    pid_hdr.type = MCAP_PID;
    pid_hdr.length = sizeof(pid);
    write(global_state.mcap_fd, &pid_hdr, sizeof(pid_hdr));
    write(global_state.mcap_fd, &pid, sizeof(pid));

    char pname[256];
    get_proc_name(pname, 256);
    struct mcap_meta_entry_header pname_hdr = {
        .type = MCAP_PROCESS_NAME,
        .length = strlen(pname)
    };
    write(global_state.mcap_fd, &pname_hdr, sizeof(pname_hdr));
    write(global_state.mcap_fd, &pname, strlen(pname));

    char pcmd[256];
    get_proc_cmd(pcmd, 256);
    struct mcap_meta_entry_header pcmd_hdr = {
        .type = MCAP_COMMAND,
        .length = strlen(pcmd)
    };
    
    write(global_state.mcap_fd, &pcmd_hdr, sizeof(pcmd_hdr));
    write(global_state.mcap_fd, &pcmd, strlen(pcmd));

    off_t meta_end = lseek(global_state.mcap_fd, 0, SEEK_CUR);
    uint meta_len = meta_end - hdr_pos - sizeof(hdr);
    lseek(global_state.mcap_fd, hdr_pos + offsetof(struct mcap_file_header, metadata_len), SEEK_SET);
    write(global_state.mcap_fd, &meta_len, sizeof(meta_len));
    lseek(global_state.mcap_fd, meta_end, SEEK_SET);
}

static void init_logging() {
    if(global_state.mcap_fd != -1)
        return;
    const char *log_filename = getenv("HOOK_LOG_FILE");
    if(log_filename != NULL) {
        global_state.mcap_fd = open(log_filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    } 
    if(global_state.mcap_fd == -1) { // fallback to default filename if envvar is not set or fopen fails
        global_state.mcap_fd = open("memory_events.mcap", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    }

    mcap_write_hdr();
    //fflush(log_file);
}

static void mcap_write_alloc(uint64_t addr, uint64_t size, int calloc) {
    pthread_mutex_lock(&global_state.write_lock);
    struct mcap_event_header eh = {
        .type = MCAP_ALLOC,
        .timestamp = get_time(),
        .length = sizeof(struct mcap_alloc)
    };
    struct mcap_alloc alloc_e = {
        .addr = addr,
        .size = size,
        .tid = syscall(SYS_gettid),
        .type_pad = calloc
    };
    write(global_state.mcap_fd, &eh, sizeof(eh));
    write(global_state.mcap_fd, &alloc_e, sizeof(alloc_e));
    pthread_mutex_unlock(&global_state.write_lock);
}

static void mcap_write_free(uint64_t addr) {
    pthread_mutex_lock(&global_state.write_lock);
    struct mcap_event_header eh = {
        .type = MCAP_FREE,
        .timestamp = get_time(),
        .length = sizeof(struct mcap_free)
    };
    struct mcap_free free_e = {
        .addr = addr,
        .tid = syscall(SYS_gettid),
    };
    write(global_state.mcap_fd, &eh, sizeof(eh));
    write(global_state.mcap_fd, &free_e, sizeof(free_e));
    pthread_mutex_unlock(&global_state.write_lock);
}

static void mcap_write_realloc(uint64_t p, uint64_t addr, uint64_t size) {
    pthread_mutex_lock(&global_state.write_lock);
    struct mcap_event_header eh = {
        .type = MCAP_REALLOC,
        .timestamp = get_time(),
        .length = sizeof(struct mcap_realloc)
    };
    struct mcap_realloc realloc_e = {
        .addr_source = p,
        .addr_dest = addr,
        .size = size,
        .tid = syscall(SYS_gettid),
    };
    write(global_state.mcap_fd, &eh, sizeof(eh));
    write(global_state.mcap_fd, &realloc_e, sizeof(realloc_e));
    pthread_mutex_unlock(&global_state.write_lock);
}

static void mcap_write_write_batch(write_batch_t *batch) {
    pthread_mutex_lock(&global_state.write_lock);
    tracked_region_t *reg = batch->reg;

    uint32_t len = sizeof(struct mcap_write_batch);
    for(cached_write_t *write_node = batch->write_h; write_node; write_node = write_node->next) {
        len += sizeof(struct mcap_write) + write_node->size;
    }
    struct mcap_event_header e_hdr = {
        .type = MCAP_WRITE_DIFF,
        .timestamp = get_time() - global_state.start_time,
        .length = len
    };
    struct mcap_write_batch b_hdr = {
        .addr = reg->addr,
        .tid = syscall(SYS_gettid),
        .batches = batch->write_num,
        .snapshot_num = reg->snapshot_num++
    };

    write(global_state.mcap_fd, &e_hdr, sizeof(e_hdr));
    write(global_state.mcap_fd, &b_hdr, sizeof(b_hdr));

    for(cached_write_t *write_node = batch->write_h; write_node; write_node = write_node->next) {
        struct mcap_write b_data = {
            .offset = write_node->offset,
            .size = write_node->size
        };
        write(global_state.mcap_fd, &b_data, sizeof(b_data));
        write(global_state.mcap_fd, write_node->data, write_node->size);
    }
    for(cached_write_t *write_node = batch->write_h; write_node; write_node = write_node->next) { // we COWing
        memcpy(reg->data + write_node->offset, write_node->data, write_node->size);
    }
    pthread_mutex_unlock(&global_state.write_lock);
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
    mcap_write_alloc((uint64_t)addr, size, 0);
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
    mcap_write_alloc((uint64_t)addr, size * n, 1);
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
    mcap_write_realloc((uint64_t)p, (uint64_t)addr, size);
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
    mcap_write_free((uint64_t)p);
    reentrancy_guard = 0;
}
