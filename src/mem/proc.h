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
    int reg_count;
    memory_region *regions;
} process;

typedef struct node {
    memory_region reg;
    struct node *next;
} node;


void populate_regions(process *proc);
void open_memory_file(process *proc, int flags);
void close_memory_file(process *proc);

off_t find_first_buffern(void *haystack, size_t length, void *buf, size_t n, off_t *offset);
off_t *find_buffern(process *proc, void *buf, size_t n);
void printn_at(int fd, int n, off_t offset);
void writen_to(int fd, void *buffer, int n, off_t offset);
int filter_addrs(off_t *buffer, int n, off_t *filter, int fn, off_t *filtered);
