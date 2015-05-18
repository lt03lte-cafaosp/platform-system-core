LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
ifeq ($(call is-vendor-board-platform,QCOM),true)
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
endif

LOCAL_SRC_FILES := sdcard.c
LOCAL_MODULE := sdcard
LOCAL_CFLAGS := -Wall -Wno-unused-parameter -Werror

LOCAL_SHARED_LIBRARIES := libc libcutils

APPOPS_SDCARD_PROTECT := enable

ifdef APPOPS_SDCARD_PROTECT
LOCAL_CFLAGS += -DAPPOPS_SDCARD_PROTECT
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../../external/sqlite/dist

LOCAL_SHARED_LIBRARIES += libsqlite
endif

include $(BUILD_EXECUTABLE)
