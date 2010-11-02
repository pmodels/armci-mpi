#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests/armci_test/test

tests_armci_test_test_SOURCES = $(top_srcdir)/tests/armci_test/test.c
tests_armci_test_test_LDADD = -larmci
tests_armci_test_test_DEPENDENCIES = libarmci.la
