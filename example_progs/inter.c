#include <stdio.h>
#include <unistd.h>

int main() {
    int n = 200;
    usleep(20000);
    for(int i = 0; i < n; i++) {
        printf("i: %d\n", i);
        //usleep(2000);
    }
    return 0;
}
