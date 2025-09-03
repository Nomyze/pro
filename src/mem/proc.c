#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>

#include "proc.h"

int is_number(const char *str) {
    if(*str == '\0') return 0;
    while(*str) {
        if(!isdigit(*str)) return 0;
        str++;
    }
    return 1;
}
void bind_proc_by_name(process* proc, char* str) {
    DIR *d_proc = NULL;
    d_proc = opendir("/proc/");
    if(d_proc == NULL) {
        fprintf(stderr, "Failed to open /proc directory\n");
        exit(EXIT_FAILURE);
    }
    struct dirent* entry = NULL;
    while ((entry = readdir(d_proc)) != NULL) {
        if(!is_number(entry->d_name)) {
            continue;
        }
        char *comm_path;
        size_t needed;
        needed = snprintf(NULL, 0, "/proc/%s/comm", entry->d_name);
        comm_path = (char *)malloc(needed + 1);
        snprintf(comm_path, needed + 1, "/proc/%s/comm", entry->d_name);
        FILE *comm_file = NULL;
        comm_file = fopen(comm_path, "r");
        if(comm_file == NULL) {
            fprintf(stderr, "Failed to open proc %s comm file\n", entry->d_name);
        } 
        else {
            char *line = NULL;
            size_t n = 0;
            ssize_t read = getline(&line, &n, comm_file);
            line[read - 1] = '\0';
            if(strcmp(str, line) == 0) {
                printf("Got: %s\n", entry->d_name);
                proc->pid = atoi(entry->d_name);                
                free(comm_path);
                //free(entry);
                closedir(d_proc);
                free(line);
                return;
            }
            free(line);
        }
        free(comm_path);
    }
    //free(entry);
    closedir(d_proc);
}
void populate_regions(process *proc) {
    char *mapspath;
    size_t nneeded;
    nneeded = snprintf(NULL, 0, "/proc/%d/maps", proc->pid);
    mapspath = (char *)malloc(nneeded + 1);
    snprintf(mapspath, nneeded + 1, "/proc/%d/maps", proc->pid);

    FILE *mptr;
    mptr = fopen(mapspath, "r");
    free(mapspath);
    if(mptr == NULL) {
        fprintf(stderr, "Failed to open maps file\n");
        exit(EXIT_FAILURE);
    }
    char *line = NULL;
    size_t n = 0;
    ssize_t chars;
    node *head = NULL;
    int reg_count = 0;
    while((chars = getline(&line, &n, mptr)) != -1) {
        memory_region reg = {};
        //printf("Line: %s", line);
        unsigned long long start, end;
        sscanf(line, "%llx-%llx", &start, &end);
        reg.start = (void *)start;
        reg.end = (void *)end;
        char *chptr = line;
        for(int i = 0; i < 5; i++)
            chptr = strchr(chptr, ' ') + 1;
        while((++chptr)[0] == ' ') {}
        if(chptr[0] == '\0') {
            reg.pathname = "";
        } else {
            char *newline = strchr(chptr, '\n');
            reg.pathname = strndup(chptr, newline - chptr);
        }
        // TODO: FIX THIS
        //if(!strcmp(reg.pathname, "[stack]") || !strcmp(reg.pathname, "[heap]")) {
        node *cur = malloc(sizeof(node));
        cur->reg = reg;
        cur->next = head;
        head = cur;
        reg_count++;
        //}
    }
    fclose(mptr);
    free(line);
    node *cur = head;
    proc->reg_count = reg_count;
    proc->regions = malloc(sizeof(memory_region) * reg_count);
    int i = 0;
    while(cur != NULL) {
        memcpy(&proc->regions[i], &cur->reg, sizeof(memory_region));
        i++;
        node *prev = cur;
        cur = cur->next;
        free(prev);
    }
}

void open_memory_file(process *proc, int flags) {
    char *mempath;
    size_t nneeded;
    nneeded = snprintf(NULL, 0, "/proc/%d/mem", proc->pid);
    mempath = (char *)malloc(nneeded + 1);
    snprintf(mempath, nneeded + 1, "/proc/%d/mem", proc->pid);

    int m_fd;
    m_fd = open(mempath, flags);
    if(m_fd == -1) {
        fprintf(stderr, "Failed to open memory file\nerrno: %d", errno);
        exit(EXIT_FAILURE);
    }
    proc->mem_file_fd = m_fd;
    free(mempath);
}

void close_memory_file(process *proc) {
    if(proc->mem_file_fd != -1) {
        close(proc->mem_file_fd);
        proc->mem_file_fd = -1;
    } else {
        fprintf(stderr, "[PID: %d] Tried closing already closed memory file", proc->pid);
    }
}

