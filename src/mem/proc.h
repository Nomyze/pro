#pragma once

#include <sys/types.h>

typedef struct memory_region {
    void *start;
    void *end;
    char *pathname;
} memory_region;

typedef struct process {
    unsigned int pid;
    int mem_file_fd;
    size_t reg_count;
    memory_region *regions;
} process;

typedef struct node {
    memory_region reg;
    struct node *next;
} node;


void bind_proc_by_name(process* proc, char* str);
void populate_regions(process *proc);
void open_memory_file(process *proc, int flags);
void close_memory_file(process *proc);


off_t find_first_buffern(void *haystack, size_t length, void *buf, size_t n, off_t *offset);
void find_buffern(process *proc, void *buf, size_t n, off_t **out_off, size_t *size);
void find_int32_t(process *proc, int32_t val, off_t **out_off, size_t *size);
void printn_at(process *proc, int n, off_t offset);
void writen_to(process *proc, void *buffer, int n, off_t offset);
void write_int32_t_to(process *proc, int32_t val, off_t offset);

int filter_addrs(off_t *buffer, size_t n, off_t *filter, size_t fn, off_t *filtered);
void destroy_proc(process *proc);
