/**
a C queue implementation using linked list
*/
#include "queue.h"
/*initialize the queue*/
void initQueue(struct Queue *q) {
    q->head = NULL;
    q->tail = NULL;
}
/*insert an element to the end of the queue*/
void enqueue(struct Queue *q, struct MBIdx _value) {
    //allocate a new QeueuElement for _value
    QueueElement *newElement;
    newElement = (QueueElement*) malloc(sizeof(QueueElement));
    newElement->value = _value;
    newElement->next = NULL;
    if (q->head == NULL) {
	//first element
        q->head = newElement;
        q->tail = newElement;
    } else {
	//put it to the tail
	q->tail->next = newElement;
	q->tail = newElement;
    }
}
/*delete the first element from the queue*/
void dequeue(struct Queue *q) {
    QueueElement *element;
    if (q->head == NULL) {
	//empty queue
	return;
    } else {
	element = q->head;
	q->head = q->head->next;
	free(element);
    }
}
/*get the front value of the queue, but don't delete it*/
struct MBIdx front(struct Queue *q) {
    return q->head->value;
}
/*check if the queue is empty*/
int ifEmpty(struct Queue *q) {
    return (q->head == NULL ? 1:0);
}
