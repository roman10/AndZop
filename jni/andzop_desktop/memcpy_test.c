/*
this is a small test program tests which method to get the file size is faster
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FRAME_NUM_IN_GOP 50
#define MAX_MB_H 90
#define MAX_MB_W 90

int interDepMask[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];
int nextInterDepMask[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];

int main() {
    struct timeval stTime, edTime;
    int LOOP_COUNT = 1;
    pthread_cond_t preloadCondVar;
    pthread_cond_init(&preloadCondVar, NULL);
    int i, j, k;
    //using memcpy   
    gettimeofday(&stTime, NULL);
    for (i = 0; i < LOOP_COUNT; ++i) {
        memcpy(interDepMask, nextInterDepMask, sizeof(int)*MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
	//pthread_cond_signal(&preloadCondVar);
    }
    gettimeofday(&edTime, NULL);
    printf("%u:%u\n", (unsigned int)(edTime.tv_sec - stTime.tv_sec), (unsigned int)(edTime.tv_usec - stTime.tv_usec));
    //using memset   
    gettimeofday(&stTime, NULL);
    for (i = 0; i < MAX_FRAME_NUM_IN_GOP; ++i) {
        for (j = 0; j < MAX_MB_H; ++j) {
            memset(interDepMask[i][j], 0, sizeof(interDepMask[0][0][0])*MAX_MB_W);
        }
    }
    gettimeofday(&edTime, NULL);
    printf("%u:%u\n", (unsigned int)(edTime.tv_sec - stTime.tv_sec), (unsigned int)(edTime.tv_usec - stTime.tv_usec));
    gettimeofday(&stTime, NULL);
    for (i = 0; i < MAX_FRAME_NUM_IN_GOP; ++i) {
        for (j = 0; j < MAX_MB_H; ++j) {
            for (k = 0; k < MAX_MB_W; ++k) {
                interDepMask[i][j][k] = 0;
            }
        }
    }
    gettimeofday(&edTime, NULL);
    printf("%u:%u\n", (unsigned int)(edTime.tv_sec - stTime.tv_sec), (unsigned int)(edTime.tv_usec - stTime.tv_usec));
}
