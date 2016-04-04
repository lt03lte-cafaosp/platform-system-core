bin_PROGRAMS = ueventd

# ueventd tool for linux
# =========================================================
ueventd_SOURCES := \
	devices.c \
	ueventd.c \
	util.c

AM_CFLAGS = -O2 \
	-g \
	-Wall \
	-Wno-unused-parameter \
	-D_XOPEN_SOURCE \
	-D_GNU_SOURCE \
	-I../include \
	-include ../include/arch/linux-arm/OEConfig.h

ueventd_LDFLAGS = -lpthread

ueventd_LDADD = -lz
