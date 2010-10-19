#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

bin_PROGRAMS += test_onesided test_mutex test_mutex_rmw test_mutex_trylock test_malloc \
	test_malloc_irreg ARMCI_PutS_latency ARMCI_PutAccS_latency test_groups test_group_split \
	test_malloc_group

test_onesided_SOURCES = $(top_srcdir)/tests/test_onesided.c
test_onesided_LDADD = -larmci
test_onesided_DEPENDENCIES = libarmci.la

test_mutex_SOURCES = $(top_srcdir)/tests/test_mutex.c
test_mutex_LDADD = -larmci
test_mutex_DEPENDENCIES = libarmci.la

test_mutex_rmw_SOURCES = $(top_srcdir)/tests/test_mutex_rmw.c
test_mutex_rmw_LDADD = -larmci
test_mutex_rmw_DEPENDENCIES = libarmci.la

test_mutex_trylock_SOURCES = $(top_srcdir)/tests/test_mutex_trylock.c
test_mutex_trylock_LDADD = -larmci
test_mutex_trylock_DEPENDENCIES = libarmci.la

test_malloc_SOURCES = $(top_srcdir)/tests/test_malloc.c
test_malloc_LDADD = -larmci
test_malloc_DEPENDENCIES = libarmci.la

test_malloc_irreg_SOURCES = $(top_srcdir)/tests/test_malloc_irreg.c
test_malloc_irreg_LDADD = -larmci
test_malloc_irreg_DEPENDENCIES = libarmci.la

ARMCI_PutS_latency_SOURCES = $(top_srcdir)/tests/ARMCI_PutS_latency.c
ARMCI_PutS_latency_LDADD = -larmci
ARMCI_PutS_latency_DEPENDENCIES = libarmci.la

ARMCI_PutAccS_latency_SOURCES = $(top_srcdir)/tests/ARMCI_PutAccS_latency.c
ARMCI_PutAccS_latency_LDADD = -larmci
ARMCI_PutAccS_latency_DEPENDENCIES = libarmci.la

test_groups_SOURCES = $(top_srcdir)/tests/test_groups.c
test_groups_LDADD = -larmci
test_groups_DEPENDENCIES = libarmci.la

test_group_split_SOURCES = $(top_srcdir)/tests/test_group_split.c
test_group_split_LDADD = -larmci
test_group_split_DEPENDENCIES = libarmci.la

test_malloc_group_SOURCES = $(top_srcdir)/tests/test_malloc_group.c
test_malloc_group_LDADD = -larmci
test_malloc_group_DEPENDENCIES = libarmci.la

include tests/armci_test/Makefile.mk
include tests/cg/Makefile.mk
include tests/lu/Makefile.mk
include tests/transp1D/Makefile.mk
include tests/non-blocking/Makefile.mk
