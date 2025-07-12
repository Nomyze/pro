#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

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

void populate_regions(process *proc) {
    char *mapspath;
    size_t nneeded;
    nneeded = snprintf(NULL, 0, "/proc/%d/maps", proc->pid);
    mapspath = (char *)malloc(nneeded + 1);
    snprintf(mapspath, nneeded + 1, "/proc/%d/maps", proc->pid);

    FILE *mptr;
    mptr = fopen(mapspath, "r");
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
        node *cur = malloc(sizeof(node));
        cur->reg = reg;
        cur->next = head;
        head = cur;
        reg_count++;
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

void *find_first_buffern(void *haystack, size_t length, void *buf, size_t n, off_t *offset) {
    void *pos = memmem(haystack + *offset, length - *offset, buf, n);
    *offset += (pos == NULL ? length : pos - haystack + n);
    return pos;
}

void **find_buffern(process *proc, void *buf, size_t n) {
    if(proc->mem_file_fd == -1) {
        fprintf(stderr, "[PID: %d] No open memory file", proc->pid);
    }
    void **out_buffer;
    out_buffer = malloc(sizeof(void**));
    int count = 0;
    int size = 0;
    for(int i = 0; i < proc->reg_count; i++) {
        size_t length = proc->regions[i].end - proc->regions[i].start + 1;
        printf("Length: %ld, ", length);
        //void *reg = mmap(NULL, length, PROT_READ, MAP_SHARED, proc->mem_file_fd, (off_t)proc->regions[i].start);
        off_t offset = (off_t) 0;
        lseek(proc->mem_file_fd, (off_t)proc->regions[i].start, SEEK_SET);
        void *buffer;
        buffer = malloc(length);
        ssize_t nbytes = read(proc->mem_file_fd, buffer, length);
        while (offset < nbytes) {
            printf("Offset: %d\n", offset);
            void *pos = NULL;
            pos = find_first_buffern(buffer, length, buf, n, &offset);

            if(pos != NULL) {
                //return pos;
                if(count >= size) {
                    void **temp_buffer = malloc((size + 1) * sizeof(void *));
                    memcpy(temp_buffer, out_buffer, size * sizeof(void *));
                    free(out_buffer);
                    out_buffer = temp_buffer;
                    size++;
                }
                printf("Found position at: %llx", pos);
                out_buffer[count] = pos;
                count++;
            }
            offset += nbytes;
        }
        for(int i = 0; i < length; i++) {
            printf("%02X", ((char *)buffer)[i]);
            if((i + 1) % 16 == 0) {
                printf("\n");
            }
        }
        free(buffer);
        printf("\nat %llx-%llx\t%s\n\n", proc->regions[i].start, proc->regions[i].end, proc->regions[i].pathname);
    }
    out_buffer[count] = NULL;
    return out_buffer;
}


int main(int argc, char *argv[]) {
    // Flags if any - later
    
    // 
    process proc = {};
    proc.pid = atoi(argv[1]);
    populate_regions(&proc);
    memory_region reg = proc.regions[proc.reg_count - 1];
    printf("%llx-%llx, pathname: %s\n", reg.start, reg.end, reg.pathname);
    open_memory_file(&proc, O_RDONLY);

    char *text = "myinput";
    void **addrs = find_buffern(&proc, text, strlen(text));
    printf("Searched for: %s\nAs ASCII:\n", text);
    for(int i = 0; i < sizeof(int); i++) {
        printf("%d ", text[i]);
    }
    printf("\n");
    if(addrs[0] == NULL) {
        printf("Failed to find pattern\n");
    } else {
        printf("Found pattern at addrs:\n");
        int i = 0;
        while(addrs[i++] != NULL) {
            printf("Addr: %llx\n", addrs[i-1]);
        }
    }
    return 0;
}
