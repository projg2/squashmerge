AC_PREREQ([2.60])
AC_INIT([squashmerge], [0.1])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.6 foreign dist-bzip2 parallel-tests subdir-objects])

AC_PROG_CC

AC_USE_SYSTEM_EXTENSIONS

AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

AC_TYPE_SIZE_T

AC_ARG_ENABLE([lzo],
	AS_HELP_STRING([--disable-lzo], [Disable lzo support (default: autodetect)]))
found_lzo=no
AS_IF([test "x$enable_lzo" != "xno"], [
	AC_CHECK_HEADER([lzo/lzo1x.h], [
		AC_CHECK_LIB([lzo2], [lzo1x_decompress_safe], [
			found_lzo=yes
			AC_DEFINE([ENABLE_LZO], [1], [Define to enable LZO support])
			AC_SUBST([LZO_CFLAGS], [])
			AC_SUBST([LZO_LIBS], [-llzo2])
		])
	])
])
AM_CONDITIONAL([ENABLE_LZO], [test "$found_lzo" = "yes"])

AC_ARG_ENABLE([lz4],
	AS_HELP_STRING([--disable-lz4], [Disable lz4 support (default: autodetect)]))
found_lz4=no
AS_IF([test "x$enable_lz4" != "xno"], [
	AC_CHECK_HEADER([lz4.h], [
		AC_CHECK_LIB([lz4], [LZ4_decompress_safe], [
			found_lz4=yes
			AC_DEFINE([ENABLE_LZ4], [1], [Define to enable LZ4 support])
			AC_SUBST([LZ4_CFLAGS], [])
			AC_SUBST([LZ4_LIBS], [-llz4])
		])
	])
])
AM_CONDITIONAL([ENABLE_LZ4], [test "$found_lz4" = "yes"])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile tests/Makefile])
AC_OUTPUT
