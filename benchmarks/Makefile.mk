#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += benchmarks/ping-pong          \
                  benchmarks/ping-pong-mpi      \
                  benchmarks/ring-flood         \
                  benchmarks/armci-perf         \
                  benchmarks/contiguous-bench     \
                  benchmarks/strided-bench     \
                  # end

benchmarks_ping_pong_SOURCES = $(top_srcdir)/benchmarks/ping-pong.c
benchmarks_ping_pong_LDADD = -larmci
benchmarks_ping_pong_DEPENDENCIES = libarmci.la

benchmarks_ping_pong_mpi_SOURCES = $(top_srcdir)/benchmarks/ping-pong-mpi.c
benchmarks_ping_pong_mpi_LDADD = -larmci
benchmarks_ping_pong_mpi_DEPENDENCIES = libarmci.la

benchmarks_ring_flood_SOURCES = $(top_srcdir)/benchmarks/ring-flood.c
benchmarks_ring_flood_LDADD = -larmci
benchmarks_ring_flood_DEPENDENCIES = libarmci.la

benchmarks_armci_perf_SOURCES = $(top_srcdir)/benchmarks/armci-perf.c
benchmarks_armci_perf_LDADD = -larmci -lm
benchmarks_armci_perf_DEPENDENCIES = libarmci.la

benchmarks_contiguous_bench_SOURCES = $(top_srcdir)/benchmarks/contiguous-bench.c
benchmarks_contiguous_bench_LDADD = -larmci -lm
benchmarks_contiguous_bench_DEPENDENCIES = libarmci.la

benchmarks_strided_bench_SOURCES = $(top_srcdir)/benchmarks/strided-bench.c
benchmarks_strided_bench_LDADD = -larmci -lm
benchmarks_strided_bench_DEPENDENCIES = libarmci.la
