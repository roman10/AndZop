#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    int **a;
    int i, j;
    a = (int **)malloc(sizeof(int *)*10);
    for (i = 0; i < 10; ++i) {
        a[i] = (int*)malloc(sizeof(int)*10);
    }
    //memset to all 0s
    for (i = 0; i < 10; ++i) {
        memset(a[i], 0, sizeof(a[i][0])*10);
    }
    //print out the value
    for (i = 0; i < 10; ++i) {
        for (j = 0; j < 10; ++j) {
            printf("%d ", a[i][j]);
        }
        printf("\n");
    }
    return 0;
}

