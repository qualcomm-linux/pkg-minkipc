LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

VENDOR_QC_PROP := vendor/qcom/proprietary
SECUREMSM_SHIP_PATH := $(TOP)/$(VENDOR_QC_PROP)/securemsm

LOCAL_SRC_FILES := src/main.c \
                   src/profiling.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc \
                    $(SECUREMSM_SHIP_PATH)/smcinvoke/inc

LOCAL_HEADER_LIBRARIES := mink_headers minksocket_headers qtvm_sdk_headers credentialsHeaders
LOCAL_SHARED_LIBRARIES := libcutils \
                          libutils \
                          liblog \
                          libdmabufheap \
                          libminksocket_vendor \
                          libfdwrapper_vendor \
                          libminkdescriptor

LOCAL_STATIC_LIBRARIES := libminkcredentials

LOCAL_CFLAGS := -O3
LOCAL_CFLAGS += -DDEBUG
LOCAL_MODULE := QTVMSampleCA
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti

include $(BUILD_EXECUTABLE)