off_t find_first_buffern(void *haystack, size_t length, void *buf, size_t n, off_t *offset) {
    void *pos;
    pos = memmem(haystack + *offset, length - *offset, buf, n);
    *offset += (pos == NULL ? length : pos - haystack + n);
    if(pos == NULL) {
        return -1;
    }
    return pos - haystack;
}

void find_buffern(process *proc, void *buf, size_t n, off_t **out_off, size_t *size) {
    if(proc->mem_file_fd == -1) {
        fprintf(stderr, "[PID: %d] No open memory file", proc->pid);
    }
    size_t count = 0;
    *out_off = malloc(sizeof(void*));
    *size = 0;
    for(size_t i = 0; i < proc->reg_count; i++) {
        if(!strcmp(proc->regions[i].pathname, "[stack]") || !strcmp(proc->regions[i].pathname, "[heap]")) {
            size_t length = proc->regions[i].end - proc->regions[i].start + 1;
            off_t offset = (off_t) 0;
            lseek(proc->mem_file_fd, proc->regions[i].start - (void *)0, SEEK_SET);
            char buffer[length] = {};
            ssize_t nbytes = read(proc->mem_file_fd, buffer, length);
            while (offset < nbytes) {
                off_t pos = -1;
                pos = find_first_buffern(buffer, length, buf, n, &offset);

                if(pos > -1) {
                    pos += proc->regions[i].start - (void *)0;
                    if(count >= *size) {
                        (*size)++;
                        off_t *temp_buffer = malloc((*size) * sizeof(void *));
                        memcpy(temp_buffer, *out_off, (count) * sizeof(void *));
                        free(*out_off);
                        (*out_off) = temp_buffer;
                    }
                    count++;
                    (*out_off)[count - 1] = pos;
                    for(size_t i = 0; i < *size; i++) {
                        printf("i: %zu, addr: %lx\n", i, (*out_off)[i]);
                    }
                } else
                    offset += nbytes;
            }
            for(size_t i = 0; i < length; i++) {
                //printf("%02X", ((char *)buffer)[i]);
                if((i + 1) % 16 == 0) {
                    //printf("\n");
                }
            }
        }
        //printf("\nat %llx-%llx\t%s\n\n", proc->regions[i].start, proc->regions[i].end, proc->regions[i].pathname);
    }
}

void find_int32_t(process *proc, int32_t val, off_t **out_off, size_t *size) {
    char *buffer = malloc(sizeof(int32_t));
    memcpy(buffer, &val, sizeof(int32_t));
    find_buffern(proc, buffer, sizeof(int32_t), out_off, size);
    free(buffer);
}

size_t in_region(memory_region *regs, size_t n, off_t offset) {
    for(size_t i = 0; i < n; i++) {
        if((off_t)regs[i].start <= offset && (off_t)regs[i].end >= offset) {
            return i;
        }
    }
    return -1;
}
void printn_at(process *proc, int n, off_t offset) {
    char buffer[n+1];

    lseek(proc->mem_file_fd, offset, SEEK_SET);
    int ret = read(proc->mem_file_fd, buffer, n);
    if(ret == -1) {
        fprintf(stderr, "Failed to print at: %lx, errno: %d", offset, errno);
    }
    buffer[n] = '\0';
    size_t i = in_region(proc->regions, proc->reg_count,offset);
    printf("Printing at offset: %lx, path: %s, found: %s\n", offset, proc->regions[i].pathname, buffer);
    for(size_t i = 0; i < strlen(buffer); i++) {
        printf("%d ", buffer[i]);
    }
    printf("\n");
}

void writen_to(process *proc, void *buffer, int n, off_t offset) {
    lseek(proc->mem_file_fd, offset, SEEK_SET);
    write(proc->mem_file_fd, buffer, n);
}

void write_int32_t_to(process *proc, int32_t val, off_t offset) {
    char *buffer = malloc(sizeof(int32_t));
    memcpy(buffer, &val, sizeof(int32_t));
    writen_to(proc, buffer, sizeof(int32_t), offset);
    free(buffer);
}

int filter_addrs(off_t *buffer, size_t n, off_t *filter, size_t fn, off_t *filtered) {
    int filtered_count = 0;
    for (size_t i = 0; i < fn; i++) {
        printf("Searching: %lx with %lx\n", (unsigned long)buffer, (unsigned long)*(filter + i));
        for(size_t j = 0; j < n; j++) {
            if(buffer[j] == filter[i]) {
                filtered[filtered_count] = filter[i];
                filtered_count++;
                continue;
            }
        }
    }
    return filtered_count;
}

void destroy_proc(process *proc) {
    for(size_t i = 0; i < proc->reg_count; i++) {
        //free(proc->regions[i].pathname);
    }
    free(proc->regions);
    close(proc->mem_file_fd);
}
