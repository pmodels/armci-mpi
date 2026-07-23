# Open MPI osc/rdma over OFI crashes after MPI_Win_allocate with multiple local ranks

## Summary

Open MPI using `osc/rdma` over the OFI BTL segfaults in a pure-MPI program
that allocates one `double` per rank with `MPI_Win_allocate` and has every
rank issue one scalar `MPI_Accumulate` to rank zero.  Replacing only
`MPI_Win_allocate` with `MPI_Alloc_mem` plus `MPI_Win_create` makes the
program pass.  The failure requires multiple ranks sharing a node: one rank
per node passes, while two ranks per node are sufficient to reproduce.

The reproducer contains no derived datatype, nonstandard info hint, ARMCI
dependency, or request-based RMA operation.  It reproduces in approximately
three seconds.

The behavior is systemic across three unrelated libfabric providers:

| OFI provider | `MPI_Win_allocate` | `MPI_Win_create` |
|---|---|---|
| verbs/RxM over InfiniBand | FAIL | PASS |
| PSM3 over InfiniBand | FAIL | PASS |
| TCP over IPoIB | FAIL | PASS |

The verbs/RxM and TCP cases segfault while `MPI_Accumulate` is driving Open
MPI progress.  PSM3 reports an OFI completion error and aborts.  The identical
window-allocation split across providers points to Open MPI's
`osc/rdma`/window-allocation integration rather than a provider-specific
libfabric defect.

## Versions and topology

- Open MPI current `main`: `v6.1.0a1`, repository revision
  `afdec2e5abd11f527ded01e5f0205f4cd3636f70`
- Earlier affected Open MPI build: `v6.0.0a1`, repository revision `7ff7d28`
- Compiler: GCC 11.5
- Reproducing topology: two x86-64 nodes, at least two MPI ranks per node
- InfiniBand providers: verbs/RxM and PSM3
- TCP provider: bound to an IPoIB interface

Open MPI was configured with OFI support and launched with:

```sh
export OMPI_MCA_osc=rdma
export OMPI_MCA_osc_rdma_btls=ofi
```

Provider selection used one of:

```sh
export FI_PROVIDER='verbs;ofi_rxm'
export FI_PROVIDER=psm3
export FI_PROVIDER=tcp
```

The verbs and TCP cases were explicitly bound to the intended InfiniBand
device or IPoIB interface.

The pure-MPI reproducer and its `MPI_Win_create` control were run against
both listed Open MPI revisions.  Both revisions produce the same
`MPI_Win_allocate` failure and passing `MPI_Win_create` control.

## Reproducer

Save the following as `ompi-ofi-win-allocate.c`:

