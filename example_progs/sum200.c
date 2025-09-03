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
    char str2[20] = "myinput";
    char* str;
    str = (char*)malloc(sizeof(char) * 20);
    fgets(str2, 20, stdin);
    for(int i = 0; i < n; i++) {
        sum += i;
    }

    fgets(str, 20, stdin);
    printf("Sum: %d, N: %d, text1: %s, text2: %s\n", sum, n, str, str2);
    fgets(text, 20, stdin);
    printf("Ended up with: %s", str);
    free(heap);
    free(str);
    return 0;
}
