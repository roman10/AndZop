LOCAL_PATH := $(call my-dir)

#declare the sqlite library
#include $(CLEAR_VARS)
#LOCAL_MODULE := sqlite-prebuilt
#LOCAL_SRC_FILES := sqlite/android/armv7-a/libsqlite_andzop.so
#LOCAL_EXPORT_C_INCLUDES := sqlite/android/armv7-a/include
#LOCAL_EXPORT_LDLIBS := sqlite/android/armv7-a/libsqlite_andzop.so
#LOCAL_PRELINK_MODULE := true
#include $(PREBUILT_SHARED_LIBRARY)

#declare the prebuilt library
include $(CLEAR_VARS)
LOCAL_MODULE := ffmpeg-prebuilt
LOCAL_SRC_FILES := ffmpeg/android/armv7-a/libffmpeg.so
LOCAL_EXPORT_C_INCLUDES := ffmpeg/android/armv7-a/include
LOCAL_EXPORT_LDLIBS := ffmpeg/android/armv7-a/libffmpeg.so
LOCAL_PRELINK_MODULE := true
include $(PREBUILT_SHARED_LIBRARY)

#the andzop library
include $(CLEAR_VARS)
LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := andzop
LOCAL_SRC_FILES := cpu_id.c scale.c yuv2rgb.neon.S utility.c queue.c packetqueue.c dependency.c andzop.c
#LOCAL_SRC_FILES := scale.c yuv2rgb16tab.c yuv420rgb8888c.c yuv420rgb8888.S utility.c queue.c packetqueue.c dependency.c andzop.c
#LOCAL_SRC_FILES := yuv2rgb.neon.S utility.c queue.c packetqueue.c dependency.c andzop.c
#LOCAL_ARM_NEON := true 
#ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	#if the cpu supports neon, we use neon
#LOCAL_CFLAGS := -DHAVE_NEON=1
LOCAL_ARM_NEON := true 
#else
	#otherwise, use the arm assembly to scale and do color conversion
#	LOCAL_SRC_FILES += scale.c yuv2rgb16tab.c yuv420rgb8888.S 
#endif
LOCAL_C_INCLUDES := $(LOCAL_PATH)/ffmpeg/android/armv7-a/include
LOCAL_SHARED_LIBRARY := ffmpeg-prebuilt
LOCAL_STATIC_LIBRARIES := cpufeatures
LOCAL_LDLIBS    := -llog -ljnigraphics -lz -lm $(LOCAL_PATH)/ffmpeg/android/armv7-a/libffmpeg.so
include $(BUILD_SHARED_LIBRARY)
$(call import-module,cpufeatures)
