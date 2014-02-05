AC_PREREQ([2.60])
AC_INIT([squashmerge], [0])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.6 foreign dist-bzip2 subdir-objects])

AC_PROG_CC

AC_USE_SYSTEM_EXTENSIONS

AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_ARG_ENABLE([lzo],
	AS_HELP_STRING([--disable-lzo], [Disable lzo support (default: autodetect)]))
AS_IF([test "x$enable_lzo" != "xno"], [
	AC_CHECK_HEADER([lzo/lzo1x.h], [
		AC_CHECK_LIB([lzo2], [lzo1x_decompress_safe], [
			AC_DEFINE([ENABLE_LZO], [1], [Define to enable LZO support])
			AC_SUBST([LZO_CFLAGS], [])
			AC_SUBST([LZO_LIBS], [-llzo2])
		])
	])
])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
