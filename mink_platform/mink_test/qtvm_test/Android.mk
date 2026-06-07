LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

VENDOR_QC_PROP := vendor/qcom/proprietary
SECUREMSM_SHIP_PATH := $(TOP)/$(VENDOR_QC_PROP)/securemsm
SECUREMSM_INTERNAL_PATH := $(TOP)/$(VENDOR_QC_PROP)/securemsm-internal
MINKIPC_PATH := $(LOCAL_PATH)/../../..
QTVM_TEST_IDL_SRC_DIR := $(LOCAL_PATH)/idl
TARGET_OUTPUT_DIR := $(TARGET_OUT_INTERMEDIATES)/include

# get idl filename list in qtvm_test
QTVM_TEST_IDL_SRC := $(wildcard $(QTVM_TEST_IDL_SRC_DIR)/*.idl)
QTVM_TEST_IDL_INVOKE_SRC := $(wildcard $(QTVM_TEST_IDL_SRC_DIR)/I*.idl)

# Define INTERNAL targets
QTVM_TEST_INT_IDL          := $(QTVM_TEST_IDL_SRC)
QTVM_TEST_INT_IDL_H        := $(patsubst $(QTVM_TEST_IDL_SRC_DIR)/%.idl, $(TARGET_OUTPUT_DIR)/%.h, $(QTVM_TEST_INT_IDL))
QTVM_TEST_INT_IDL_HPP      := $(patsubst $(QTVM_TEST_IDL_SRC_DIR)/%.idl, $(TARGET_OUTPUT_DIR)/%.hpp, $(QTVM_TEST_INT_IDL))
QTVM_TEST_INT_INVOKE_IDL   := $(QTVM_TEST_IDL_INVOKE_SRC)
QTVM_TEST_INT_IDL_INVK_H   := $(patsubst $(QTVM_TEST_IDL_SRC_DIR)/%.idl, $(TARGET_OUTPUT_DIR)/%_invoke.h, $(QTVM_TEST_INT_INVOKE_IDL))
QTVM_TEST_INT_IDL_INVK_HPP := $(patsubst $(QTVM_TEST_IDL_SRC_DIR)/%.idl, $(TARGET_OUTPUT_DIR)/%_invoke.hpp, $(QTVM_TEST_INT_INVOKE_IDL))

# Define INTERNAL recipes
$(TARGET_OUTPUT_DIR):
	@mkdir -p $@
$(QTVM_TEST_INT_IDL_H): $(TARGET_OUTPUT_DIR)/%.h : $(QTVM_TEST_IDL_SRC_DIR)/%.idl
	$(MINKIPC_PATH)/libmink/SDK/QTVM/minkidl -o $@ $<
$(QTVM_TEST_INT_IDL_HPP): $(TARGET_OUTPUT_DIR)/%.hpp : $(QTVM_TEST_IDL_SRC_DIR)/%.idl
	$(MINKIPC_PATH)/libmink/SDK/QTVM/minkidl -o $@ $< --cpp
$(QTVM_TEST_INT_IDL_INVK_H): $(TARGET_OUTPUT_DIR)/%_invoke.h : $(QTVM_TEST_IDL_SRC_DIR)/%.idl
	$(MINKIPC_PATH)/libmink/SDK/QTVM/minkidl -o $@ $< --skel
$(QTVM_TEST_INT_IDL_INVK_HPP): $(TARGET_OUTPUT_DIR)/%_invoke.hpp : $(QTVM_TEST_IDL_SRC_DIR)/%.idl
	$(MINKIPC_PATH)/libmink/SDK/QTVM/minkidl -o $@ $< --skel --cpp

LOCAL_MODULE := qtvm_test_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(TARGET_OUT_INTERMEDIATES)/include
LOCAL_GENERATED_SOURCES := $(TARGET_OUTPUT_DIR) \
	$(QTVM_TEST_INT_IDL_H) \
	$(QTVM_TEST_INT_IDL_HPP) \
	$(QTVM_TEST_INT_IDL_INVK_H) \
	$(QTVM_TEST_INT_IDL_INVK_HPP)

include $(BUILD_HEADER_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := src/TvmdTest.cpp \
                   src/TVMMonitor.cpp \
                   src/CAVMTestService.cpp

LOCAL_C_INCLUDES := $(SECUREMSM_SHIP_PATH)/smcinvoke/inc \
                    $(SECUREMSM_SHIP_PATH)/smcinvoke/TZCom/inc \
                    $(SECUREMSM_SHIP_PATH)/smcinvoke/mink/inc/uid \
                    $(MINKIPC_PATH)/mink_platform/HLOS/minkdaemon/inc \
                    $(MINKIPC_PATH)/libmink/Hub/MinkHub/interface \
                    $(MINKIPC_PATH)/libmink/SDK/QTVM/inc \
                    $(MINKIPC_PATH)/mink_platform/QTVM/daemons/mink/inc \
                    $(MINKIPC_PATH)/mink_platform/mink_test/qtvm_test/inc

LOCAL_HEADER_LIBRARIES := mink_headers minksocket_headers qtvm_sdk_headers qtvm_test_headers credentialsHeaders libbinder_ndk_headers libAVFVirtClientInterface_headers
LOCAL_SHARED_LIBRARIES := libcutils \
                          libutils \
                          liblog \
                          libdmabufheap \
                          libvmmem \
                          libminksocket_vendor \
                          libssl \
                          libcrypto \
                          libminkdescriptor \
                          libfdwrapper_vendor \
                          libqcbor \
                          libbinder_ndk \
                          libAVFVirtClientInterface

ifeq ($(ENABLE_VIRT_CLIENT_INTERFACE), true)
    LOCAL_SHARED_LIBRARIES += vendor.qti.AvfQcvmManager-V1-ndk
endif

LOCAL_STATIC_LIBRARIES := libgtest \
                          libgtest_main \
                          libminkcredentials

LOCAL_CFLAGS := -O3
ifeq ($(ENABLE_UNIX_TZD_SERVER),true)
LOCAL_CFLAGS += -DHLOSMINKD_UNIX_TZD_SERVER
endif #ENABLE_UNIX_TZD_SERVER
LOCAL_MODULE := MinkPlatformTest
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti

include $(BUILD_EXECUTABLE)
