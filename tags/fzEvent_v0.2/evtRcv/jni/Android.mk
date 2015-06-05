LOCAL_PATH := $(call my-dir)

#Library
include $(CLEAR_VARS)
LOCAL_MODULE := libACE
LOCAL_SRC_FILES := /home/skwak/ACE_wrappers/lib/libACE.so
include $(PREBUILT_SHARED_LIBRARY)

#Executable
include $(CLEAR_VARS)

LOCAL_MODULE := er
LOCAL_SRC_FILES := main.cpp \
                   CEvtRcv.cpp
LOCAL_LDFLAGS:=-fPIE -pie
LOCAL_SHARED_LIBRARIES := libACE
LOCAL_LDLIBS += -lz
LOCAL_C_INCLUDES += /home/skwak/ACE_wrappers \
                    /home/skwak/fzEvent/evtRcv/jni
LOCAL_CFLAGS := -DACE_NTRACE=1

include $(BUILD_EXECUTABLE)
