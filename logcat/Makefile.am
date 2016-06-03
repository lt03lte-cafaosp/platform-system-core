bin_PROGRAMS = logcat

logcat_SOURCES = logcat.cpp event.logtags

logcat_CPPFLAGS  = -I$(top_srcdir)/include
logcat_CPPFLAGS += -I$(top_srcdir)/base/include
logcat_CPPFLAGS += -Wall -Wno-unused-parameter
logcat_CPPFLAGS += -D_XOPEN_SOURCE
logcat_CPPFLAGS += -DHAVE_IOCTL -DHAVE_SYS_UIO_H

logcat_LDADD   =  ../base/libbase.la
logcat_LDFALGS =  -llog -lcutils

