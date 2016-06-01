common_sources := logd_write_syslog.c
common_sources += log_event_write.c
common_sources += event_tag_map.c

host_sources   := $(common_sources) fake_log_device.c event.logtags
target_sources := $(common_sources) log_time.cpp
target_sources += log_is_loggable.c
target_sources += logprint.c
target_sources += log_read_kern.c

requiredlibs = -lpthread

# Build liblog.so
lib_LTLIBRARIES    := liblog.la

liblog_la_CPPFLAGS := -DHAVE_PTHREADS
liblog_la_CPPFLAGS += -DHAVE_SYS_UIO_H
liblog_la_CPPFLAGS += -DLINUX_ENABLED
liblog_la_CPPFLAGS += -D__linux__
liblog_la_CPPFLAGS += -DLIBLOG_LOG_TAG=1005

if BUILD_DEVICE
liblog_la_SOURCES  := $(target_sources)
liblog_la_CPPFLAGS += -DANDROID_SMP=0
endif

if BUILD_HOST
liblog_la_SOURCES  := $(host_sources)
liblog_la_CPPFLAGS += -DFAKE_LOG_DEVICE=1
endif

# Export headers
log_includedir=$(includedir)/log
log_include_HEADERS := @core_incdir@/log/event_tag_map.h
log_include_HEADERS += @core_incdir@/log/logd.h
log_include_HEADERS += @core_incdir@/log/logger.h
log_include_HEADERS += @core_incdir@/log/log.h
log_include_HEADERS += @core_incdir@/log/logprint.h
log_include_HEADERS += @core_incdir@/log/log_read.h
log_include_HEADERS += @core_incdir@/log/uio.h

alog_includedir=$(includedir)/android
alog_include_HEADERS = @core_incdir@/android/log.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = liblog.pc
EXTRA_DIST = $(pkgconfig_DATA)
