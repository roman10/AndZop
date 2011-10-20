#include "packetqueue.h"

/**
the structure for AVPakcetList
	struct AVPacketList {
	    AVPacket pkt;
	    struct AVPacketList* next;
	}
**/

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    //pthread_mutex_init(&q->mutex, NULL);
    //pthread_cond_init(&q->cond, NULL);
}

void packet_queue_flush(PacketQueue *q) {
    AVPacketList *lPktList, *lPktList1;
    //pthread_mutex_lock(&q->mutex);
    for (lPktList = q->first_pkt; lPktList != NULL; lPktList = lPktList1) {
	lPktList1 = lPktList->next;
	av_free_packet(&lPktList->pkt);
	av_freep(&lPktList);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    //pthread_mutex_unlock(&q->mutex);
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;
    /*duplicate the packet*/
    //printf("packet_queue_put\n");
    if (av_dup_packet(pkt) < 0)
	return -1;
    pkt1 = av_malloc(sizeof(AVPacketList));
    if (!pkt1)
	return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    /*add the list token to the queue*/
    //pthread_mutex_lock(&q->mutex);
    if (!q->last_pkt) {
	q->first_pkt = pkt1;	     //if the queue is empty
    } else {
	q->last_pkt->next = pkt1;    //if the queue is not empty
    }
    q->last_pkt = pkt1;
    ++q->nb_packets;
    //pthread_mutex_unlock(&q->mutex);
    //printf("packet_queue_put done\n");
}

int packet_queue_get(PacketQueue *q, AVPacket *pPkt) {
    AVPacketList *lPktList;
    int lRet;
    printf("packet_queue_get\n");
    //pthread_mutex_lock(&q->mutex);
    for (;;) {
	lPktList = q->first_pkt;
	if (q->dep_gop_num > q->decode_gop_num && lPktList) {
	    q->first_pkt = lPktList->next;
	    if (!q->first_pkt) {
		q->last_pkt = NULL;
	    }
	    --q->nb_packets;
	    *pPkt = lPktList->pkt;
	    av_free(lPktList);
	    lRet = 1;
	    break;
	} else {
	    lRet = 0;
	    break;
	} 
    }
    //pthread_mutex_unlock(&q->mutex);
    printf("packet_queue_get done\n");
    return lRet;
}
