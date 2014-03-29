bin_PROGRAMS = squashmerge

squashmerge_SOURCES = \
	src/compressor.c \
	src/compressor.h \
	src/util.c \
	src/util.h \
	src/squashmerge.c

squashmerge_CPPFLAGS = -pthread \
	$(LZO_CFLAGS) \
	$(LZ4_CFLAGS)
squashmerge_LDFLAGS = -pthread
squashmerge_LDADD = \
	$(LZO_LIBS) \
	$(LZ4_LIBS)

EXTRA_DIST = FORMAT NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
