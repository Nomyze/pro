#include <stdio.h>
#include <unistd.h>

int main() {
    int n = 200;

    for(int i = 0; i < n; i++) {
        printf("i: %d\n", i);
        usleep(10000);
    }
    return 0;
}
