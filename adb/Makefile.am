initddir = /etc/init.d
initd_SCRIPTS = start_adbd

bin_PROGRAMS = adbd

# adbd device daemon
# =========================================================
adbd_CC = @CC@

adbd_SOURCES = adb.c \
	fdevent.c \
	transport.c \
	transport_local.c \
	transport_usb.c \
	adb_auth_client.c\
	backup_service.c \
	sockets.c \
	services.c \
	file_sync_service.c \
	jdwp_service.c \
	framebuffer_service.c \
	remount_service.c \
	usb_linux_client.c \
	log_service.c \
	utils.c


adbd_CFLAGS = -O2 \
	-g \
	-DADB_HOST=0 \
	-Wall \
	-Wno-unused-parameter \
	-D_XOPEN_SOURCE \
	-D_GNU_SOURCE \
	-DANDROID_GADGET=1 \
	-DADB_REBOOT_ENABLED=1 \
	-D_FILE_OFFSET_BITS=64 \
	-I../include \
	-include ../include/arch/linux-arm/OEConfig.h \
        ${OPENSSL_CFLAGS} \
        ${GTHREAD_CFLAGS}

adbd_LDFLAGS = -lresolv
adbd_LDADD =  ../libcutils/libcutils.a
adbd_LDADD += ${OPENSSL_LIBS} ${GTHREAD_LIBS}
adbd_LDADD += -ldl
