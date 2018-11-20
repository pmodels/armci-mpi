# Authors

* James Dinan  (MPI-2 implementation)
* Jeff Hammond (MPI-3 implementation)

# Introduction

This project provides a full, high performance, portable implementation of the
ARMCI runtime system using MPI's remote memory access (RMA) functionality.

# Quality Assurance

[![Build Status](https://travis-ci.org/pmodels/armci-mpi.svg?branch=master)](https://travis-ci.org/pmodels/armci-mpi)

See Travis for failure details.  All recent failures have been caused by dependencies (system toolchain or MPI library).

# Installing Only ARMCI-MPI

ARMCI-MPI uses autoconf and must be configured before compiling:
```
 $ ./configure
```
Many configure options are provided, run `configure --help` for details.  After
configuring the source tree, the code can be built and installed by running:
```
 $ make && make install
```

## MPI Library Issues

The quality of MPI-RMA implementations varies.   We recommend that you always use
the absolute latest release or release candidate version of MPI unless you are aware
of a specific issues that prevents this.

With the exception of the IBM Blue Gene platforms, all MPI libraries known to the
ARMCI-MPI developers support MPI-3 RMA, hence the MPI-2 RMA implementation has
been deprecated and is no longer supported.  If you are using ARMCI-MPI on a Blue Gene
system, use the `legacy` branch or contact the developers for assistance.

### MPI-3

As of September 2018, MPICH 3.3b3 and Open-MPI 3.1.2 are passing all of the tests in Travis CI.
Information about other implementations will be added here soon.

As of April 2014, the following implementations were known to work correctly
with ARMCI-MPI (MPI-3 version):
 * MPICH 3.0.4 and later on Mac, Linux SMPs and SGI SMPs.
 * MVAPICH2 2.0a and later on Linux InfiniBand clusters.
 * CrayMPI 6.1.0 and later on Cray XC30.
 * SGI MPT 2.09 on SGI SMPs.
 * Open-MPI development version on Mac (set `ARMCI_STRIDED_METHOD=IOV` and `ARMCI_IOV_METHOD=BATCHED`)
 
Note that a bug in MPICH 3.0 or 3.1 that propagated to MVAPICH2, Cray MPI and Intel MPI affects correctness
when windows are backed by shared-memory.  This bug affects `ARMCI_Rmw` and is avoided by setting
`ARMCI_USE_WIN_ALLOCATE=0` in your runtime environment.  This may negatively affect performance in some
cases and prevents one from using Casper, hence is not the default.

### MPI-2

As of August, 2011 the following MPI-2 implementations were known to work correctly
with ARMCI-MPI (MPI-2 version):
 * MPICH2 and MPICH 3+
 * MVAPICH2 1.6
 * Cray MPI on Cray XE6
 * IBM MPI on BG/P (set `ARMCI_STRIDED_METHOD=IOV` and `ARMCI_IOV_METHOD=BATCHED` for _performance reasons_)
 * Open-MPI 1.5.4 (set `ARMCI_STRIDED_METHOD=IOV` and `ARMCI_IOV_METHOD=BATCHED` for _correctness reasons_)

The following MPI-2 implementations are known to fail with ARMCI-MPI:
 * MVAPICH2 prior to 1.6

# Installing Global Arrays with ARMCI-MPI

To build GA (version 5.2 or later) with ARMCI-MPI (any version), use the configure option
`--with-armci=$(PATH_TO_ARMCI_MPI)` and make sure that you use the same MPI
implementation with GA that was used to compile ARMCI-MPI.

ARMCI-MPI (MPI-3) has been tested extensively with GA since version 5.2.

# Installing NWChem with ARMCI-MPI

If you are an NWChem user, you can use `${NWCHEM_TOP}/src/tools/install-armci-mpi`
without having to download or build ARMCI-MPI manually.

# The ARMCI-MPI Test Suite

ARMCI-MPI includes a set of testing and benchmark programs located under `tests/`
and `benchmarks/`.  These programs can be compiled and run via:
```
$ make check MPIEXEC="mpiexec -n 4"
```

The `MPIEXEC` variable is optional and is used to override the default MPI launch
command.  If you want only to build the test suite, the following target can be
used:
```
$ make checkprogs
```

# ARMCI-MPI Errata

## Direct access to local buffers

 * Because of MPI-2's semantics, you are not allowed to access shared memory
   directly, it must be through put/get.  Alternatively you can use the
   new ARMCI_Access_begin/end() functions.

 * MPI-3 allows direct access provided one uses a synchronization operation
   afterwards.  The ARMCI_Access_begin/end() functions are also valid.

## Progress semantics

 * On some MPI implementations and networks you may need to enable implicit
   progress.  In many cases this is done through an environment variable.  For
   MPICH2: set `MPICH_ASYNC_PROGRESS`; for MVAPICH2 recompile with
   `--enable-async-progress` and set `MPICH_ASYNC_PROGRESS`; set `DCMF_INTERRUPTS=1`
   for MPI on BGP; etc.

See [this page](https://github.com/jeffhammond/HPCInfo/tree/master/mpi#asynchronous-progress)
for more information on activating asynchronous progress in MPI.  However, we find
that most platforms show no improvement and often a decrease in performance,
provided the application makes calls to GA/ARMCI/MPI frequently enough on all
processes.

We recommend the user of [Casper](http://www.mcs.anl.gov/project/casper/) for
asynchronous progress in ARMCI-MPI.  See the Casper website for details.

# Environment Variables:

Boolean environment variables are enabled when set to a value beginning with
't', 'T', 'y', 'Y', or '1'; any other value is interpreted as false.

## Debugging Options

`ARMCI_VERBOSE` (boolean)

  Enable extra status output from ARMCI-MPI.

`ARMCI_DEBUG_ALLOC` (boolean)

  Turn on extra shared allocation debugging.

`ARMCI_FLUSH_BARRIERS` (boolean) (deprecated)

  Enable/disable extra communication flushing in ARMCI_Barrier.  Extra flushes
  are present to help make unsafe DLA safer.  (This option is deprecated with
  the ARMCI-MPI3 implementation.)

## Performance Options

`ARMCI_CACHE_RANK_TRANSLATION` (boolean)

  Create a table to more quickly translate between absolute and group ranks.

`ARMCI_PROGRESS_THREAD` (boolean)

  Create a Pthread to poke the MPI progress engine.

`ARMCI_PROGRESS_USLEEP` (int)

  Argument to `usleep()` to pause the progress polling loop.

## Noncollective Groups

`ARMCI_NONCOLLECTIVE_GROUPS` (boolean)

  Enable noncollective ARMCI group formation; group creation is collective on
  the output group rather than the parent group.

## Shared Buffer Protection

`ARMCI_SHR_BUF_METHOD` = { `COPY` (default), `NOGUARD` }

  ARMCI policy for managing shared origin buffers in communication operations:
  lock the buffer (unsafe, but fast), copy the buffer (safe), or don't guard
  the buffer - assume that the system is cache coherent and MPI supports
  unlocked load/store.

## Strided Options

`ARMCI_STRIDED_METHOD` = { `DIRECT` (default), `IOV` }

  Select the method for processing strided operations.

## I/O Vector Options

`ARMCI_IOV_METHOD` = { `AUTO` (default), `CONSRV`, `BATCHED`, `DIRECT` }

  Select the IO vector communication strategy: automatic; a "conservative"
  implementation that does lock/unlock around each operation; an implementation
  that issues batches of operations within a single lock/unlock epoch; and a
  direct implementation that generates datatypes for the origin and target and
  issues a single operation using them.

`ARMCI_IOV_CHECKS` (boolean)

  Enable (expensive) IOV safety/debugging checks (not recommended for
  performance runs).

`ARMCI_IOV_BATCHED_LIMIT` = { 0 (default), 1, ... }

  Set the maximum number of one-sided operations per epoch for the BATCHED IOV
  method.  Zero (default) is unlimited.
