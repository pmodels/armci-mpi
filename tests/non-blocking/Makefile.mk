#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests/non-blocking/overlap tests/non-blocking/simple

TESTS          += tests/non-blocking/simple

tests_non_blocking_overlap_SOURCES = $(top_srcdir)/tests/non-blocking/overlap.c
tests_non_blocking_overlap_LDADD = -larmci -lm
tests_non_blocking_overlap_DEPENDENCIES = libarmci.la

tests_non_blocking_simple_SOURCES = $(top_srcdir)/tests/non-blocking/simple.c
tests_non_blocking_simple_LDADD = -larmci -lm
tests_non_blocking_simple_DEPENDENCIES = libarmci.la
