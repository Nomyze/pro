#include <stdio.h>

int main(void) {
    int n = 200;

    int sum = 0;
    scanf("Input something");
    for(int i = 0; i < n; i++) {
        sum += i;
    }

    printf("Sum: %d, N: %d", sum, n);
    return 0;
}
