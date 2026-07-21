# `MPI_Rput` aborts at 255 outstanding requests with Open MPI 5 and UCX

## Description

Open MPI 5.0.10rc2 with UCX 1.21 aborts when one process initiates 255
`MPI_Rput` operations to a peer before completing any returned request.  An
otherwise identical run with 254 requests completes successfully.

The reproducer uses the predefined `MPI_INT` datatype.  No derived datatype or
accumulate operation is involved.  The failure reproduces with UCX over both
InfiniBand and TCP.

This was found while validating request-based RMA handles for
[ARMCI-MPI pull request 53](https://github.com/pmodels/armci-mpi/pull/53), but
the reproducer below is pure MPI and has no ARMCI dependency.

## Reproducer

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

Build and run with two processes on separate nodes:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    ompi-ucx-rput-minimal.c -o ompi-ucx-rput-minimal
mpirun -n 2 ./ompi-ucx-rput-minimal
```

Build the adjacent working control by changing only the request count:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror -DREQUEST_COUNT=254 \
    ompi-ucx-rput-minimal.c -o ompi-ucx-rput-254
mpirun -n 2 ./ompi-ucx-rput-254
```

## Expected behavior

All 255 `MPI_Rput` calls return valid requests, `MPI_Waitall` completes them,
and both processes exit successfully.

## Observed behavior

The 255-request program aborts with signal 6.  The relevant assertion and
stack are:

```text
flush.c: Assertion `(next_ep_ext->ep)->refcount < UINT8_MAX' failed
ucp_worker_flush_req_set_next_ep
ucp_worker_flush_nbx
ucp_worker_flush_nb
opal_common_ucx_wpmem_flush_ep_nb
ompi_osc_ucx_rput
PMPI_Rput
```

The program compiled with `REQUEST_COUNT=254` exits with status zero.  Both
outcomes were repeated on two independent two-node systems.

## Root-cause analysis

`ompi_osc_ucx_rput` implements request completion by calling
`opal_common_ucx_wpmem_flush_ep_nb`.  Despite the helper's name, it starts a
nonblocking UCX worker flush.  Each unfinished worker flush retains the same
UCP endpoint.

UCX stores the endpoint reference count in eight bits.  The endpoint's
existing reference plus 254 unfinished worker flushes fills the counter.  The
255th `MPI_Rput` starts another flush, reaches `ucp_ep_refcount_add`, and
triggers the `refcount < UINT8_MAX` assertion.

The native stack reaches `PMPI_Rput` directly, so a PMPI tracing interposer is
not required to establish the call sequence.

## Related issues

This does not appear to be covered by:

- [Open MPI issue 14181](https://github.com/open-mpi/ompi/issues/14181), which
  is an indexed-datatype `MPI_Accumulate` correctness failure;
- [Open MPI issue 14175](https://github.com/open-mpi/ompi/issues/14175), which
  is a derived-datatype `MPI_Accumulate` crash; or
- [Open MPI issue 14173](https://github.com/open-mpi/ompi/issues/14173), which
  is a contended atomic-swap hang.

This reproducer uses only predefined datatypes and `MPI_Rput`, and it fails at
a precise outstanding-request count.

