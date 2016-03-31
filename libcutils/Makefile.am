androidincludedir = $(includedir)/android

androidinclude_HEADERS = $(abs_top_srcdir)/include/android/log.h

cutilsincludedir = $(includedir)/cutils

cutilsinclude_HEADERS = $(abs_top_srcdir)/include/cutils/abort_socket.h \
	$(abs_top_srcdir)/include/cutils/android_reboot.h \
	$(abs_top_srcdir)/include/cutils/array.h \
	$(abs_top_srcdir)/include/cutils/ashmem.h \
	$(abs_top_srcdir)/include/cutils/atomic-arm.h \
	$(abs_top_srcdir)/include/cutils/atomic.h \
	$(abs_top_srcdir)/include/cutils/atomic-inline.h \
	$(abs_top_srcdir)/include/cutils/atomic-x86.h \
	$(abs_top_srcdir)/include/cutils/compiler.h \
	$(abs_top_srcdir)/include/cutils/config_utils.h \
	$(abs_top_srcdir)/include/cutils/cpu_info.h \
	$(abs_top_srcdir)/include/cutils/dir_hash.h \
	$(abs_top_srcdir)/include/cutils/event_tag_map.h \
	$(abs_top_srcdir)/include/cutils/hashmap.h \
	$(abs_top_srcdir)/include/cutils/iosched_policy.h \
	$(abs_top_srcdir)/include/cutils/jstring.h \
	$(abs_top_srcdir)/include/cutils/klog.h \
	$(abs_top_srcdir)/include/cutils/logd.h \
	$(abs_top_srcdir)/include/cutils/logger.h \
	$(abs_top_srcdir)/include/cutils/log.h \
	$(abs_top_srcdir)/include/cutils/logprint.h \
	$(abs_top_srcdir)/include/cutils/memory.h \
	$(abs_top_srcdir)/include/cutils/misc.h \
	$(abs_top_srcdir)/include/cutils/mq.h \
	$(abs_top_srcdir)/include/cutils/mspace.h \
	$(abs_top_srcdir)/include/cutils/native_handle.h \
	$(abs_top_srcdir)/include/cutils/open_memstream.h \
	$(abs_top_srcdir)/include/cutils/partition_utils.h \
	$(abs_top_srcdir)/include/cutils/process_name.h \
	$(abs_top_srcdir)/include/cutils/properties.h \
	$(abs_top_srcdir)/include/cutils/record_stream.h \
	$(abs_top_srcdir)/include/cutils/sched_policy.h \
	$(abs_top_srcdir)/include/cutils/selector.h \
	$(abs_top_srcdir)/include/cutils/sockets.h \
	$(abs_top_srcdir)/include/cutils/threads.h \
	$(abs_top_srcdir)/include/cutils/tztime.h \
	$(abs_top_srcdir)/include/cutils/uio.h \
	$(abs_top_srcdir)/include/cutils/zygote.h

# NOTE: Only supports builds of shared/static libraries for the target

common_sources := array.c
common_sources += android_reboot.c
common_sources += hashmap.c
common_sources += atomic.c
common_sources += native_handle.c
common_sources += buffer.c
common_sources += klog.c
common_sources += socket_inaddr_any_server.c
common_sources += socket_local_client.c
common_sources += socket_local_server.c
common_sources += socket_loopback_client.c
common_sources += socket_loopback_server.c
common_sources += socket_network_client.c
common_sources += config_utils.c
common_sources += cpu_info.c
common_sources += load_file.c
common_sources += strdup16to8.c
common_sources += strdup8to16.c
common_sources += record_stream.c
common_sources += partition_utils.c
common_sources += process_name.c
common_sources += properties.c
common_sources += threads.c
# common_sources += sched_policy.c
# common_sources += iosched_policy.c

# TODO: When building against Mingw, these files should NOT be built
common_sources += abort_socket.c
common_sources += mspace.c
common_sources += selector.c
common_sources += tztime.c
# common_sources += adb_networking.c
common_sources += zygote.c

# Target libcutils
lib_LIBRARIES := libcutils.a

libcutils_a_CPPFLAGS := -I../include
libcutils_a_CPPFLAGS += -I../liblog
libcutils_a_CPPFLAGS += -DHAVE_PTHREADS
libcutils_a_CPPFLAGS += -DLINUX_ENABLED
libcutils_a_CPPFLAGS += -DANDROID_SMP=0
libcutils_a_CPPFLAGS += -include ../include/arch/linux-arm/OEConfig.h

# libcutils_a_LDFLAGS := -version-info 1:0:0
# libcutils_a_LIBADD  := -lpthread
requiredlibs = -lpthread

libcutils_a_SOURCES := $(common_sources)
libcutils_a_SOURCES += ashmem-dev.c
libcutils_a_SOURCES += mq.c
libcutils_a_SOURCES += memory.c

if ARM
libcutils_a_CPPFLAGS += -DHAVE_ASM_MEMSET
libcutils_a_SOURCES += memset32.S
endif # !ARM


# Build a DSO too
lib_LTLIBRARIES := libcutils.la
libcutils_la_SOURCES := $(libcutils_a_SOURCES)
libcutils_la_CPPFLAGS := $(libcutils_a_CPPFLAGS)
libcutils_la_LIBADD  := ../liblog/liblog.la

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := libcutils.pc
EXTRA_DIST := $(pkgconfig_DATA)
