bin_PROGRAMS = squashmerge

squashmerge_SOURCES = \
	src/compressor.c \
	src/squashmerge.c \
	src/util.c

squashmerge_CPPFLAGS = -pthread \
	$(LZO_CFLAGS)
squashmerge_LDFLAGS = -pthread
squashmerge_LDADD = \
	$(LZO_LIBS)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