```c
#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void mpi_check(int error, const char *operation)
{
    if (error != MPI_SUCCESS) {
        char message[MPI_MAX_ERROR_STRING];
        int length = 0;

        MPI_Error_string(error, message, &length);
        fprintf(stderr, "%s failed: %.*s\n", operation, length, message);
        MPI_Abort(MPI_COMM_WORLD, error);
    }
}

int main(int argc, char **argv)
{
    mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    int rank = -1;
    int nranks = 0;

    mpi_check(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    mpi_check(MPI_Comm_size(MPI_COMM_WORLD, &nranks), "MPI_Comm_size");

    if (argc != 2 ||
        (strcmp(argv[1], "allocate") != 0 &&
         strcmp(argv[1], "create") != 0)) {
        if (rank == 0) {
            fprintf(stderr, "usage: %s {allocate|create}\n", argv[0]);
        }
        MPI_Finalize();
        return 2;
    }

    const int use_allocate = strcmp(argv[1], "allocate") == 0;
    const MPI_Aint bytes = sizeof(double);
    MPI_Win window = MPI_WIN_NULL;
    double *target = NULL;

    if (use_allocate) {
        mpi_check(
            MPI_Win_allocate(
                bytes,
                1,
                MPI_INFO_NULL,
                MPI_COMM_WORLD,
                &target,
                &window),
            "MPI_Win_allocate");
    } else {
        mpi_check(
            MPI_Alloc_mem(bytes, MPI_INFO_NULL, &target),
            "MPI_Alloc_mem");
        mpi_check(
            MPI_Win_create(
                target,
                bytes,
                1,
                MPI_INFO_NULL,
                MPI_COMM_WORLD,
                &window),
            "MPI_Win_create");
    }

    mpi_check(
        MPI_Win_lock_all(MPI_MODE_NOCHECK, window),
        "MPI_Win_lock_all");
    *target = 0.0;
    mpi_check(MPI_Win_sync(window), "MPI_Win_sync");
    mpi_check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");

    const double source = 6.0 * (rank + 1);

    mpi_check(
        MPI_Accumulate(
            &source,
            1,
            MPI_DOUBLE,
            0,
            0,
            1,
            MPI_DOUBLE,
            MPI_SUM,
            window),
        "MPI_Accumulate");
    mpi_check(MPI_Win_flush_all(window), "MPI_Win_flush_all");
    mpi_check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");
    mpi_check(MPI_Win_sync(window), "MPI_Win_sync");

    int errors = 0;

    if (rank == 0) {
        const double rank_sum = nranks * (nranks + 1) / 2.0;
        const double expected = 6.0 * rank_sum;

        if (*target != expected) {
            fprintf(
                stderr,
                "target = %.17g, expected %.17g\n",
                *target,
                expected);
            ++errors;
        }
    }

    mpi_check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    mpi_check(MPI_Win_free(&window), "MPI_Win_free");
    if (!use_allocate) {
        mpi_check(MPI_Free_mem(target), "MPI_Free_mem");
    }

    int total_errors = 0;

    mpi_check(
        MPI_Allreduce(
            &errors,
            &total_errors,
            1,
            MPI_INT,
            MPI_SUM,
            MPI_COMM_WORLD),
        "MPI_Allreduce");
    if (rank == 0) {
        printf(
            "window=%s: %s\n",
            argv[1],
            total_errors == 0 ? "PASS" : "FAIL");
    }

    mpi_check(MPI_Finalize(), "MPI_Finalize");
    return total_errors == 0 ? 0 : 1;
}
```

Compile it with the affected Open MPI:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    ompi-ofi-win-allocate.c -o ompi-ofi-win-allocate
```

Select `osc/rdma` and the OFI BTL, then run eight ranks with four ranks on
each of two nodes:

```sh
export OMPI_MCA_osc=rdma
export OMPI_MCA_osc_rdma_btls=ofi
export FI_PROVIDER='verbs;ofi_rxm'

mpirun -n 8 --map-by ppr:4:node \
    ./ompi-ofi-win-allocate allocate
```

The `allocate` case receives SIGSEGV.  The adjacent control changes only the
window construction and passes:

```sh
mpirun -n 8 --map-by ppr:4:node \
    ./ompi-ofi-win-allocate create
```

The observed topology boundary with verbs/RxM is:

| Layout | `MPI_Win_allocate` | `MPI_Win_create` |
|---|---|---|
| two ranks, one per node | PASS | PASS |
| four ranks, two per node | SIGSEGV | PASS |
| eight ranks, four per node | SIGSEGV | PASS |

## What the tests exercise

The pure-MPI reproducer has each rank issue exactly one scalar
`MPI_Accumulate` of one predefined `MPI_DOUBLE` to rank zero during a
lock-all epoch.  It flushes the window, verifies the exact sum, and uses
`MPI_Win_sync` so the verification is valid for either MPI window memory
model.

The original ARMCI-MPI discovery tests used overlapping contiguous, strided,
and vector operations over every supported accumulate element type.  Those
details are not required: the single-scalar pure-MPI program preserves the
same `MPI_Win_allocate` failure predicate.

## Complete configuration split

Thirteen ARMCI tuples varied:

- window allocation;
- direct versus IOV strided transfers;
- direct versus batched vector transfers;
- RMA atomicity;
- request-based atomics;
- request-atomic flushing; and
- accumulate granularity.

For each of the three OFI providers, all ten `MPI_Win_allocate` tuples have
the same result:

| Test | Result |
|---|---|
| two-rank `armci-test` | PASS |
| eight-rank overlapping accumulate | FAIL |
| eight-rank mixed location consistency | FAIL |

All three `MPI_Win_create` tuples pass all three tests.  Neither datatype
method, atomicity, request atomics, request flushing, nor accumulate
granularity changes the predicate.

## Failure signatures

### verbs/RxM and TCP

Both providers segfault in the same Open MPI path:

```text
Signal: Segmentation fault (11)
Signal code: Address not mapped

