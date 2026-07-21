# Default production validation at 8ccfcbf

This report records the two-node production qualification of ARMCI-MPI commit
`8ccfcbfd115574fe297068e31b22f3ab8d778bd0`.  It follows the request-RMA
correctness matrix and validates the defaults selected by commit `895bc08`, the
Open MPI UCX request throttle from `779bba4`, and the deep-request reproducer
from `8ccfcbf`.

Every `make check` ran with `ARMCI_VERBOSE=2`.  No other `ARMCI_*` environment
variable was set.  A run passes only if it completes within five minutes and
the Automake summary is exactly 43/43.  A timeout is a failure.  TCP transports
were explicitly bound to the `ib0_mlx5` IPoIB interface rather than native
Ethernet.  All tests used two nodes with one MPI rank per node.

The complete logs and one-line TSV records are under
`../armci-val/default-production-sweep-8ccfcbf/results` relative to the
workspace containing this checkout.

## Default configuration observed

| MPI implementation | Allocate | Strided | Vector | Atomicity | Request atomics | Flush request atomics | RMA requests |
| --- | --- | --- | --- | --- | --- | --- | --- |
| MPICH 5.0.1 | yes | direct | batched | no | yes | no | yes |
| Open MPI 4 | yes | IOV | batched | no | no | no | yes |
| Open MPI 5.0.10 | yes | direct | batched | no | yes | no | yes |

These settings match the intended mitigations: Open MPI 4 avoids request-based
atomics and direct strided datatypes, while Open MPI 5 batches vector operations
but retains direct strided operations and request-based atomics.

## MPICH and Open MPI results

The parenthesized number is elapsed wall time in seconds.  `TIMEOUT` means the
five-minute suite bound expired and is counted as a failure even where the
individual tests completed before that point had passed.

| MPI / transport | Iris | Thor | Rome |
| --- | --- | --- | --- |
| MPICH 5 / UCX IB | 43/43 (96) | 43/43 (100) | TIMEOUT (301) |
| MPICH 5 / UCX TCP over IPoIB | 43/43 (129) | 43/43 (156) | TIMEOUT (301) |
| MPICH 5 / OFI verbs;ofi_rxm | 43/43 (159) | 43/43 (108) | 43/43 (126) |
| MPICH 5 / OFI PSM3 | 3/43 (13) | 3/43 (13) | 3/43 (29) |
| MPICH 5 / OFI TCP over IPoIB | 43/43 (166) | 43/43 (161) | 43/43 (177) |
| Open MPI 4 / UCX IB | 43/43 (139) | 43/43 (148) | TIMEOUT (301) |
| Open MPI 4 / UCX TCP over IPoIB | TIMEOUT (300) | TIMEOUT (300) | TIMEOUT (301) |
| Open MPI 5 / UCX IB | 43/43 (127) | 43/43 (134) | TIMEOUT (301) |
| Open MPI 5 / UCX TCP over IPoIB | 42/43 (275) | 42/43 (294) | TIMEOUT (300) |
| Open MPI 5 / OFI verbs;ofi_rxm | TIMEOUT (300) | TIMEOUT (300) | TIMEOUT (300) |
| Open MPI 5 / OFI PSM3 | 43/43 (169) | 43/43 (175) | 43/43 (205) |
| Open MPI 5 / OFI TCP over IPoIB | TIMEOUT (300) | TIMEOUT (300) | TIMEOUT (300) |

Rome used UCX and libfabric builds compiled against its older rdma-core ABI.
The successful MPICH OFI and Open MPI PSM3 runs demonstrate that this
compatibility setup works.  Rome's UCX runs and the remaining Open MPI OFI runs
do not meet the five-minute production acceptance bound.

### PSM3 distinction

Open MPI 5 with OFI PSM3 is a clean 43/43 path on all three platforms.  MPICH
5 with the same provider fails before ARMCI initialization.  PSM3 reports
`Failed to modify UD QP to INIT on mlx5_0: Operation not permitted`, after
which MPICH fails in `MPIDI_OFI_create_vci_context` while creating the OFI
endpoint.  This is an MPICH/provider startup incompatibility, not an ARMCI
failure, but it remains a failed production tuple and MPICH 5 must not select
PSM3 on these systems.

### Slow paths

Open MPI 5 UCX TCP reaches 42/43 on Iris and Thor; only `benchmarks/rmw_perf`
fails its per-test time bound.  Open MPI 4 UCX TCP and Open MPI 5 OFI
verbs/TCP exceed the outer five-minute bound earlier in the suite.  These are
not accepted as passes, regardless of whether the underlying issue is extreme
RMA latency rather than a data mismatch.

## Deep Open MPI request validation

`validation-armci-request-depth` issues 512 `ARMCI_NbPut` operations on one
handle, exceeding the prior Open MPI 5 UCX failure threshold.  It passes with
native UCX IB and UCX TCP over IPoIB on both Iris and Thor.  All four runs exit
zero in two to five seconds.  This directly validates the request-throttle
mitigation in `779bba4` in addition to the standard 43-test suite.

## Initial VibeMPI baseline

ARMCI-MPI was also built with the VibeMPI installation requested for follow-up
testing.  VibeMPI identifies itself as `vibempi 0.1 (MPI ABI 1.0)`.  The same
default ARMCI configuration is direct strided operations, batched vector
operations, no extra RMA atomicity, and request-based atomics.

| VibeMPI transport | Iris | Thor |
| --- | --- | --- |
| UCX IB | 40/43 (69) | 40/43 (73) |
| UCX TCP over IPoIB | 42/43 (131) | 42/43 (147) |
| OFI verbs;ofi_rxm | 42/43 (191) | 42/43 (192) |
| OFI PSM3 | 42/43 (119) | 42/43 (129) |
| OFI TCP over IPoIB | 42/43 (262) | 42/43 (283) |
| VibeMPI TCP over IPoIB | 42/43 (121) | 42/43 (128) |

