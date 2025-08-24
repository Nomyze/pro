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
    DIR *d_proc;
    d_proc = opendir("/proc/");
    if(d_proc == NULL) {
        fprintf(stderr, "Failed to open /proc directory\n");
        exit(EXIT_FAILURE);
    }
    struct dirent* entry;
    while ((entry = readdir(d_proc)) != NULL) {
        if(!is_number(entry->d_name)) {
            continue;
        }
        char *comm_path;
        size_t needed;
        needed = snprintf(NULL, 0, "/proc/%s/comm", entry->d_name);
        comm_path = (char *)malloc(needed + 1);
        snprintf(comm_path, needed + 1, "/proc/%s/comm", entry->d_name);
        FILE *comm_file;
        comm_file = fopen(comm_path, "r");
        if(comm_file == NULL) {
            fprintf(stderr, "Failed to open proc %s comm file\n", entry->d_name);
        } 
        else {
            char *line;
            size_t n = 0;
            ssize_t read = getline(&line, &n, comm_file);
            line[read - 1] = '\0';
            if(strcmp(str, line) == 0) {
                printf("Got: %s\n", entry->d_name);
                proc->pid = atoi(entry->d_name);                
                return;
            }
        }
    }
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
        if(!strcmp(reg.pathname, "[stack]") || !strcmp(reg.pathname, "[heap]")) {
            node *cur = malloc(sizeof(node));
            cur->reg = reg;
            cur->next = head;
            head = cur;
            reg_count++;
        }
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
    void *pos = memmem(haystack + *offset, length - *offset, buf, n);
    *offset += (pos == NULL ? length : pos - haystack + n);
    return pos - haystack;
}

off_t *find_buffern(process *proc, void *buf, size_t n) {
    if(proc->mem_file_fd == -1) {
        fprintf(stderr, "[PID: %d] No open memory file", proc->pid);
    }
    off_t *out_buffer;
    out_buffer = malloc(sizeof(void**));
    int count = 0;
    int size = 0;
    for(int i = 0; i < proc->reg_count; i++) {
        size_t length = proc->regions[i].end - proc->regions[i].start + 1;
        //printf("Length: %ld, ", length);
        //void *reg = mmap(NULL, length, PROT_READ, MAP_SHARED, proc->mem_file_fd, (off_t)proc->regions[i].start);
        off_t offset = (off_t) 0;
        lseek(proc->mem_file_fd, proc->regions[i].start - (void *)0, SEEK_SET);
        void *buffer;
        buffer = malloc(length);
        ssize_t nbytes = read(proc->mem_file_fd, buffer, length);
        while (offset < nbytes) {
            //printf("Offset: %ld\n", offset);
            off_t pos = -1;
            pos = find_first_buffern(buffer, length, buf, n, &offset);

            if(pos >= -1) {
                pos += proc->regions[i].start - (void *)0;
                //return pos;
                if(count >= size) {
                    off_t *temp_buffer = malloc((size + 1) * sizeof(void *));
                    memcpy(temp_buffer, out_buffer, size * sizeof(void *));
                    free(out_buffer);
                    out_buffer = temp_buffer;
                    size++;
                }
                printf("Found position at: %lx\n", pos);
                out_buffer[count] = pos;
                count++;
            }
            offset += nbytes;
        }
        for(int i = 0; i < length; i++) {
            //printf("%02X", ((char *)buffer)[i]);
            if((i + 1) % 16 == 0) {
                //printf("\n");
            }
        }
        free(buffer);
        //printf("\nat %llx-%llx\t%s\n\n", proc->regions[i].start, proc->regions[i].end, proc->regions[i].pathname);
    }
    out_buffer[count] = -1;
    return out_buffer;
}

void printn_at(int fd, int n, off_t offset) {
    char buffer[n+1];

    lseek(fd, offset, SEEK_SET);
    int ret = read(fd, buffer, n);
    if(ret == -1) {
        fprintf(stderr, "Failed to print at: %lx, errno: %d", offset, errno);
    }
    buffer[n] = '\0';
    
    printf("Printing at offset: %lx, found: %s\n", offset, buffer);
    for(int i = 0; i < strlen(buffer); i++) {
        printf("%d ", buffer[i]);
    }
    printf("\n");
}

void writen_to(int fd, void *buffer, int n, off_t offset) {
    lseek(fd, offset, SEEK_SET);
    write(fd, buffer, n);
}

int filter_addrs(off_t *buffer, int n, off_t *filter, int fn, off_t *filtered) {
    int filtered_count = 0;
    for (int i = 0; i < fn; i++) {
        off_t *_off;
        if (0 >= find_first_buffern(buffer, sizeof(off_t) * n, filter + i, sizeof(off_t), _off)) {
            filtered[filtered_count] = filter[i];
            filtered_count += 1;
        }
    }
    filtered[filtered_count] = -1;
    return filtered_count;
}
