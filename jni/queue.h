#include <stdio.h>
#include <stdlib.h>

typedef struct MBIdx {
    int h;
    int w;
} MBIdx;

typedef struct QueueElement {
    struct MBIdx value;
    struct QueueElement *next;
} QueueElement;

typedef struct Queue {
    struct QueueElement* head;
    struct QueueElement* tail;
} Queue;

void initQueue(struct Queue *q);
void enqueue(struct Queue *q, struct MBIdx _value);
void dequeue(struct Queue *q);
struct MBIdx front(struct Queue *q);
int ifEmpty(struct Queue *q);
