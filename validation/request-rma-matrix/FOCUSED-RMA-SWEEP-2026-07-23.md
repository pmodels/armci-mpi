# Focused RMA correctness sweep, 2026-07-23

## Scope

This sweep validates ARMCI-MPI commit
`01afc591afaf556753370cdd6b0528f80fc34415` on two Iris nodes.  Every
invocation used `ARMCI_VERBOSE=2`.  TCP profiles were pinned to the active
IPoIB interface rather than a native Ethernet interface.

Each MPI and transport profile was tested with 13 ARMCI environment tuples.
For every tuple, the two-rank `armci-test` ran first.  The two eight-rank
focused tests ran only if `armci-test` passed:

1. `test_acc_overlap`, which overlaps contiguous, strided, and vector
   accumulates for every ARMCI element type;
2. `test_location_consistency`, which mixes contiguous, strided, and vector
   put, accumulate, and get operations without an intervening flush.

The authoritative Slurm array was job `622006`.  It produced 1,131 test-slot
records: 830 passes, 149 failures, and 152 deliberate skips behind a failed
`armci-test` gate.  The raw summaries and individual verbose logs are in:

```text
~/Claude/armci-val/focused-rma-sweep-01afc59/results/
~/Claude/armci-val/focused-rma-sweep-01afc59/slurm/
```

An earlier partial run at `cb498f1` was cancelled after the accumulate
granularity implementation changed.  None of its results are included here.

## Environment tuples

The columns below use these abbreviations:

- `A`: `ARMCI_USE_WIN_ALLOCATE`
- `S`: `ARMCI_STRIDED_METHOD`
- `V`: `ARMCI_IOV_METHOD`
- `R`: `ARMCI_RMA_ATOMICITY`
- `F`: `ARMCI_FLUSH_REQUEST_ATOMICS`
- `U`: `ARMCI_USE_REQUEST_ATOMICS`
- `G`: `ARMCI_ACCUMULATE_GRANULARITY`
- `-`: unset, selecting the compiled default

| ID | Tuple | A | S | V | R | F | U | G |
|---|---|---:|---|---|---:|---:|---:|---:|
| D | default | - | - | - | - | - | - | - |
| I | ideal | 1 | DIRECT | DIRECT | 0 | 1 | 1 | - |
| P | portable | 1 | DIRECT | BATCHED | 0 | 1 | 0 | - |
| RN | request_no_flush | 1 | DIRECT | BATCHED | 0 | 0 | 1 | - |
| NF | no_request_no_flush | 1 | DIRECT | BATCHED | 0 | 0 | 0 | - |
| A0 | allocate_zero | 0 | DIRECT | BATCHED | 0 | 1 | 0 | - |
| SI | strided_iov | 1 | IOV | BATCHED | 0 | 1 | 0 | - |
| VD | vector_direct_no_requests | 1 | DIRECT | DIRECT | 0 | 1 | 0 | - |
| AT | atomicity | 1 | DIRECT | BATCHED | 1 | 1 | 0 | - |
| AR | atomicity_requests | 1 | DIRECT | DIRECT | 1 | 1 | 1 | - |
| G1 | granularity_one | 1 | DIRECT | BATCHED | 0 | 1 | 0 | 1 |
| C | conservative | 0 | IOV | BATCHED | 1 | 0 | 0 | 1 |
| X | alternate_extreme | 0 | IOV | DIRECT | 0 | 0 | 1 | - |

The observed compiled defaults were `A=1`, `S=DIRECT`, `V=BATCHED`, `R=0`,
`F=0`, `U=1`, and `G=1048576`.  Thus D and RN select the same reported ARMCI
configuration.  Different outcomes between those columns indicate
nondeterministic MPI behavior rather than a meaningful configuration
difference.

## Result legend

- `PASS`: all three tests passed.
- `A`: `armci-test` failed or timed out; both focused tests were skipped.
- `O`: `test_acc_overlap` failed after `armci-test` passed.
- `L`: `test_location_consistency` failed after `armci-test` passed.
- `OL`: both focused tests failed after `armci-test` passed.

All 180-second timeouts are failures.

## MPICH 5

| Transport | D | I | P | RN | NF | A0 | SI | VD | AT | AR | G1 | C | X |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UCX IB | A | A | A | A | A | A | PASS | PASS | PASS | PASS | A | PASS | A |
| UCX TCP/IPoIB | A | A | A | A | A | A | PASS | A | PASS | PASS | A | PASS | PASS |
| OFI verbs/RxM | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| OFI PSM3 | A | A | A | A | A | A | A | A | A | A | A | A | A |
| OFI TCP/IPoIB | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |

Both OFI verbs/RxM and OFI TCP/IPoIB passed all 39 invocations.  UCX failures
are explicit `armci-test` data mismatches in its nonblocking get/put section;
MPI subsequently exits with status zero, but the log contains the mismatch
and `ARMCI Error: Bailing out`.  The pure-MPI investigation identifies a
distinct MPICH CH4/UCX defect: noncontiguous `MPI_Rget` returns a completed
request before its active-message fallback has transferred any data.  See
[`MPICH5-UCX-RGET-DERIVED-BUG-REPORT.md`](MPICH5-UCX-RGET-DERIVED-BUG-REPORT.md).
This is not the large-accumulate active-message header overflow in MPICH issue
7886.

The MPICH OFI PSM3 provider fails during `MPI_Init`, before ARMCI
initialization.  PSM3 attempts to initialize an unusable `mlx5_0` UD queue
pair and reports `Operation not permitted` followed by an OFI endpoint
allocation failure.

## Open MPI 4

