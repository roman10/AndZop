/*
this is a small test program tests which method to get the file size is faster
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

int main() {
    struct timeval stTime, edTime;
    char fileName[100];
    struct stat st;
    int LOOP_COUNT = 1;
    int i, size;
    FILE *f;
    sprintf(fileName, "%s", "./h11_1280_720_5m.mp4_mbstpos_gop2.txt");
    //using stat   
    stat(fileName, &st);
    printf("%d\n", st.st_size);
    gettimeofday(&stTime, NULL);
    for (i = 0; i < LOOP_COUNT; ++i) {
        stat(fileName, &st);
    }
    gettimeofday(&edTime, NULL);
    printf("%u:%u\n", (unsigned int)(edTime.tv_sec - stTime.tv_sec), (unsigned int)(edTime.tv_usec - stTime.tv_usec));
    //using fseek
    gettimeofday(&stTime, NULL);
    f = fopen(fileName, "r");
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    printf("%d\n", size);
    fclose(f);
    for (i = 0; i < LOOP_COUNT; ++i) {
        f = fopen(fileName, "r");
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
    }
    gettimeofday(&edTime, NULL);
    printf("%u:%u\n", (unsigned int)(edTime.tv_sec - stTime.tv_sec), (unsigned int)(edTime.tv_usec - stTime.tv_usec));
}
