AM_CFLAGS = -Wundef \
         -MD \
         -Wno-trigraphs \
         -Wno-multichar\
                 -Werror \
         -g -O0 \
         -fno-inline \
         -fno-short-enums \
         -fpic \
                 -Wno-multichar \
                -Wno-conversion

libsync_la_SOURCES = sync.c
libsync_la_CFLAGS  = -I$(top_srcdir)/include
libsync_la_CFLAGS  += -I$(top_srcdir)/libsync/include

# Export headers
sync_includedir=$(includedir)/sync
sync_include_HEADERS := $(top_srcdir)/libsync/include/sync/sync.h

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := libsync.pc
EXTRA_DIST := $(pkgconfig_DATA)
# Build shared lib.
lib_LTLIBRARIES = libsync.la