| Transport | D | I | P | RN | NF | A0 | SI | VD | AT | AR | G1 | C | X |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UCX IB | PASS | A | PASS | A | PASS | PASS | PASS | PASS | PASS | A | PASS | PASS | A |
| UCX TCP/IPoIB | PASS | A | PASS | A | A | PASS | PASS | L | L | A | PASS | PASS | A |

The four explicitly configured `U=1` tuples time out on both transports,
confirming that `ARMCI_USE_REQUEST_ATOMICS=0` remains the safe Open MPI 4
mitigation.  D reports the same ARMCI settings as RN but passes, so this run
also demonstrates nondeterminism in that failure.  The TCP profile has one
additional `U=0` timeout and two location-consistency failures in which rank
zero prints `PASS`, but remote ranks receive `SIGBUS` while UCX endpoints are
closing.

## Open MPI 5

| Transport | D | I | P | RN | NF | A0 | SI | VD | AT | AR | G1 | C | X |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UCX IB | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| UCX TCP/IPoIB | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | A | PASS | PASS | PASS | PASS |
| OFI verbs/RxM | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| OFI PSM3 | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| OFI TCP/IPoIB | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |

Four of five profiles passed all 39 invocations.  UCX TCP/IPoIB failed only
the AT tuple: `armci-test` timed out during a UCX worker flush after 180
seconds.  UCX IB passed the same tuple.

## Open MPI 6 development snapshot

| Transport | D | I | P | RN | NF | A0 | SI | VD | AT | AR | G1 | C | X |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UCX IB | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| UCX TCP/IPoIB | PASS | PASS | PASS | A | PASS | PASS | A | A | PASS | PASS | PASS | PASS | PASS |
| OFI verbs/RxM | OL | OL | OL | OL | OL | PASS | OL | OL | OL | OL | OL | PASS | PASS |
| OFI PSM3 | OL | OL | OL | OL | OL | PASS | OL | OL | OL | OL | OL | PASS | PASS |
| OFI TCP/IPoIB | OL | OL | OL | OL | OL | PASS | OL | OL | OL | OL | OL | PASS | PASS |

UCX IB passed all 39 invocations.  The three UCX TCP/IPoIB `A` cells are
180-second `armci-test` timeouts.

All three OFI providers have the same exact split: every tuple with
`ARMCI_USE_WIN_ALLOCATE=0` passes, while every tuple with window allocation
enabled passes two-rank `armci-test` and then fails both eight-rank focused
tests.  Open MPI aborts from OFI progress after `fi_cq_readerr` reports
`Permission denied` with provider error 8.  The provider-independent pattern
points to the Open MPI 6 development OFI BTL/window-allocation path.

## MVAPICH 4.1

| Transport | D | I | P | RN | NF | A0 | SI | VD | AT | AR | G1 | C | X |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UCX IB | A | A | A | A | PASS | PASS | PASS | A | PASS | PASS | A | PASS | A |
| UCX TCP/IPoIB | A | A | A | A | A | A | PASS | A | PASS | PASS | PASS | PASS | A |
| OFI verbs/RxM | PASS | A | PASS | PASS | PASS | PASS | PASS | A | PASS | PASS | PASS | PASS | A |
| OFI PSM3 | A | A | A | A | A | A | A | A | A | A | A | A | A |
| OFI TCP/IPoIB | PASS | A | PASS | PASS | PASS | PASS | PASS | A | PASS | PASS | PASS | PASS | A |

UCX `A` cells are primarily explicit data mismatches in the nonblocking
get/put section of `armci-test`, like MPICH 5 UCX.  The two usable OFI
providers fail the same I, VD, and X tuples; the logs include process-manager
assertions or application segmentation faults after the ARMCI test failure.
MVAPICH OFI PSM3 fails during MPI initialization in the same way as MPICH
OFI PSM3.

## VibeMPI

| Transport | D | I | P | RN | NF | A0 | SI | VD | AT | AR | G1 | C | X |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| auto | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| native TCP/IPoIB | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| mux | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| OFI verbs/RxM | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| OFI TCP/IPoIB | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| UCX IB | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS |
| UCX TCP/IPoIB | L | L | L | L | PASS | PASS | L | L | L | L | L | L | L |

Six of seven VibeMPI profiles passed all 39 invocations.  In the UCX
TCP/IPoIB profile, all 13 `armci-test` gates and all 13 overlapping
accumulate tests pass.  Eleven location-consistency invocations print `PASS`
on rank zero, but other ranks then receive `SIGBUS` during shutdown.  NF and
A0 are the only complete passes for that transport in this run.

The VibeMPI library identified itself as commit
`bd27d4e5497922f31a5eced8c65148490e79185b`.

## Cross-profile conclusions

Every MPI implementation has at least one transport and environment tuple
that passes all three tests:

- MPICH 5: OFI verbs/RxM and OFI TCP/IPoIB pass every tuple.
- Open MPI 4: both UCX transports have multiple passing tuples; request
  atomics must remain disabled.
- Open MPI 5: four profiles pass every tuple, and UCX TCP/IPoIB passes 12 of
  13.
- Open MPI 6 development: UCX IB passes every tuple.
- MVAPICH 4.1: every non-PSM3 profile has passing tuples.
- VibeMPI: six profiles pass every tuple; UCX TCP/IPoIB has two passing
  tuples.

Across all 29 profiles, the conservative C tuple has the widest coverage:
26 complete passes.  A0 has 24, SI has 22, AT and AR each have 21, and NF
and G1 each have 20.  No single tuple passes all profiles because MPICH and
MVAPICH cannot initialize their PSM3 providers on Iris.

The result matrix is a sample, not proof of determinism.  D and RN report the
same ARMCI settings but sometimes differ, and several failures occur during
MPI teardown after the focused test has printed `PASS`.  Those cases should
be treated as MPI correctness failures and rerun when the corresponding MPI
implementation changes.
