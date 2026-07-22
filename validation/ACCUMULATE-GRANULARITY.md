# `mpi_accumulate_granularity` validation

ARMCI-MPI sets the MPI 4.1 `mpi_accumulate_granularity` window info hint on
all GMR and mutex windows.  `ARMCI_ACCUMULATE_GRANULARITY` selects the value
in bytes and defaults to 1048576.  A local window smaller than eight bytes
always receives a value of 1.

## Implementation source audit

- MPICH 5.0.1 parses the key into `MPIDIG_WIN(win, info_args)` and reports it
  through `MPI_Win_get_info`, but no RMA path reads the stored field.  The hint
  has no performance effect in this release.
- Open MPI 4 and 5 do not recognize the key in the available source trees.
- VibeMPI parses the standard key (and an older unprefixed alias), caps the
  value at each target window size, and uses it in typed accumulate.  An
  effective value below 128 takes a lock for each datatype leaf; a value of
  at least 128 holds one lock across the operation.

## VibeMPI experiment

A two-node Iris run used VibeMPI OFI over `verbs;ofi_rxm`.  Each timed
operation accumulated a 64-by-8 strided patch of doubles and fenced the target.
Each value was tested in five independent runs of 50 operations.  All 30 runs
completed correctly.

| Requested bytes | Mean latency (microseconds) | Correct runs |
|---:|---:|---:|
| 0 | 217.759 | 5/5 |
| 1 | 217.613 | 5/5 |
| 64 | 219.541 | 5/5 |
| 127 | 218.622 | 5/5 |
| 128 | 219.024 | 5/5 |
| 1048576 | 220.419 | 5/5 |

There is no meaningful performance separation in this experiment: the full
range of means is about 1.3%.  The 1 MiB default selects VibeMPI's coarse-lock
path without causing a measurable regression here.

A PMPI interposer also checked both GMR constructors.  With the environment
set to 777777, four-byte `MPI_Win_allocate` and `MPI_Win_create` windows both
received 1, while eight-byte windows received 777777.  Both functional tests
passed.  Two-byte queue-mutex windows also received 1, and the mutex test
passed.

The scripts, microbenchmark, complete logs, and tabular results are under
`armci-val/accumulate-granularity-20260722` in the validation workspace.

## Functional validation

- MPICH 5.0.1 OFI over TCP passed 43/43 tests on the login node.
- Open MPI 5.0.10 over TCP passed 43/43 tests on the login node.
- The VibeMPI two-node OFI focused sweep passed 30/30 runs.
- A complete VibeMPI OFI run did not finish within the five-minute suite cap.
  `contiguous-bench`, `strided-bench`, and `ARMCI_AccS_latency` each exceeded
  their 110-second per-test limit; the latter progressed through its
  256-by-1024 case before timing out.  These are recorded as failures.  No
  corruption or crash was observed before the suite was terminated.
