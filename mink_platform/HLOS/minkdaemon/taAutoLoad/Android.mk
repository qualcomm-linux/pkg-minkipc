ifeq ($(ENABLE_TAAUTO_LOAD), true)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(DMA_BUF2_ENABLE),true)
LOCAL_CFLAGS += -DDMA_BUF2_ENABLE
endif

LOCAL_HEADER_LIBRARIES := qtvm_sdk_headers mink_headers smcinvoke_headers securemsm_GPTEE_inc libjsoncpp_headers minksocket_headers

LOCAL_SRC_FILES :=  TaAutoLoad.cpp \
                    TaImageReader.cpp \
                    DmaMemPool.cpp \
                    CRequestTABuffer.cpp

LOCAL_SHARED_LIBRARIES := liblog \
                          libminkdescriptor \
                          libjsoncpp \
                          libdmabufheap

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../inc

ifeq ($(TARGET_USES_ION),true)
include $(LIBION_HEADER_PATH_WRAPPER)
LOCAL_C_INCLUDES += $(LIBION_HEADER_PATHS)
LOCAL_SHARED_LIBRARIES += libion
endif

LOCAL_MODULE                := libtaautoload
LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)
endif
