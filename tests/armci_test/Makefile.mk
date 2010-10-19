#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

bin_PROGRAMS += test

test_SOURCES = $(top_srcdir)/tests/armci_test/test.c
test_LDADD = -larmci
test_DEPENDENCIES = libarmci.la
