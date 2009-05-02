# Copyright 2006 The Android Open Source Project
# Copyright (c) 2009, Code Aurora Forum. All rights reserved.

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../../../vendor/qcom-proprietary/diag/include \

LOCAL_SRC_FILES:= logcat.cpp

LOCAL_SHARED_LIBRARIES := liblog

ifeq ($(strip $(BOARD_USES_QCOM_HARDWARE)), true)
LOCAL_SHARED_LIBRARIES += libdiag
LOCAL_CFLAGS += -DUSE_DIAG
endif

LOCAL_MODULE:= logcat

include $(BUILD_EXECUTABLE)

########################
include $(CLEAR_VARS)

LOCAL_MODULE := event-log-tags

# This will install the file in /system/etc
#
LOCAL_MODULE_CLASS := ETC

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)
