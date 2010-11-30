#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests/test_onesided         \
                  tests/test_mutex            \
                  tests/test_mutex_rmw        \
                  tests/test_mutex_trylock    \
                  tests/test_malloc           \
                  tests/test_malloc_irreg     \
                  tests/ARMCI_PutS_latency    \
                  tests/ARMCI_AccS_latency \
                  tests/test_groups           \
                  tests/test_group_split      \
                  tests/test_malloc_group     \
                  # end

tests_test_onesided_SOURCES = $(top_srcdir)/tests/test_onesided.c
tests_test_onesided_LDADD = -larmci
tests_test_onesided_DEPENDENCIES = libarmci.la

tests_test_mutex_SOURCES = $(top_srcdir)/tests/test_mutex.c
tests_test_mutex_LDADD = -larmci
tests_test_mutex_DEPENDENCIES = libarmci.la

tests_test_mutex_rmw_SOURCES = $(top_srcdir)/tests/test_mutex_rmw.c
tests_test_mutex_rmw_LDADD = -larmci
tests_test_mutex_rmw_DEPENDENCIES = libarmci.la

tests_test_mutex_trylock_SOURCES = $(top_srcdir)/tests/test_mutex_trylock.c
tests_test_mutex_trylock_LDADD = -larmci
tests_test_mutex_trylock_DEPENDENCIES = libarmci.la

tests_test_malloc_SOURCES = $(top_srcdir)/tests/test_malloc.c
tests_test_malloc_LDADD = -larmci
tests_test_malloc_DEPENDENCIES = libarmci.la

tests_test_malloc_irreg_SOURCES = $(top_srcdir)/tests/test_malloc_irreg.c
tests_test_malloc_irreg_LDADD = -larmci
tests_test_malloc_irreg_DEPENDENCIES = libarmci.la

tests_ARMCI_PutS_latency_SOURCES = $(top_srcdir)/tests/ARMCI_PutS_latency.c
tests_ARMCI_PutS_latency_LDADD = -larmci
tests_ARMCI_PutS_latency_DEPENDENCIES = libarmci.la

tests_ARMCI_AccS_latency_SOURCES = $(top_srcdir)/tests/ARMCI_AccS_latency.c
tests_ARMCI_AccS_latency_LDADD = -larmci
tests_ARMCI_AccS_latency_DEPENDENCIES = libarmci.la

tests_test_groups_SOURCES = $(top_srcdir)/tests/test_groups.c
tests_test_groups_LDADD = -larmci
tests_test_groups_DEPENDENCIES = libarmci.la

tests_test_group_split_SOURCES = $(top_srcdir)/tests/test_group_split.c
tests_test_group_split_LDADD = -larmci
tests_test_group_split_DEPENDENCIES = libarmci.la

tests_test_malloc_group_SOURCES = $(top_srcdir)/tests/test_malloc_group.c
tests_test_malloc_group_LDADD = -larmci
tests_test_malloc_group_DEPENDENCIES = libarmci.la

include tests/armci_test/Makefile.mk
include tests/cg/Makefile.mk
include tests/lu/Makefile.mk
include tests/transp1D/Makefile.mk
include tests/non-blocking/Makefile.mk
