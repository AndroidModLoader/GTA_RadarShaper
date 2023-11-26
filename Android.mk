LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_MODULE    := RadarShapeR
LOCAL_SRC_FILES := main.cpp mod/logger.cpp mod/config.cpp
LOCAL_CFLAGS += -Ofast -mfloat-abi=softfp -DNDEBUG
LOCAL_LDLIBS += -llog
include $(BUILD_SHARED_LIBRARY)