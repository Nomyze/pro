#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct memory_region {
    void *start;
    void *end;
    char *pathname;
} memory_region;

typedef struct process {
    unsigned int pid;
    FILE *mem_file;
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

void open_memory_file(process *proc, char *modes) {
    char *mempath;
    size_t nneeded;
    nneeded = snprintf(NULL, 0, "/proc/%d/mem", proc->pid);
    mempath = (char *)malloc(nneeded + 1);
    snprintf(mempath, nneeded + 1, "/proc/%d/mem", proc->pid);

    FILE *mptr;
    mptr = fopen(mempath, modes);
    if(mptr == NULL) {
        fprintf(stderr, "Failed to open memory file\n");
        exit(EXIT_FAILURE);
    }
    proc->mem_file = mptr;
}

void close_memory_file(process *proc) {
    if(proc->mem_file != NULL) {
        fclose(proc->mem_file);
        proc->mem_file = NULL;
    } else {
        fprintf(stderr, "[PID: %d] Tried closing already closed memory file", proc->pid);
    }
}

int main(int argc, char *argv[]) {
    // Flags if any - later
    
    // 
    process proc = {};
    proc.pid = atoi(argv[1]);
    populate_regions(&proc);
    //printf("heap: %X-%X", test.heap_addrs[0], test.heap_addrs[1]);
    memory_region reg = proc.regions[proc.reg_count - 1];
    printf("%llx-%llx, pathname: %s\n", reg.start, reg.end, reg.pathname);
    return 0;
}
