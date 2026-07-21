# Pure-MPI request-RMA bug reports

## Context

These defects were discovered while validating request-based RMA handles for
[ARMCI-MPI pull request 53](https://github.com/pmodels/armci-mpi/pull/53).
Both failures have been reduced to two-process MPI C programs with no ARMCI
dependency.  They reproduce with one rank per node over InfiniBand.  The
Open MPI failure also reproduces with UCX over TCP, and the MPICH failure also
reproduces with the OFI TCP provider.

The defects are independent of these previously reported problems:

- [Open MPI issue 14181](https://github.com/open-mpi/ompi/issues/14181), an
  indexed-datatype `MPI_Accumulate` correctness failure;
- [Open MPI issue 14175](https://github.com/open-mpi/ompi/issues/14175), a
  derived-datatype `MPI_Accumulate` crash;
- [Open MPI issue 14173](https://github.com/open-mpi/ompi/issues/14173), a
  contended atomic-swap hang; and
- [MPICH issue 7886](https://github.com/pmodels/mpich/issues/7886), a CH4/UCX
  active-message header overflow involving a large accumulate datatype.

## Open MPI 5/UCX: the 255th outstanding `MPI_Rput` aborts

### Summary

Open MPI 5.0.10rc2 with UCX 1.21 aborts when one process initiates 255
`MPI_Rput` operations to a peer before completing any returned request.  A run
with 254 otherwise identical requests completes.

The datatype is the predefined `MPI_INT`; no derived datatype or accumulate
operation is involved.

### Build and run

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    ompi-ucx-rput-minimal.c -o ompi-ucx-rput-minimal
mpirun -n 2 ./ompi-ucx-rput-minimal
```

Compile the adjacent passing control with:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror -DREQUEST_COUNT=254 \
    ompi-ucx-rput-minimal.c -o ompi-ucx-rput-254
mpirun -n 2 ./ompi-ucx-rput-254
```

### Expected behavior

All 255 `MPI_Rput` calls return valid requests, `MPI_Waitall` completes them,
and the program exits successfully.

### Observed behavior

Request 255 aborts in UCX with an assertion equivalent to:

```text
flush.c: Assertion `(next_ep_ext->ep)->refcount < UINT8_MAX' failed
```

The relevant call path is:

```text
MPI_Rput
ompi_osc_ucx_rput
opal_common_ucx_wpmem_flush_ep_nb
ucp_worker_flush_nb
ucp_worker_flush_req_set_next_ep
ucp_ep_refcount_add
```

Open MPI implements each `MPI_Rput` request by starting a nonblocking UCX
worker flush.  Each unfinished worker flush retains the endpoint.  UCX stores
the endpoint reference count in eight bits.  The endpoint's existing reference
plus 254 unfinished flushes fills the counter, and the 255th flush triggers the
assertion.

### Complete source: `ompi-ucx-rput-minimal.c`

```c
#include <mpi.h>

#include <stdio.h>

#ifndef REQUEST_COUNT
#define REQUEST_COUNT 255
#endif

static void check(int rc, const char *name)
{
    if (rc != MPI_SUCCESS) {
        fprintf(stderr, "%s failed\n", name);
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
}

int main(int argc, char **argv)
{
    check(MPI_Init(&argc, &argv), "MPI_Init");

    int rank;
    int size;
    check(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    check(MPI_Comm_size(MPI_COMM_WORLD, &size), "MPI_Comm_size");
    if (size != 2) MPI_Abort(MPI_COMM_WORLD, 1);

    int *target = NULL;
    MPI_Win window;
    const MPI_Aint bytes = rank == 0 ? (MPI_Aint)sizeof(*target) : 0;
    check(MPI_Win_allocate(bytes, sizeof(*target), MPI_INFO_NULL, MPI_COMM_WORLD,
                           &target, &window),
          "MPI_Win_allocate");
    if (rank == 0) *target = 0;

    check(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");
    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");

    if (rank == 1) {
        int values[REQUEST_COUNT];
        MPI_Request requests[REQUEST_COUNT];
        for (int i = 0; i < REQUEST_COUNT; ++i) {
            values[i] = i;
            check(MPI_Rput(&values[i], 1, MPI_INT, 0, 0, 1, MPI_INT, window,
                           &requests[i]),
                  "MPI_Rput");
        }
        check(MPI_Waitall(REQUEST_COUNT, requests, MPI_STATUSES_IGNORE),
              "MPI_Waitall");
    }

    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");
    check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    check(MPI_Win_free(&window), "MPI_Win_free");
    check(MPI_Finalize(), "MPI_Finalize");
    return 0;
}
```

## MPICH 5/OFI: packed noncontiguous `MPI_Rput` dereferences a NULL request

### Summary

MPICH 5.0.1 CH4/OFI receives signal 11 inside `MPI_Rput` when the origin is a
sparse indexed datatype and the target indexed datatype describes adjacent
blocks.  Two blocks of 20 doubles with a one-double origin gap are sufficient.

The same operation succeeds with blocking `MPI_Put`.  The request-based
program also succeeds with CH4/UCX.  Freeing the datatypes is not involved,
because the crash occurs before `MPI_Rput` returns.

### Build and run

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    mpich-ofi-rput-minimal.c -o mpich-ofi-rput-minimal
mpiexec -n 2 ./mpich-ofi-rput-minimal
```

Compile the blocking-operation control with:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror -DUSE_BLOCKING_PUT \
    mpich-ofi-rput-minimal.c -o mpich-ofi-put-control
mpiexec -n 2 ./mpich-ofi-put-control
```

### Expected behavior

`MPI_Rput` returns a valid request, `MPI_Wait` completes it, and the program
exits successfully.

### Observed behavior and root cause

The request-based program receives signal 11 with this call path:

```text
issue_packed_put
MPIDI_OFI_INIT_CHUNK_CONTEXT
MPIDI_OFI_pack_put
MPIDI_OFI_do_put
MPIDI_NM_mpi_rput
MPI_Rput
```

`MPIDI_OFI_pack_put` saves the non-NULL address of the caller's output request
pointer but does not create the `MPIR_Request`.  `issue_packed_put` calls
`MPIDI_OFI_INIT_CHUNK_CONTEXT`, which evaluates
`MPIR_cc_inc((*sigreq)->cc_ptr)` and dereferences the NULL `*sigreq`.

MPICH's no-pack path explicitly calls
`MPIDI_OFI_REQUEST_CREATE(*sigreq, MPIR_REQUEST_KIND__RMA, vci)` before
initializing a completion chunk.  The packed put and get paths omit the
equivalent request creation.  The omission is present in MPICH 5.0.1 and was
also present in the MPICH `main` branch when this report was prepared.

### Complete source: `mpich-ofi-rput-minimal.c`

```c
#include <mpi.h>

#include <stdio.h>

static void check(int rc, const char *name)
{
    if (rc != MPI_SUCCESS) {
        fprintf(stderr, "%s failed\n", name);
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
}

int main(int argc, char **argv)
{
    check(MPI_Init(&argc, &argv), "MPI_Init");

    int rank;
    int size;
    check(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    check(MPI_Comm_size(MPI_COMM_WORLD, &size), "MPI_Comm_size");
    if (size != 2) MPI_Abort(MPI_COMM_WORLD, 1);

    double *target = NULL;
    MPI_Win window;
    const MPI_Aint bytes = rank == 0 ? 40 * (MPI_Aint)sizeof(*target) : 0;
    check(MPI_Win_allocate(bytes, sizeof(*target), MPI_INFO_NULL, MPI_COMM_WORLD,
                           &target, &window),
          "MPI_Win_allocate");

    check(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");
    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");

    if (rank == 1) {
        double origin[41];
        for (int i = 0; i < 20; ++i) {
            origin[i] = i;
            origin[21 + i] = 20 + i;
        }

        const int origin_displacements[2] = {0, 21};
        const int target_displacements[2] = {0, 20};
        MPI_Datatype origin_type;
        MPI_Datatype target_type;
        check(MPI_Type_create_indexed_block(2, 20, origin_displacements,
                                            MPI_DOUBLE, &origin_type),
              "MPI_Type_create_indexed_block origin");
        check(MPI_Type_create_indexed_block(2, 20, target_displacements,
                                            MPI_DOUBLE, &target_type),
              "MPI_Type_create_indexed_block target");
        check(MPI_Type_commit(&origin_type), "MPI_Type_commit origin");
        check(MPI_Type_commit(&target_type), "MPI_Type_commit target");

#ifdef USE_BLOCKING_PUT
        check(MPI_Put(origin, 1, origin_type, 0, 0, 1, target_type, window),
              "MPI_Put");
#else
        MPI_Request request;
        check(MPI_Rput(origin, 1, origin_type, 0, 0, 1, target_type, window,
                       &request),
              "MPI_Rput");
        check(MPI_Wait(&request, MPI_STATUS_IGNORE), "MPI_Wait");
#endif
        check(MPI_Win_flush(0, window), "MPI_Win_flush");
        check(MPI_Type_free(&target_type), "MPI_Type_free target");
        check(MPI_Type_free(&origin_type), "MPI_Type_free origin");
    }

    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");
    check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    check(MPI_Win_free(&window), "MPI_Win_free");
    check(MPI_Finalize(), "MPI_Finalize");
    return 0;
}
```

## Application mitigations

Until the MPI implementations are fixed:

- limit Open MPI/UCX to fewer than 255 simultaneously outstanding request-RMA
  operations per endpoint; and
- avoid MPICH CH4/OFI `MPI_Rput` for sparse origins with a contiguous target,
  for example by batching blocking puts or using the atomic replacement path
  where its semantics and cost are acceptable.