mca_btl_sm_poll_handle_frag
opal_progress
ompi_request_default_wait_all
ompi_coll_base_barrier_intra_basic_linear
PMPI_Barrier
ompi-ofi-win-allocate
```

The pure reproducer's `MPI_Accumulate` has already initiated the failing
activity; the crash is observed when subsequent synchronization drives Open
MPI progress.  A less-reduced version also crashed directly in
`ompi_osc_rdma_accumulate`.  In both cases the failure address is in an
unmapped region.  Although the selected OSC data path is OFI, the crash occurs
while `opal_progress` polls the node-local `sm` BTL.  This is consistent with
stale or invalid `MPI_Win_allocate` shared-memory state when several ranks
share a node.

### PSM3

PSM3 aborts from OFI progress instead of segfaulting:

```text
mca_btl_ofi_context_progress:
    fi_cq_readerr: Permission denied (13) (provider err_code = 8)

mca_btl_ofi_exit:
    BTL OFI will now abort
```

Despite the different surface error, the pure-MPI program follows the same
exact `MPI_Win_allocate` failure predicate and `MPI_Win_create` control.

## Expected behavior

`MPI_Win_allocate` and `MPI_Win_create` may use different storage and
registration mechanisms, but both must support the same legal passive-target
RMA operations.  Multiple local ranks and concurrent accumulate operations
must not expose unmapped shared-memory fragments or invalid OFI completions.

## Distinction from existing Open MPI reports

This is not the indexed-accumulate corruption in Open MPI issue 14181, the
Open MPI 4/5 UCX derived-accumulate crash in issue 14175, or the UCX
Get-accumulate swap hang in issue 14173:

- both Open MPI 6.0 development and current 6.1 development builds are
  affected;
- the selected OSC is `rdma`, not `ucx`;
- all three OFI providers reproduce;
- the reproducer is pure MPI and needs only one predefined-type accumulate;
- the decisive switch is `MPI_Win_allocate` versus `MPI_Win_create`; and
- verbs/RxM and TCP crash in the node-local `sm` BTL progress path.

A search of the current Open MPI issue tracker did not find an existing report
with this window-allocation, `osc/rdma`, OFI, and multi-rank-per-node
combination.

## Suggested investigation

1. Reproduce with four ranks per node and compare with one rank per node.
2. Disable the `sm` BTL while retaining OFI for OSC to determine whether the
   verbs/TCP segfault moves or disappears.
3. Trace allocation, registration, and release of the storage returned by
   `MPI_Win_allocate`, especially any node-shared backing mapping used by
   `osc/rdma`.
4. Check whether `osc/rdma` completion can reference a fragment after the
   corresponding `sm` mapping or registration was replaced or unmapped.
5. Run a debug build under AddressSanitizer or preserve the first-rank core;
   the unmapped fault address should make stale mapping ownership visible.

## Workaround

Use application-managed storage with `MPI_Win_create`:

```sh
export ARMCI_USE_WIN_ALLOCATE=0
```

This passed every focused test and every tested OFI provider.  It is a
correctness workaround and may have a performance cost relative to
`MPI_Win_allocate`.
