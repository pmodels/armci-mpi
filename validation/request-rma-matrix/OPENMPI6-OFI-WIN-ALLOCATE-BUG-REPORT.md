# Open MPI 6 osc/rdma over OFI fails RMA stress tests whenever MPI_Win_allocate is used

## Summary

An Open MPI 6 development build using `osc/rdma` over the OFI BTL fails two
independent eight-rank RMA correctness tests for every tested configuration
that creates windows with `MPI_Win_allocate`.  Replacing only
`MPI_Win_allocate` with application allocation plus `MPI_Win_create` makes
every case pass.

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

- Open MPI: `v6.0.0a1`, repository revision `7ff7d28`
- ARMCI-MPI: `01afc591afaf556753370cdd6b0528f80fc34415`
- Compiler: GCC 11.5
- Topology: two x86-64 nodes, four MPI ranks per node
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

## Reproducer

Build these ARMCI-MPI tests from commit `01afc591`:

```text
tests/test_acc_overlap
tests/test_location_consistency
```

Run either with eight ranks split four per node:

```sh
export ARMCI_VERBOSE=2
export ARMCI_USE_WIN_ALLOCATE=1
export ARMCI_STRIDED_METHOD=DIRECT
export ARMCI_IOV_METHOD=BATCHED
export ARMCI_RMA_ATOMICITY=0
export ARMCI_USE_REQUEST_ATOMICS=0
export ARMCI_FLUSH_REQUEST_ATOMICS=1

mpirun -n 8 --map-by ppr:4:node ./tests/test_acc_overlap
mpirun -n 8 --map-by ppr:4:node ./tests/test_location_consistency
```

Both fail within approximately two or three seconds.

Change only:

```sh
export ARMCI_USE_WIN_ALLOCATE=0
```

ARMCI-MPI then allocates the storage itself and uses `MPI_Win_create`.  Both
tests pass with all three OFI providers.

## What the tests exercise

`test_acc_overlap` has all ranks concurrently issue overlapping:

- contiguous `MPI_Accumulate`;
- strided derived-datatype `MPI_Accumulate`; and
- vector/batched accumulate operations.

It repeats the operation for every ARMCI integer, real, and complex accumulate
type and verifies every target element against an exact oracle.

`test_location_consistency` mixes one contiguous, strided, or vector Put,
three Accumulate variants in randomized order, and one contiguous, strided,
or vector Get.  It intentionally relies on MPI/ARMCI location consistency
without inserting an application flush between those operations.

The older two-rank `armci-test`, with one rank per node, passes all 13
environment tuples.  The failure appears when the focused tests run four
ranks per node, which implicates interaction with the node-local path.

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
<osc/rdma internal progress>
ompi_osc_rdma_accumulate
PMPI_Accumulate
test_acc_overlap
```

The failure address is in an unmapped region.  Although the selected OSC data
path is OFI, the crash occurs while `opal_progress` polls the node-local
`sm` BTL.  This is consistent with stale or prematurely released
`MPI_Win_allocate` shared-memory state when several ranks share a node.

### PSM3

PSM3 aborts from OFI progress instead of segfaulting:

```text
mca_btl_ofi_context_progress:
    fi_cq_readerr: Permission denied (13) (provider err_code = 8)

mca_btl_ofi_exit:
    BTL OFI will now abort
```

Despite the different surface error, it follows the same exact
`MPI_Win_allocate` failure predicate and `MPI_Win_create` control.

## Expected behavior

`MPI_Win_allocate` and `MPI_Win_create` may use different storage and
registration mechanisms, but both must support the same legal passive-target
RMA operations.  Multiple local ranks and concurrent accumulate operations
must not expose unmapped shared-memory fragments or invalid OFI completions.

## Distinction from existing Open MPI reports

This is not the indexed-accumulate corruption in Open MPI issue 14181, the
Open MPI 4/5 UCX derived-accumulate crash in issue 14175, or the UCX
Get-accumulate swap hang in issue 14173:

- the affected build is Open MPI 6;
- the selected OSC is `rdma`, not `ucx`;
- all three OFI providers reproduce;
- both focused tests fail;
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
