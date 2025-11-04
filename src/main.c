#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "mem/proc.h"

void sum200o(process *proc) {
    char *text = "myinput";
    off_t *addrs = NULL;
    size_t size = 0;
    find_buffern(proc, text, strlen(text), &addrs, &size);
    printf("Searched for: %s\nAs ASCII:\n", text);
    for(size_t i = 0; i < strlen(text); i++) {
        printf("%d ", text[i]);
    }
    printf("\n");
    if(size == 0) {
        printf("Failed to find pattern\n");
    } else {
        printf("Found pattern at addrs:\n");
        for(size_t i = 0; i < size; i++) {
            printf("Addr: %lx\n", addrs[i]);
            printn_at(proc, strlen(text), addrs[i]);
        }
        char *text2="ourinput";
        size_t filtered_count = 0;
        off_t filtered[size];
        while(filtered_count == 0) {
            off_t *addrs2 = NULL;
            size_t size2 = 0;
            find_buffern(proc, text2, strlen(text2), &addrs2, &size2);
            if(size2 > 0) {
                for(size_t j = 0; j < size2; j++) {
                    printf("Filter Addr: %lx\n", addrs2[j]);
                    printn_at(proc, strlen(text2), addrs2[j]);
                }
                filtered_count = filter_addrs(addrs, size, addrs2, size2, filtered);
            }
            free(addrs2);
        }
        free(addrs);

        char *newtext = "cokolwiek";
        for(size_t i = 0; i < filtered_count; i++) {
            printf("Filtered Addr: %lx\n", filtered[i]);
            printf("Writing to found addr(%lx) with data: %s\n", filtered[i], newtext);
            writen_to(proc, newtext, strlen(newtext) + 1, filtered[i]);
            printn_at(proc, strlen(newtext), filtered[i]);
        }
    }
}

void inter(process *proc) {
    int n = 200;
    off_t *addrs = NULL;
    size_t size = 0;
    find_int32_t(proc, n, &addrs, &size);
    printf("Searched for: %d", n);
    if(size == 0) {
        printf("Failed to find pattern\n");
    } else {
        printf("Found pattern at addrs:\n");
        for(size_t i = 0; i < size; i++) {
            printf("Addr: %lx\n", addrs[i]);
            printn_at(proc, sizeof(int32_t), addrs[i]);
        }
        if(size == 1) {
            int n2 = 100;
            printf("Writing to found addr(%lx) with data: %d\n", addrs[0], n2);
            write_int32_t_to(proc, n2, addrs[0]);
            printn_at(proc, sizeof(int32_t), addrs[0]);
        }
    }
}

int main(int argc, char *argv[]) {
    // Flags if any - later
    if(argc < 2) {
        return 0;
    } 
    process proc = {};
    bind_proc_by_name(&proc, argv[1]);
    populate_regions(&proc);
    //memory_region reg = proc.regions[proc.reg_count - 1];
    //printf("%llx-%llx, pathname: %s\n", (long long unsigned int)reg.start, (long long unsigned int)reg.end, reg.pathname);
    open_memory_file(&proc, O_RDWR);
    
    if(strcmp(argv[1], "sum200.o") == 0) {
        sum200o(&proc);
    } else if(strcmp(argv[1], "inter.o") == 0) {
        inter(&proc);
    }
    destroy_proc(&proc);
    return 0;
}


