LOCAL_PATH:=$(call my-dir)

#
# MAKE CAPTURE BINARY
# 
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=\
	main.cpp \
	capture/surface_getter.cpp \
	event/event_getter.cpp \
	event/event_player.cpp

LOCAL_MODULE:= dmt 
LOCAL_LDFLAGS:=-fPIE -pie 
#LOCAL_LDFLAGS+= -g # Deubg only
LOCAL_LDFLAGS+= -O2 -s -D_REENTRANT
APP_STL:=gnustl_static

LOCAL_LDLIBS+=-lz
LOCAL_C_INCLUDES+=/cygdrive/c/android-ndk-r10d/platforms/android-21/arch-arm/usr/include/

include $(BUILD_EXECUTABLE)
