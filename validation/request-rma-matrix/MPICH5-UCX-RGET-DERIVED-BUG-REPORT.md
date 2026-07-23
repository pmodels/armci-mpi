# CH4/UCX `MPI_Rget` with a noncontiguous datatype returns an already-completed request without data

## Summary

MPICH 5.0.1 configured with `ch4:ucx` returns successfully from `MPI_Wait`
on an `MPI_Rget` that uses a vector datatype, but the result buffer is
unchanged. A noncontiguous datatype in either the origin/result or target
argument is sufficient to reproduce the failure.

The failure occurs between two nodes over both native InfiniBand and UCX TCP.
The equivalent non-request `MPI_Get` passes, predefined-datatype `MPI_Rget`
passes, and the same vector `MPI_Rget` passes with CH4/OFI.

## Root cause

The problem is in `src/mpid/ch4/netmod/ucx/ucx_rma.h`.
`MPIDI_NM_mpi_rget` supplies `&sreq` to `MPIDI_UCX_do_get`. When both
datatypes are contiguous, `MPIDI_UCX_do_get` passes that request pointer to
`MPIDI_UCX_contig_get`, which correctly creates a request for an incomplete
UCX operation.

When either datatype is noncontiguous, however, `MPIDI_UCX_do_get` executes:

```c
mpi_errno = MPIDIG_mpi_get(origin_addr, origin_count, origin_datatype,
                           target_rank, target_disp, target_count,
                           target_datatype, win);
```

This starts the non-request active-message get and does not set `sreq`.
`MPIDI_NM_mpi_rget` then interprets `sreq == NULL` as immediate completion
and creates a completed request:

```c
if (sreq == NULL) {
    sreq = MPIR_Request_create_complete(MPIR_REQUEST_KIND__RMA);
}
*request = sreq;
```

Consequently, `MPI_Wait` returns before the active-message get has delivered
and unpacked its data. This code path is unchanged in MPICH `main` as
inspected on 2026-07-23.

The likely correction is for the noncontiguous branch to call
`MPIDIG_mpi_rget(..., reqptr)`, just as the UCX netmod's unreachable-target
fallback already does, rather than calling `MPIDIG_mpi_get`.

## Reproducer

Compile with:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    mpich-ucx-rget-vector.c -o mpich-ucx-rget-vector
mpiexec -n 2 ./mpich-ucx-rget-vector
```

Place one process on each of two nodes. The program contains no
machine-specific configuration:

```c
#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

#ifndef USE_GET
#define USE_GET 0
#endif

#ifndef ORIGIN_VECTOR
#define ORIGIN_VECTOR 1
#endif

#ifndef TARGET_VECTOR
#define TARGET_VECTOR 1
#endif

enum {
    elements = 64,
    blocks = 16,
    block_length = 2,
    stride = 4
};

static void check_mpi(int error, const char *operation)
{
    if (error != MPI_SUCCESS) {
        char text[MPI_MAX_ERROR_STRING];
        int length = 0;

        MPI_Error_string(error, text, &length);
        fprintf(stderr, "%s failed: %.*s\n", operation, length, text);
        MPI_Abort(MPI_COMM_WORLD, error);
    }
}

#if ORIGIN_VECTOR || TARGET_VECTOR
static MPI_Datatype make_vector(void)
{
    MPI_Datatype datatype;

    check_mpi(
        MPI_Type_vector(blocks, block_length, stride, MPI_DOUBLE, &datatype),
        "MPI_Type_vector");
    check_mpi(MPI_Type_commit(&datatype), "MPI_Type_commit");
    return datatype;
}
#endif

