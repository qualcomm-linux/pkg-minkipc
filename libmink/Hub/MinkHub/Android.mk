ifeq ($(TARGET_ENABLE_MINK_COMPONENT), true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libminkhub
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS := -Wall -Werror -Wno-unused-parameter

LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc \
                    $(LOCAL_PATH)/interface

LOCAL_HEADER_LIBRARIES := smcinvoke_headers minksocket_headers mink_headers credentialsHeaders
LOCAL_SHARED_LIBRARIES := liblog \
                          libminksocket_vendor \
                          libminkdescriptor \
                          libminkcredentials

LOCAL_SRC_FILES := src/ConnectionManager.c \
                   src/MinkHubUtils.c \
                   src/MinkHub.c

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/interface

include $(BUILD_STATIC_LIBRARY)
endif
