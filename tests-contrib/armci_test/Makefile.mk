#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests-contrib/armci_test/test

TESTS          += tests-contrib/armci_test/test

tests_contrib_armci_test_test_SOURCES = $(top_srcdir)/tests-contrib/armci_test/test.c
tests_contrib_armci_test_test_LDADD = -larmci
tests_contrib_armci_test_test_DEPENDENCIES = libarmci.la
