#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests-contrib/non-blocking/overlap tests-contrib/non-blocking/simple

TESTS          += tests-contrib/non-blocking/simple

tests_contrib_non_blocking_overlap_SOURCES = $(top_srcdir)/tests-contrib/non-blocking/overlap.c
tests_contrib_non_blocking_overlap_LDADD = -larmci -lm
tests_contrib_non_blocking_overlap_DEPENDENCIES = libarmci.la

tests_contrib_non_blocking_simple_SOURCES = $(top_srcdir)/tests-contrib/non-blocking/simple.c
tests_contrib_non_blocking_simple_LDADD = -larmci -lm
tests_contrib_non_blocking_simple_DEPENDENCIES = libarmci.la
