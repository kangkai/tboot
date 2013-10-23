bin_PROGRAMS = tboot
tboot_SOURCES = \
	aboot.c \
	aboot.h \
	fastboot.c \
	fastboot.h \
	tboot_util.c \
	tboot_util.h \
	tboot.c \
	tboot.h \
	fstab.c \
	fstab.h \
	charging.c \
	charging.h \
	battery.c \
	battery.h \
	uevent.c \
	uevent.h \
	buffer.c \
	buffer.h \
	tboot_ui.c \
	tboot_ui.h \
	theme.h \
	debug.h \
	backlight_control.c \
	backlight_control.h \
	platform.c \
	platform.h \
	tboot_plugin.h

AM_CFLAGS = \
	-m32 \
	-O2 \
	-DNDEBUG \
	-DOTA_UPDATE_FILE=\"/cache/update.zip\" \
	-DDEVICE_NAME=\"medfield\" \
	-I$(top_srcdir)/kernel-headers \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)

AM_CFLAGS += $(DIRECTFB_CFLAGS)

tboot_LDADD = \
	$(top_builddir)/libminui/libminui.a \
	$(top_builddir)/ext4_utils/libext4_utils.a \
	$(top_builddir)/libdiskconfig/libdiskconfig.a \
	@PLUGINS@ \
	$(top_builddir)/libsparse/libsparse.a \
	$(top_builddir)/libcutils/libcutils.a \
	-lpthread \
	-lz

tboot_LDADD += $(DIRECTFB_LIBS)

EXTRA_DIST = \
	plugins.h
