# Copyright 2005 The Android Open Source Project
# Copyright (c) 2009, Code Aurora Forum. All rights reserved.

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	builtins.c \
	init.c \
	devices.c \
	property_service.c \
	util.c \
	parser.c \
	logo.c

ifeq ($(strip $(INIT_BOOTCHART)),true)
LOCAL_SRC_FILES += bootchart.c
LOCAL_CFLAGS    += -DBOOTCHART=1
endif

ifneq (, $(filter qsd8250_surf qsd8250_ffa, $(TARGET_PRODUCT)))
  LOCAL_CFLAGS += -DSURF8K
endif

LOCAL_MODULE:= init

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_UNSTRIPPED)

LOCAL_STATIC_LIBRARIES := libcutils libc

#LOCAL_STATIC_LIBRARIES := libcutils libc libminui libpixelflinger_static
#LOCAL_STATIC_LIBRARIES += libminzip libunz libamend libmtdutils libmincrypt
#LOCAL_STATIC_LIBRARIES += libstdc++_static

include $(BUILD_EXECUTABLE)

