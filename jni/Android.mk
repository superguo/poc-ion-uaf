LOCAL_PATH := $(call my-dir)

# ==================================================
# payload_user_thumb
include $(CLEAR_VARS)
LOCAL_SRC_FILES := poc-ion.c
LOCAL_MODULE := poc-ion
include $(BUILD_EXECUTABLE)
