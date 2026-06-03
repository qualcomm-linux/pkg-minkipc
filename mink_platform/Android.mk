LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

MINK_PLATFORM_PATH := $(LOCAL_PATH)
include $(MINK_PLATFORM_PATH)/HLOS/minkdaemon/Android.mk
include $(MINK_PLATFORM_PATH)/mink_test/Android.mk