int main(int argc, char **argv)
{
    check_mpi(MPI_Init(&argc, &argv), "MPI_Init");

    int rank;
    int size;
    check_mpi(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    check_mpi(MPI_Comm_size(MPI_COMM_WORLD, &size), "MPI_Comm_size");
    if (size != 2) {
        if (rank == 0) {
            fprintf(stderr, "run with exactly two MPI processes\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    double *window_buffer = NULL;
    MPI_Win window;
    check_mpi(
        MPI_Win_allocate(
            elements * (MPI_Aint)sizeof(double),
            sizeof(double),
            MPI_INFO_NULL,
            MPI_COMM_WORLD,
            &window_buffer,
            &window),
        "MPI_Win_allocate");
    check_mpi(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");

    double source[elements];
    double result[elements];
    for (int i = 0; i < elements; ++i) {
        source[i] = i;
        result[i] = -1.0;
    }

    if (rank == 0) {
        check_mpi(
            MPI_Put(
                source,
                elements,
                MPI_DOUBLE,
                1,
                0,
                elements,
                MPI_DOUBLE,
                window),
            "MPI_Put");
        check_mpi(MPI_Win_flush(1, window), "MPI_Win_flush");
    }
    check_mpi(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");

    int errors = 0;
    if (rank == 0) {
#if ORIGIN_VECTOR
        MPI_Datatype origin_type = make_vector();
        const int origin_count = 1;
#else
        MPI_Datatype origin_type = MPI_DOUBLE;
        const int origin_count = blocks * block_length;
#endif
#if TARGET_VECTOR
        MPI_Datatype target_type = make_vector();
        const int target_count = 1;
#else
        MPI_Datatype target_type = MPI_DOUBLE;
        const int target_count = blocks * block_length;
#endif

#if USE_GET
        check_mpi(
            MPI_Get(
                result,
                origin_count,
                origin_type,
                1,
                0,
                target_count,
                target_type,
                window),
            "MPI_Get");
        check_mpi(MPI_Win_flush(1, window), "MPI_Win_flush");
#else
        MPI_Request request;
        check_mpi(
            MPI_Rget(
                result,
                origin_count,
                origin_type,
                1,
                0,
                target_count,
                target_type,
                window,
                &request),
            "MPI_Rget");
        check_mpi(MPI_Wait(&request, MPI_STATUS_IGNORE), "MPI_Wait");
#endif

#if ORIGIN_VECTOR
        check_mpi(MPI_Type_free(&origin_type), "MPI_Type_free(origin)");
#endif
#if TARGET_VECTOR
        check_mpi(MPI_Type_free(&target_type), "MPI_Type_free(target)");
#endif

        for (int block = 0; block < blocks; ++block) {
            for (int element = 0; element < block_length; ++element) {
#if ORIGIN_VECTOR
                const int result_index = block * stride + element;
#else
                const int result_index = block * block_length + element;
#endif
#if TARGET_VECTOR
                const int expected_index = block * stride + element;
#else
                const int expected_index = block * block_length + element;
#endif
                if (result[result_index] != source[expected_index]) {
                    if (errors < 8) {
                        fprintf(
                            stderr,
                            "index=%d expected=%.1f actual=%.1f\n",
                            result_index,
                            source[expected_index],
                            result[result_index]);
                    }
                    ++errors;
                }
            }
        }
    }

    check_mpi(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    check_mpi(MPI_Win_free(&window), "MPI_Win_free");

    if (rank == 0) {
        printf(
            "MPI_%s with %s/%s datatypes: %s\n",
            USE_GET ? "Get" : "Rget",
            ORIGIN_VECTOR ? "vector" : "MPI_DOUBLE",
            TARGET_VECTOR ? "vector" : "MPI_DOUBLE",
            errors == 0 ? "PASS" : "FAIL");
    }

    check_mpi(MPI_Finalize(), "MPI_Finalize");
    return errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

## Observed result

With MPICH 5.0.1 CH4/UCX, `MPI_Wait` reports success but the selected result
elements remain `-1.0`:

```text
index=0 expected=0.0 actual=-1.0
index=1 expected=1.0 actual=-1.0
index=4 expected=4.0 actual=-1.0
index=5 expected=5.0 actual=-1.0
index=8 expected=8.0 actual=-1.0
index=9 expected=9.0 actual=-1.0
index=12 expected=12.0 actual=-1.0
index=13 expected=13.0 actual=-1.0
MPI_Rget with vector/vector datatypes: FAIL
```

## Controls

| Device/operation/datatypes | Result |
| --- | --- |
| CH4/UCX, UCX InfiniBand, `MPI_Rget`, vector/vector | FAIL |
| CH4/UCX, UCX TCP, `MPI_Rget`, vector/vector | FAIL |
| CH4/UCX, UCX InfiniBand, `MPI_Get`, vector/vector | PASS |
| CH4/UCX, UCX InfiniBand, `MPI_Rget`, `MPI_DOUBLE`/`MPI_DOUBLE` | PASS |
| CH4/UCX, UCX InfiniBand, `MPI_Rget`, vector/`MPI_DOUBLE` | FAIL |
| CH4/UCX, UCX InfiniBand, `MPI_Rget`, `MPI_DOUBLE`/vector | FAIL |
| CH4/OFI, OFI verbs/RxM, `MPI_Rget`, vector/vector | PASS |

The datatype objects are retained until after `MPI_Wait`. A separate control
that frees them before waiting fails identically, so datatype-object lifetime
is not the cause.

## Expected result

Completion of the request returned by `MPI_Rget` must indicate local
completion. After `MPI_Wait`, the origin/result buffer must contain the data
selected by the remote target datatype. The reproducer should print:

```text
MPI_Rget with vector/vector datatypes: PASS
```

## Relationship to existing reports

This is not [MPICH issue 7886](https://github.com/pmodels/mpich/issues/7886).
That report concerns a large irregular `MPI_Accumulate` whose CH4
active-message header exceeds a UCX limit. This reproducer uses a small
`MPI_Rget`; its request is completed too early because the UCX netmod selects
the non-request active-message fallback.

Searches of the MPICH issue tracker for `MPI_Rget`, CH4/UCX, and derived or
noncontiguous datatypes did not find an existing report for this defect.

## Downstream impact and workaround

ARMCI-MPI's direct strided nonblocking-get path maps to `MPI_Rget` with
derived datatypes. This causes explicit data mismatches in `armci-test`.
Using non-request `MPI_Get`, converting strided transfers to contiguous IOV
operations, or selecting ARMCI's atomic RMA path avoids this specific MPICH
defect.
