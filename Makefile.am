BUILD_COMPONENTS :=

BUILD_COMPONENTS += liblog logcat libzipfile
if USELIBCUTILS
BUILD_COMPONENTS += libcutils
endif

BUILD_COMPONENTS += adb libmincrypt fastboot usb fs_mgr mkbootimg ueventd

SUBDIRS := $(BUILD_COMPONENTS)
