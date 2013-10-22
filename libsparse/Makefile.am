noinst_LIBRARIES = libsparse.a
libsparse_a_SOURCES = \
	backed_block.c \
	backed_block.h \
	output_file.c \
	output_file.h \
	sparse.c \
	sparse_crc32.c \
	sparse_crc32.h \
	sparse_defs.h \
	sparse_err.c \
	sparse_file.h \
	sparse_format.h \
	sparse_read.c

AM_CFLAGS = \
	-m32 \
	-O2 \
	-I$(top_srcdir)/libsparse/include

EXTRA_DIST = \
	include
