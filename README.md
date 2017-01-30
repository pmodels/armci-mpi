# Authors

* James Dinan  (MPI-2 implementation)
* Jeff Hammond (MPI-3 implementation)

# Introduction

This project provides a full, high performance, portable implementation of the
ARMCI runtime system using MPI's remote memory access (RMA) functionality.

# Quality Assurance

[![Build Status](https://travis-ci.org/jeffhammond/armci-mpi.svg?branch=mpi3rma)](https://travis-ci.org/jeffhammond/armci-mpi)

See Travis for failure details.  All recent failures have been caused by dependencies (system toolchain or MPI library).

# Installing Only ARMCI-MPI

ARMCI-MPI uses autoconf and must be configured before compiling:
```
 $ ./configure
```
Many configure options are provided, run `configure --help` for details.  After
configuring the source tree, the code can be built and installed by running:

 $ make && make install

The quality of MPI-RMA implementations varies.  As of August, 2011 the
following MPI-2 implementations are known to work correctly with ARMCI-MPI:
 * MPICH2 and MPICH 3+
 * MVAPICH2 1.6
 * Cray MPI on Cray XE6
 * IBM MPI on BG/P (set `ARMCI_STRIDED_METHOD=IOV` and `ARMCI_IOV_METHOD=BATCHED` for _performance reasons_)
 * Open-MPI 1.5.4 (set `ARMCI_STRIDED_METHOD=IOV` and `ARMCI_IOV_METHOD=BATCHED` for _correctness reasons_)

The following MPI-2 implementations are known to fail with ARMCI-MPI:
 * MVAPICH2 prior to 1.6

As of April 2014, the following MPI-3 implementations are know to work correctly
with ARMCI-MPI (`mpi3rma` branch):
 * MPICH 3.0.4 and later on Mac, Linux SMPs and SGI SMPs.
 * MVAPICH2 2.0a and later on Linux InfiniBand clusters.
 * CrayMPI 6.1.0 and later on Cray XC30.
 * SGI MPT 2.09 on SGI SMPs.
 * Open-MPI development version on Mac (set `ARMCI_STRIDED_METHOD=IOV` and `ARMCI_IOV_METHOD=BATCHED`)
 
Note that a bug in MPICH 3.0 or 3.1 that propagated to MVAPICH2, Cray MPI and Intel MPI affects correctness
when windows are backed by shared-memory.  This bug affects `ARMCI_Rmw` and is avoided with the
default settings, which use `MPI_Win_create`.  This may negatively affect performance in some
cases and prevents one from using Casper.  To utilize `MPI_Win_allocate`, do one of the following:
* Build with the `--enable-win-allocate` option to `configure`.
* Set `ARMCI_USE_WIN_ALLOCATE=1` in your runtime environment.

# Installing Global Arrays with ARMCI-MPI

ARMCI-MPI2 has been tested with GA 5.0.2.  To build GA with ARMCI-MPI2, rename
this directory to "armci" and substitute it for the "armci" directory in the GA
distribution.  Configure and build GA as usual; no special flags are required.

To build GA 5.2 with ARMCI-MPI (any version), use the configure option
`--with-armci=$(PATH_TO_ARMCI_MPI)` and make sure that you use the same MPI
implementation with GA that was used to compile ARMCI-MPI.

ARMCI-MPI3 has been tested extensively with GA 5.2 and 5.3.

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

Direct access to local buffers:

 * Because of MPI-2's semantics, you are not allowed to access shared memory
   directly, it must be through put/get.  Alternatively you can use the
   new ARMCI_Access_begin/end() functions.

 * MPI-3 allows direct access provided one uses a synchronization operation
   afterwards.  The ARMCI_Access_begin/end() functions are also valid.

Progress semantics:

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

# Environment Variables:

Boolean environment variables are enabled when set to a value beginning with
't', 'T', 'y', 'Y', or '1'; any other value is interpreted as false.

##Debugging Options

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

# Status

Right now, we are seeing failures caused by OpenMPI bugs, not because ARMCI-MPI itself is broken.

* `master`: [![Build Status](https://travis-ci.org/jeffhammond/armci-mpi.svg?branch=master)](https://travis-ci.org/jeffhammond/armci-mpi)
* `mpi2rma`: [![Build Status](https://travis-ci.org/jeffhammond/armci-mpi.svg?branch=mpi2rma)](https://travis-ci.org/jeffhammond/armci-mpi)
* `mpi2rma-plus-rmw`: [![Build Status](https://travis-ci.org/jeffhammond/armci-mpi.svg?branch=mpi2rma-plus-rmw)](https://travis-ci.org/jeffhammond/armci-mpi)
* `mpi3rma`: [![Build Status](https://travis-ci.org/jeffhammond/armci-mpi.svg?branch=mpi3rma)](https://travis-ci.org/jeffhammond/armci-mpi)

The `master` branch is still only for MPI-2 RMA.
The `mpi2rma` branch is essentially the same, except it is ABI-compatible with the MPI-3 branch.
The `mpi2rma-plus-rmw` uses mostly MPI-2 features except for MPI-3 atomics.  Because it uses
MPI-2 RMA synchronization, it is _conservative_ and thus may work with an otherwise buggy 
implementation of MPI-3 RMA.  It exists primarily for historical purposes.
The `mpi3rma` branch is what one should use on the majority of platforms, provided MPI-3 RMA is working.
