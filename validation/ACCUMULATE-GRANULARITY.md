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
- The original complete VibeMPI OFI run did not finish within the five-minute
  suite cap.
  `contiguous-bench`, `strided-bench`, and `ARMCI_AccS_latency` each exceeded
  their 110-second per-test limit; the latter progressed through its
  256-by-1024 case before timing out.  These are recorded as failures.  No
  corruption or crash was observed before the suite was terminated.

## VibeMPI timeout root cause

The VibeMPI failures are performance timeouts in its default OFI `native` RMW
protocol, not failures of the accumulate-granularity hint.  VibeMPI maps a
contiguous predefined reduction to native provider atomics and deliberately
decomposes it into one scalar `fi_atomic` operation per basic element.  A
large `MPI_Accumulate` therefore posts tens of thousands of fabric atomics.

ARMCI's strided test uses a stride of 1024 doubles.  A 512-column patch stays
noncontiguous and uses VibeMPI's packed AM path, while a 1024-column patch
collapses to one contiguous region and enters the scalar-native-atomic path.
This creates the apparent latency cliff:

| ARMCI patch | Native OFI | Native RMA disabled |
|---:|---:|---:|
| 64 by 512 | 0.917 ms | 0.872 ms |
| 64 by 1024 | 1058.420 ms | 1.225 ms |
| 256 by 512 | 3.301 ms | 3.403 ms |
| 256 by 1024 | 4240.351 ms | 4.366 ms |

A pure-MPI reproducer confirms linear scaling with the number of scalar
atomics:

| Doubles | Bytes | Native OFI | Native RMA disabled |
|---:|---:|---:|---:|
| 512 | 4096 | 11.515 ms | 2.445 ms |
| 1024 | 8192 | 18.966 ms | 2.438 ms |
| 8192 | 65536 | 136.105 ms | 2.547 ms |
| 65536 | 524288 | 1061.284 ms | 4.332 ms |

Pure `MPI_Get` and `MPI_Put` tests from 4088 through 8192 bytes do not have a
size cliff, which rules out generic `fi_read`, `fi_write`, and RXM eager-limit
behavior.  Changing `mpi_accumulate_granularity` from 1048576 to 1 also leaves
the 256-by-1024 time unchanged at about 4.24 seconds.

VibeMPI's newer `VIBEMPI_OFI_RMW_PROTOCOL=locked` path is the effective
mitigation.  With the current `fec427b` library preloaded, the 256-by-1024 case
takes 5.045 ms with `locked` versus 4235.717 ms with the default `native`
protocol, an approximately 840-fold speedup.  The installed `a2e462b` library
predates that protocol.  VibeMPI should either default OFI RMW to `locked` or
otherwise select one atomicity domain per window that does not scalarize bulk
accumulates; simply mixing a large-operation AM fallback with provider atomics
would not preserve MPI accumulate atomicity.

## Retest with provider-sized vector atomics

VibeMPI commit `5ab2a13125dc24f861b068c199e00811ea5ef748`, which includes
`36a1c02` (provider-sized vector atomics), removes the scalar submission
overhead. The current build was preloaded because the installed VibeMPI
library was still at `a2e462b`. With native OFI RMA, the default native RMW
protocol, and bundled libfabric `verbs;ofi_rxm`, the focused measurements were:

| Operation | Old native implementation | Provider-sized implementation | Result |
|---|---:|---:|---:|
| Pure MPI, 65536 doubles | 1061.284 ms | 4.868 ms | correct |
| ARMCI, 256 by 1024 | 4240.351 ms | 2.213 ms | correct |

The performance fix exposes a separate correctness problem under concurrent
derived-datatype accumulates. The complete default suite finished in 3:49 but
passed only 42/43 tests. `tests/contrib/armci-test` lost an update in the
three-dimensional atomic-accumulate case. Three independent isolated reruns
all failed, in 35--37 seconds, at the two- or three-dimensional accumulate
case. Representative failures were:

```
ERROR: a [0,1] (proc=1):2000.000000 b [0,1] 1990.000000
ERROR: a [1,0] (proc=1):20.000000 b [1,0] 19.800000
ERROR: a [1,0,0] (proc=1):20.000000 b [1,0,0] 19.900000
```

The control matrix isolates the failure to vector provider atomics used for
the direct strided datatype path:

| VibeMPI native RMA | RMW protocol | ARMCI strided method | `armci-test` |
|---:|---|---|---:|
| 1 | native | DIRECT | failed 3/3 |
| 1 | locked | DIRECT | passed |
| 0 | native | DIRECT | passed |
| 1 | native | IOV | passed |

The previous VibeMPI implementation explicitly limited provider atomics to one
element because some providers accepted multi-element requests but silently
updated only a prefix. The new implementation trusts the count returned by
`fi_atomicvalid`. These results show that a multi-element atomic accepted by
verbs/RxM is not reliable under the concurrent reductions in `armci-test`.
This is not a datatype-construction failure: the same direct derived datatypes
pass with the locked protocol, and the same native protocol passes when ARMCI
decomposes the operation through IOV.

As a production workaround, `VIBEMPI_OFI_RMW_PROTOCOL=locked` passed the clean
two-node suite 43/43 in 3:02. The default native protocol is therefore not yet
production-safe over `verbs;ofi_rxm`, despite the focused single-operation
performance improvement. Full logs and focused controls are retained under
`armci-val/accumulate-granularity-20260722`.
