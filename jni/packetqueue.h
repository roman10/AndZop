#include <libavformat/avformat.h>

#include <pthread.h>

/**in order to support multi-thread, we implement a packet queue, which stores a GOP of video packet. 
So that when the dependency dump thread is done with a GOP. The selective decoding thread can start decoding
*/
typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int dep_gop_num;		//number of gop that its dependency has been dumped
    int decode_gop_num;		//number of gop that its frames have been decoded, if decode_gop_num < dep_gop_num, 
				//the selective decoding of next gop can start; otherwise, we'll need to wait
    int nb_packets;
    //pthread_mutex_t mutex;
    //pthread_cond_t cond;
} PacketQueue;

PacketQueue *gVideoPacketQueueList;

void packet_queue_init(PacketQueue *q);
void packet_queue_flush(PacketQueue *q);
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
int packet_queue_get(PacketQueue *q, AVPacket *pPkt);
