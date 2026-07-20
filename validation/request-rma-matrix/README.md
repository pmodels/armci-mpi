# Request-based RMA validation matrix

This directory validates the request-based RMA handle implementation against a
non-request baseline on two nodes.  It covers:

- MPICH 5 with CH4/UCX and CH4/OFI;
- Open MPI 5 with UCX and OFI;
- Open MPI 4 from HPC-X with UCX;
- UCX `rc_x` and OFI `verbs;ofi_rxm` over InfiniBand;
- UCX and OFI TCP providers;
- Iris, Thor, and Rome x86_64 nodes; and
- UCX and libfabric builds compiled against Rome's older rdma-core.

The available HPC-X Open MPI 4 installation does not contain an OFI MTL, BTL,
or OSC component, so Open MPI 4/OFI is unsupported and is not mislabeled as a
tested combination.

`build-matrix.sh` creates five builds for each source tree.  `submit-matrix.sh`
submits one 20-element array on each requested partition.  Each array element
selects one source tree, MPI/backend build, and fabric (`ib` or `tcp`).

The request-based tree runs `tests/contrib/armci-test` for these profiles:

| Profile | Environment change from defaults |
| --- | --- |
| `default` | none |
| `request_atomics_0` | `ARMCI_USE_REQUEST_ATOMICS=0` |
| `request_atomics_1` | `ARMCI_USE_REQUEST_ATOMICS=1` |
| `request_atomics_flush` | request atomics and their target flush enabled |
| `explicit_progress_0/1` | both `ARMCI_EXPLICIT_NB_PROGRESS` values |
| `rma_atomicity_0/1` | both `ARMCI_RMA_ATOMICITY` values |
| `no_flush_local_1` | `ARMCI_NO_FLUSH_LOCAL=1` |
| `rma_nocheck_0/1` | both `ARMCI_RMA_NOCHECK` values |
| `win_create` | `ARMCI_USE_WIN_ALLOCATE=0` |
| `alloc_shm_0` | `ARMCI_USE_ALLOC_SHM=0` |
| `disable_shm_acc_1` | `ARMCI_DISABLE_SHM_ACC=1` |
| `same_op_1` | `ARMCI_USE_SAME_OP=1` |
| `ordering_none` | `ARMCI_RMA_ORDERING=none` |
| `barrier_syncs_1` | `ARMCI_MSG_BARRIER_SYNCS=1` |
| `shr_copy/noguard` | both shared-buffer policies |
| `iov_auto/consrv/batched/direct` | every `ARMCI_IOV_METHOD` value |
| `iov_batched_limit_1` | one operation per BATCHED epoch |
| `iov_checks_1` | expensive VECTOR validation enabled |
| `strided_iov/direct` | both `ARMCI_STRIDED_METHOD` values |
| `strict_completion` | requests, target flushes, full flushes, and checked locks |

The default profile also runs focused scalar, strided, VECTOR, atomic, mutex,
and nonblocking-handle tests on both trees.  Every test runs with
`ARMCI_VERBOSE=2`, and complete stdout and stderr are retained under the shared
result directory.  The explicit `iov_direct` profile therefore records whether
Open MPI forced VECTOR operations back to `BATCHED`.

For TCP, Open MPI uses transport-specific component selections.  UCX RMA uses
the UCX OSC while OB1/TCP carries MPI point-to-point traffic, because the UCX
PML intentionally refuses a TCP-only UCX configuration.  OFI uses OB1 with the
OFI BTL in two-sided-and-one-sided mode, because the OFI MTL rejects the TCP
provider.  These selections keep the ARMCI window path on the backend named by
the matrix while allowing MPI control traffic to start reliably.

Typical use is:

```sh
validation/request-rma-matrix/build-matrix.sh \
    /path/to/src-master /path/to/src-pr /shared/path/builds

LIBFABRIC_SOURCE=$HOME/MPI/external/mpi-releases/src/libfabric-2.6.0 \
LIBFABRIC_INSTALL=/shared/path/builds/libfabric-2.6.0-rdma114 \
sbatch validation/request-rma-matrix/build-libfabric-rome.sbatch

SBATCH_ACCOUNT=project \
validation/request-rma-matrix/submit-matrix.sh \
    /shared/path/builds /shared/path/results
```

Wait for the Rome libfabric build to finish before submitting the matrix.  Set
`LIBFABRIC_COMPAT` during submission if its install path differs from
`BUILD_DIRECTORY/libfabric-2.6.0-rdma114`.

The result directory contains one TSV and one log directory per array task.
Run `summarize-matrix.sh RESULTS` after all jobs finish to produce a combined
summary and list every failure or timeout.
