ifeq ($(TARGET_ENABLE_MINK_COMPONENT), true)
ifeq ($(ENABLE_HLOSMINK_DAEMON), true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

MINKDAEMON_PATH := $(LOCAL_PATH)

LOCAL_CFLAGS        := $(COMMON_CFLAGS) -Wall -Werror -Wno-ignored-qualifiers -Wno-unused-parameter -Wno-deprecated-declarations -Wno-missing-braces

LOCAL_MODULE        := hlosminkdaemon
LOCAL_INIT_RC       := hlosminkdaemon.rc
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_C_INCLUDES := $(MINKDAEMON_PATH)/inc \
                    $(MINKDAEMON_PATH)/../../../libmink/Hub/MinkHub/inc

LOCAL_HEADER_LIBRARIES := smcinvoke_headers minksocket_headers mink_headers credentialsHeaders qtvm_sdk_headers

LOCAL_SRC_FILES :=  src/MinkDaemon.cpp \
                    src/TEnv.cpp \
                    src/CMinkdRegister.cpp \
                    src/TaLoader.cpp \
                    src/CRegister.cpp \
                    src/CredentialsManager.cpp \
                    src/EnvWrapper.cpp

LOCAL_SHARED_LIBRARIES += liblog \
                          libutils \
                          libcutils \
                          libcrypto \
                          libminksocket_vendor \
                          libminkdescriptor \
                          libqcbor \
                          libqrtr \
                          libdl \
                          libjsoncpp

LOCAL_STATIC_LIBRARIES := libminkcredentials libminkhub

#ENABLE_HLOSMINKD_VENDOR_CLIENTS will load below vendor clients
#rtice, qwes, tzsc and ConnectionSecurity
ifeq ($(ENABLE_HLOSMINKD_VENDOR_CLIENTS),true)

LOCAL_CFLAGS += -DHLOSMINKD_CLIENTS

LOCAL_SRC_FILES += src/MinkdVendorClients.cpp
LOCAL_STATIC_LIBRARIES +=  libqwesd \
                           libsecure_channel

LOCAL_SHARED_LIBRARIES += libqmi_common_so \
                          libqmi_csi \
                          libqmi_cci

endif #ENABLE_HLOSMINKD_VENDOR_CLIENTS

ifeq ($(ENABLE_HLOSMINKD_MODEM_SERVICE),true)
LOCAL_CFLAGS += -DHLOSMINKD_MODEM_SERVICE
LOCAL_SRC_FILES += src/MinkModemOpener.cpp
endif #ENABLE_HLOSMINKD_MODEM_SERVICE

ifeq ($(ENABLE_UNIX_TZD_SERVER),true)
LOCAL_CFLAGS += -DHLOSMINKD_UNIX_TZD_SERVER
endif #ENABLE_UNIX_TZD_SERVER

ifeq ($(ENABLE_TAAUTO_LOAD), true)
LOCAL_CFLAGS += -DHLOSMINKD_TAAUTO_LOAD
endif #ENABLE_TAAUTO_LOAD

include $(BUILD_EXECUTABLE)
include $(LOCAL_PATH)/taAutoLoad/Android.mk
endif
endif
