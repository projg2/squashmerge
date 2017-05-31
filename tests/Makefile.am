LOG_COMPILER = "$(srcdir)/perform-test.sh"
AM_LOG_FLAGS = "$(top_builddir)/squashmerge" "$(builddir)"

TESTS =
EXTRA_DIST = perform-test.sh
CLEANFILES =

if ENABLE_LZO
TESTS += \
	01-sqfs-lzo1.sqdelta \
	01-sqfs-lzo9.sqdelta
endif

if ENABLE_LZ4
TESTS += \
	01-sqfs-lz4.sqdelta \
	01-sqfs-lz4hc.sqdelta
endif

EXTRA_DIST += \
	01-sqfs-lzo1.in \
	01-sqfs-lzo1.out \
	01-sqfs-lzo1.sqdelta \
	01-sqfs-lzo9.in \
	01-sqfs-lzo9.out \
	01-sqfs-lzo9.sqdelta \
	01-sqfs-lz4.in \
	01-sqfs-lz4.out \
	01-sqfs-lz4.sqdelta \
	01-sqfs-lz4hc.in \
	01-sqfs-lz4hc.out \
	01-sqfs-lz4hc.sqdelta

CLEANFILES += \
	01-sqfs-lzo1.testout \
	01-sqfs-lzo9.testout \
	01-sqfs-lz4.testout \
	01-sqfs-lz4hc.testout
