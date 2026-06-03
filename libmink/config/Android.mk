LOCAL_PATH:= $(call my-dir)

############## config file ##########################
ifeq ($(filter $(TARGET_BOARD_PLATFORM),sun canoe hamoa art),$(TARGET_BOARD_PLATFORM))
include $(CLEAR_VARS)
LOCAL_MODULE := ta_config.json
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ta_config.json
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)/ssg
include $(BUILD_PREBUILT)
endif
