LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
	LOCAL_MODULE := RadarShapeR
else
	LOCAL_MODULE := RadarShapeR64
endif
LOCAL_SRC_FILES := main.cpp mod/logger.cpp mod/config.cpp
LOCAL_CFLAGS += -Ofast -mfloat-abi=softfp -DNDEBUG
LOCAL_LDLIBS += -llog
include $(BUILD_SHARED_LIBRARY)