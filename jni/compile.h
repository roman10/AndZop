#ifndef COMPILE_H
#define COMPILE_H

#define ANDROID_BUILD
#define LOG_LEVEL 1

//#define SELECTIVE_DECODING			//commented: run as normal decoding mode;  uncommented: run as selective decoding mode

#define DECODE_VIDEO_THREAD		//commented: disable decoding, only dump the dependencies with BG_DUMP_THREAD ON
//[TODO]: the two flags below may not be fully compatible now??? dump and preload may conflict
//#define BG_DUMP_THREAD			//commented: no background thread running to dump or check
//#define PRE_LOAD_DEP				//uncommented: enable a separate thread to pre-load the dependency files

//#define NORM_DECODE_DEBUG			//uncommented: dump dependency for normal decoding mode; should be commented at 						
                                    //selective decoding mode
//#define DUMP_SELECTED_MB_MASK			//enabled: dump the mask for the mb needed;
//#define DUMP_VIDEO_FRAME_BYTES			//enabled: dump the bytes to a binary file
//#define DUMP_SELECTIVE_DEP			//enabled: dump the relationship in memory to files

//#define COMPOSE_PACKET_OR_SKIP          //enabled: compose packet; disabled: skip
//#define MV_BASED_DEPENDENCY               //enabled: MV-based dependency; disabled: mb-based dependency
//#define MV_BASED_OPTIMIZATION             //enabled: optimized method to compute MV-based inter-frame dependency

//#define INTRA_DEP_OPTIMIZATION

#define DUMP_PACKET

//#define CLEAR_DEP_BEFORE_START

#ifdef ANDROID_BUILD
	#define MAX_FRAME_NUM_IN_GOP 50
	#define MAX_MB_H 90
	#define MAX_MB_W 90
	#define MAX_INTER_DEP_MB 4
	#define MAX_INTRA_DEP_MB 3
#else
	#define MAX_FRAME_NUM_IN_GOP 50
	#define MAX_MB_H 90
	#define MAX_MB_W 90
	#define MAX_INTER_DEP_MB 4
	#define MAX_INTRA_DEP_MB 3
#endif

#endif
