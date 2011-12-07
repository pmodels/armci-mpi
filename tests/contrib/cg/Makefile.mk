#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests/contrib/cg/cg

tests_contrib_cg_cg_SOURCES = $(top_srcdir)/tests/contrib/cg/cg.c $(top_srcdir)/tests/contrib/cg/compute.c \
	$(top_srcdir)/tests/contrib/cg/read_input.c $(top_srcdir)/tests/contrib/cg/cg_timing.c
tests_contrib_cg_cg_LDADD = libarmci.la -lm
