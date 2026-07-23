# Latest default 45-test production validation

## Scope

ARMCI-MPI commit `62796a648d23043f475eeac9bd2af2dbbe2139e8` was built independently against MPICH 5.0.1, Open MPI 4, Open MPI 5.0.10, the installed Open MPI 6 development snapshot, and VibeMPI `af1b8482483aa3c75537d81e285526b8dc87025b`.

Every run used two nodes with one rank per node. InfiniBand profiles selected an active native InfiniBand device, and the VibeMPI TCP profile explicitly used the active `ib0_mlx5` IPoIB interface rather than a native Ethernet adapter.

No ARMCI tuning variable was set except `ARMCI_VERBOSE=2`. A run passed only if `make check` completed within 300 seconds and the Automake summary reported exactly 45 total tests, 45 passes, zero failures, and zero skips.

## Accepted production witnesses

| MPI and transport | Iris | Thor |
|---|---:|---:|
| MPICH 5.0.1, OFI verbs/RxM over InfiniBand | 45/45 in 108 seconds | 45/45 in 114 seconds |
| Open MPI 4, UCX over InfiniBand | 45/45 in 138 seconds | 45/45 in 149 seconds |
| Open MPI 5.0.10, UCX over InfiniBand | 45/45 in 128 seconds | 45/45 in 135 seconds |
| Open MPI 6 development snapshot, UCX over InfiniBand | 45/45 in 132 seconds | 45/45 in 140 seconds |
| VibeMPI `af1b848`, native TCP over IPoIB | 45/45 in 148 seconds | 45/45 in 161 seconds |

Every primary MPI implementation therefore has a clean default production witness on both Iris and Thor with the expanded suite containing the overlapping-accumulate and mixed-location-consistency tests.

## Rejected VibeMPI alternatives on Iris

| Transport | Result | Classification |
|---|---:|---|
| UCX over InfiniBand | Outer timeout at 300 seconds after 18 observed passes | Performance failure; no correctness failure was observed |
| OFI verbs/RxM over InfiniBand | 44/45 in 296 seconds | `ARMCI_PutS_latency` exceeded its 130-second per-test limit |

These alternatives remain rejected under the production runtime policy. They do not affect qualification because VibeMPI native TCP over IPoIB passed the complete suite on both systems.

## Artifacts

Complete `ARMCI_VERBOSE=2` logs, environment records, Automake logs, one-line TSV summaries, and Slurm output are retained under `/global/home/users/jehammond/Claude/armci-val/latest-default-45-62796a6/results`.

## Decision

The accumulate-granularity implementation and the expanded correctness suite are production-qualified on Iris and Thor. The feature branch is based directly on master commit `f647cb33f6c7c92605666ef89fd9a7f56a6edb02` and may be fast-forwarded into master without a rebase.
