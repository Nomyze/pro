#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int n = 200;

    int sum = 0;
    char *text;
    text = malloc(sizeof(char) * 20);
    char *heap;
    heap = malloc(sizeof(char) * 20);
    strcpy(heap, text);
    char str[20];
    fgets(str, 20, stdin);
    for(int i = 0; i < n; i++) {
        sum += i;
    }

    fgets(str, 20, stdin);
    printf("Sum: %d, N: %d, text: %s\n", sum, n, str);
    fgets(text, 20, stdin);
    printf("Ended up with: %s", str);
    free(heap);
    return 0;
}
