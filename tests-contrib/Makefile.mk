#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += \
                  tests-contrib/armci-perf         \
                  # end

TESTS          += \
                  tests-contrib/armci-perf         \
                  # end

tests_contrib_armci_perf_SOURCES = $(top_srcdir)/tests-contrib/armci-perf.c
tests_contrib_armci_perf_LDADD = -larmci -lm
tests_contrib_armci_perf_DEPENDENCIES = libarmci.la

include tests-contrib/armci_test/Makefile.mk
include tests-contrib/cg/Makefile.mk
include tests-contrib/lu/Makefile.mk
include tests-contrib/transp1D/Makefile.mk
