#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int n = 200;

    int sum = 0;
    char *text = "Sometext";
    char *heap;
    heap = malloc(sizeof(char) * 20);
    strcpy(heap, text);
    char str[20];
    fgets(str, 20, stdin);
    if(n == 100) {
        text[0] = 'O';
    }
    for(int i = 0; i < n; i++) {
        sum += i;

    }

    printf("Sum: %d, N: %d, text: %s\n", sum, n, text);
    scanf("asdf %d", &n);
    free(heap);
    return 0;
}
