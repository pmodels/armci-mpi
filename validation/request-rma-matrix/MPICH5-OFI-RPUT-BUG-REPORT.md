# Packed noncontiguous `MPI_Rput` dereferences a NULL request in MPICH 5 CH4/OFI

## Description

MPICH 5.0.1 CH4/OFI receives signal 11 inside `MPI_Rput` when the origin is a
sparse indexed datatype and the target indexed datatype describes adjacent
blocks.  The minimal reproducer needs only two blocks of 20 doubles separated
by a one-double gap at the origin.

The same datatype transfer succeeds with non-request-based `MPI_Put`.  The
request-based program also succeeds with CH4/UCX.  The crash occurs before
`MPI_Rput` returns, so datatype-free timing is not involved.

This was found while validating request-based RMA handles for
[ARMCI-MPI pull request 53](https://github.com/pmodels/armci-mpi/pull/53), but
the reproducer below is pure MPI and has no ARMCI dependency.

## Reproducer

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

#ifdef USE_NONREQUEST_PUT
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

Build and run with two processes on separate nodes:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    mpich-ofi-rput-minimal.c -o mpich-ofi-rput-minimal
mpiexec -n 2 ./mpich-ofi-rput-minimal
```

Build the non-request-based control by changing only `MPI_Rput` to `MPI_Put`
through the supplied compile-time switch:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror -DUSE_NONREQUEST_PUT \
    mpich-ofi-rput-minimal.c -o mpich-ofi-put-control
mpiexec -n 2 ./mpich-ofi-put-control
```

## Expected behavior

`MPI_Rput` returns a valid request, `MPI_Wait` completes it, and both processes
exit successfully.

## Observed behavior

The request-based program receives signal 11.  The relevant stack is:

```text
issue_packed_put
MPIDI_OFI_INIT_CHUNK_CONTEXT
MPIDI_OFI_pack_put
MPIDI_OFI_do_put
MPIDI_NM_mpi_rput
PMPI_Rput
```

The program compiled with `USE_NONREQUEST_PUT` exits with status zero.  The
request failure and non-request control were repeated over OFI verbs/RXM and
OFI TCP on two independent two-node systems.  The request program passes with
CH4/UCX over both transports.

## Root-cause analysis

`MPIDI_OFI_pack_put` stores the non-NULL address of the caller's output request
pointer in its internal window request, but it does not create the
`MPIR_Request` to which that pointer must refer.  `issue_packed_put` then calls
`MPIDI_OFI_INIT_CHUNK_CONTEXT`, whose first request operation is equivalent to:

```c
MPIR_cc_inc((*sigreq)->cc_ptr);
```

At this point `sigreq` is non-NULL but `*sigreq` is NULL, causing the
segmentation fault.

MPICH's no-pack path explicitly calls:

```c
MPIDI_OFI_REQUEST_CREATE(*sigreq, MPIR_REQUEST_KIND__RMA, vci);
```

before initializing completion chunks.  The packed put and get paths omit the
equivalent request creation.  The omission is present in MPICH 5.0.1 and was
also present in the MPICH `main` branch when this report was prepared.

The crash occurs synchronously inside the reproducer's only `MPI_Rput` call,
so a PMPI tracing interposer is not required to establish the MPI call
sequence.

## Related issues

This does not appear to be
[MPICH issue 7886](https://github.com/pmodels/mpich/issues/7886).  That issue is
a CH4/UCX active-message header overflow involving a large irregular
accumulate datatype.  This reproducer:

- uses CH4/OFI;
- calls `MPI_Rput` rather than an accumulate operation;
- needs only two small indexed blocks; and
- terminates with a NULL-request segmentation fault rather than a UCX header
  size error.
