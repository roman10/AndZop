/*
this is a small test program tests which method to get the file size is faster
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define D1 50
#define D2 90
#define D3 90

unsigned char (*pData)[D1][D2][D3];
unsigned char data1[D1][D2][D3];
unsigned char data2[D1][D2][D3];

int main() {
    struct timeval stTime, edTime;
    int LOOP_COUNT = 100;
    int cnt, i, j, k;
    
    memset(data1, 0x01, D1*D2*D3);
    memset(data2, 0x00, D1*D2*D3);
    pData = &data1;
    //using memset 
    gettimeofday(&stTime, NULL);
    /*for (cnt = 0; cnt < LOOP_COUNT; ++cnt) {
        for (i = 0; i < D1; ++i) {
            for (j = 0; j < D2; ++j) {
	        memset(&(*pData)[i][j][0], 1, sizeof(unsigned char)*D3);
            }
        }
    }*/
    for (cnt = 0; cnt < LOOP_COUNT; ++cnt) {
        memset(&(*pData[0][0][0]), 1, sizeof(unsigned char)*D1*D2*D2);
    }
    gettimeofday(&edTime, NULL);
    printf("%u:%u\n", (unsigned int)(edTime.tv_sec - stTime.tv_sec), (unsigned int)(edTime.tv_usec - stTime.tv_usec));
    //using loop
    pData = &data2;
    gettimeofday(&stTime, NULL);
    for (cnt = 0; cnt < LOOP_COUNT; ++cnt) {
        for (i = 0; i < D1; ++i) {
            for (j = 0; j < D2; ++j) {
	        for (k = 0; k < D3; ++k) {
	            (*pData)[i][j][k] = 1;
	        }
            }
        }
    }
    gettimeofday(&edTime, NULL);
    printf("%u:%u\n", (unsigned int)(edTime.tv_sec - stTime.tv_sec), (unsigned int)(edTime.tv_usec - stTime.tv_usec));
    //(*pData)[_stFrame][_edH][_edW] = 88;    //uncomment see it fails the check below
    //to check two methods get same results
    for (i = 0; i < D1; ++i) {
        for (j = 0; j < D2; ++j) {
	    for (k = 0; k < D3; ++k) {
                if (data1[i][j][k] != data2[i][j][k]) {
                    printf("it doesn't work\n");
                    return;
                }
            }
        }
    }
}
