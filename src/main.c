#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "mem/proc.h"

int main(int argc, char *argv[]) {
    // Flags if any - later
    if(argc < 2) {
        return 0;
    } 
    process proc = {};
    bind_proc_by_name(&proc, argv[1]);
    populate_regions(&proc);
    memory_region reg = proc.regions[proc.reg_count - 1];
    printf("%llx-%llx, pathname: %s\n", (long long unsigned int)reg.start, (long long unsigned int)reg.end, reg.pathname);
    open_memory_file(&proc, O_RDWR);

    char *text = "myinput2";
    off_t *addrs = find_buffern(&proc, text, strlen(text));
    printf("Searched for: %s\nAs ASCII:\n", text);
    for(size_t i = 0; i < strlen(text); i++) {
        printf("%d ", text[i]);
    }
    printf("\n");
    if(addrs[0] == -1) {
        printf("Failed to find pattern\n");
    } else {
        printf("Found pattern at addrs:\n");
        int i = 0;
        while(addrs[i++] != -1) {
            printf("Addr: %lx\n", addrs[i-1]);
            printn_at(proc.mem_file_fd, strlen(text), addrs[i-1]);
        }
        char *text2="ourinput";
        int filtered_count = 0;
        int size = i;
        off_t filtered[size];
        while(filtered_count == 0) {
            off_t *addrs2 = find_buffern(&proc, text2, strlen(text2));
            if(addrs2[0] != -1) {
                int j = 0;
                while(addrs2[j++] != -1) {
                    printf("Filter Addr: %lx\n", addrs2[j-1]);
                    printn_at(proc.mem_file_fd, strlen(text2), addrs2[j-1]);
                }
                filtered_count = filter_addrs(addrs, size, addrs2, j-1, filtered);
            }
        }

        char *newtext = "cokolwiek";
        for(i = 0; i < filtered_count; i++) {
            printf("Filtered Addr: %lx\n", filtered[i]);
            printf("Writing to found addr(%lx) with data: %s\n", filtered[i], newtext);
            writen_to(proc.mem_file_fd, newtext, strlen(newtext) + 1, filtered[i]);
            printn_at(proc.mem_file_fd, strlen(newtext), filtered[i]);
        }
        /*
        printf("i == %d\n", i);
        if(i == 1) {
            printf("Writing to found addr(%lx) with data: %s\n", filtered[0], newtext);
            writen_to(proc.mem_file_fd, newtext, strlen(newtext) + 1, filtered[0]);
            printn_at(proc.mem_file_fd, strlen(newtext), filtered[0]);
        }
        */
    }
    return 0;
}