All six transports fail `tests/contrib/armci-test` with a multidimensional
nonblocking data mismatch.  Native UCX IB additionally fails
`tests/mpi/test_mpi_dim` and `tests/contrib/armci-perf`; the accumulate result
in `armci-perf` is exactly half the expected value.  Because the common failure
appears over UCX, OFI, and VibeMPI TCP on both platforms, VibeMPI is not
qualified for ARMCI production use with the default direct-strided path.

A focused `ARMCI_STRIDED_METHOD=IOV` control produced the following results.
This is diagnostic evidence and is not part of the no-override default matrix.

| VibeMPI transport with strided IOV | Iris | Thor |
| --- | --- | --- |
| UCX IB | 41/43 (101) | 41/43 (107) |
| UCX TCP over IPoIB | 43/43 (148) | 43/43 (168) |
| OFI verbs;ofi_rxm | 40/43 (186) | 42/43 (209) |
| OFI PSM3 | 43/43 (151) | 43/43 (163) |
| OFI TCP over IPoIB | 43/43 (288) | TIMEOUT (300) |
| VibeMPI TCP over IPoIB | 43/43 (152) | 42/43 (147) |

IOV removes the common `armci-test` failure on several runs and provides two
repeatable 43/43 workarounds: UCX TCP over IPoIB and OFI PSM3.  It does not
make VibeMPI generally reliable.  Native UCX IB retains failures in the direct
MPI dimensional test and `armci-perf`; OFI verbs and VibeMPI TCP remain
platform-dependent; and OFI TCP exceeds the five-minute bound on Thor.
Consequently the initial VibeMPI checkout was not production-qualified.  These
results motivated fixes in VibeMPI rather than an ARMCI-MPI IOV workaround.

## VibeMPI RMA fixes and final validation

VibeMPI commit `9c5ae32de9df0187385c715903ecca1edcf32aaf` contains the RMA fixes
developed from the initial failures.  In particular, the changes correct packed
subarray ordering and request completion, order operations across rendezvous and
native paths, honor OFI atomic limits, repair UCX put local completion, and add
the application-progress worker flush required by detached UCX puts.  Direct
strided ARMCI operations therefore exercise MPI subarray datatypes in every run
below; no run uses `ARMCI_STRIDED_METHOD=IOV`.

The remaining OFI `verbs;ofi_rxm` investigation isolated corruption to native
window RMA when it shares a window domain with the active-message fallback.
Reducing the native-operation window to one, copying inbound payloads, changing
the stream fragment size, disabling internal rendezvous RMA, and routing only
derived operations through active messages did not eliminate corruption.  A
fully active-message window passes the exact large `ARMCI_AccS` reproducer both
with and without native rendezvous enabled.  VibeMPI therefore disables native
window RMA for this provider pair at `9c5ae32`; the diagnostic override
`VIBEMPI_OFI_NATIVE_RMA=1` restores the unsafe path.  This fallback is correct
but too slow to finish the complete ARMCI suite within the five-minute
production limit.

The final no-override ARMCI-MPI sweep produced the following results.  Every run
used `ARMCI_VERBOSE=2`, direct strided operations, batched vector operations,
two nodes, and IPoIB for TCP transports.  The elapsed time is in seconds.

| VibeMPI transport at `9c5ae32` | Iris | Thor |
| --- | --- | --- |
| UCX IB | 43/43 (111) | 43/43 (118) |
| UCX TCP over IPoIB | 43/43 (152), rerun | 43/43 (174) |
| OFI verbs;ofi_rxm | TIMEOUT (300) | TIMEOUT (300) |
| OFI PSM3 | 43/43 (163) | 43/43 (171) |
| OFI TCP over IPoIB | 43/43 (297) | TIMEOUT (300) |
| VibeMPI TCP over IPoIB | 43/43 (160) | 43/43 (171) |

The first final Iris UCX TCP attempt reached 42/43 in 218 seconds because
`armci-test` timed out in atomic swap.  That attempt remains a recorded failure;
an immediate identical rerun passed 43/43 in 152 seconds.  The final commit also
passes on Thor, but UCX IB is the less variable production choice.  OFI TCP on
Iris is too close to the acceptance limit to be a preferred production path.
Complete logs and TSV summaries are retained in
`default-production-sweep-8ccfcbf/results` with variants `final-9c5ae32` and
`ucx-tcp-rerun-9c5ae32`; focused verbs/RXM evidence is in
`iris-vibempi-ofi-verbs-rxm-force-am-armci-perf-0488a26`.

## Production decision

The ARMCI-MPI defaults are production-qualified on at least one high-performance
path for every primary MPI implementation:

- MPICH 5 passes with UCX IB and with OFI verbs/TCP on Iris and Thor, and with
  OFI verbs/TCP on Rome.
- Open MPI 4 passes with UCX IB on Iris and Thor.
- Open MPI 5 passes with UCX IB on Iris and Thor and with OFI PSM3 on all three
  platforms.
- VibeMPI `9c5ae32` passes with direct MPI subarray datatypes over UCX IB, OFI
  PSM3, and its native TCP transport on Iris and Thor.

Therefore commit `8ccfcbf` is ready for production with the qualified paths
above.  Provider selection remains material: MPICH PSM3 and all paths marked
`TIMEOUT` must be excluded.  VibeMPI is qualified only on the passing transports
listed above; its OFI verbs/RXM active-message mitigation is correctness-safe
but does not meet the full-suite performance bound.
