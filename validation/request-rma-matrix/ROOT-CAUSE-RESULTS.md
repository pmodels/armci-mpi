# Request-RMA matrix root-cause results

## Scope

These results investigate matrix failures not explained by the following known
issues:

- [Open MPI 14181](https://github.com/open-mpi/ompi/issues/14181): Open MPI 5
  UCX/IB loses updates for `MPI_Accumulate` with a one-element indexed type;
- [Open MPI 14175](https://github.com/open-mpi/ompi/issues/14175): Open MPI 4
  and 5 `osc/ucx` can segfault in short-message derived-datatype
  `MPI_Accumulate`;
- [Open MPI 14173](https://github.com/open-mpi/ompi/issues/14173): Open MPI 4
  UCX/IB hangs in a contended `MPI_Get_accumulate` or
  `MPI_Rget_accumulate(MPI_REPLACE)` swap; and
- [MPICH 7886](https://github.com/pmodels/mpich/issues/7886): CH4/UCX exceeds
  the UCX active-message header limit for a large irregular accumulate type.

All ARMCI runs used `ARMCI_VERBOSE=2`.  TCP was bound to IPoIB interface
`ib0_mlx5`, not a native Ethernet interface.  Every diagnostic used two nodes
with one MPI rank per node unless a retained matrix runner explicitly recorded
otherwise.

The complete diagnostic logs are under:

```text
/global/home/users/jehammond/Claude/armci-val/request-rma-rootcause-90721f6
```

## Summary

| Signature | Classification | Required mitigation |
| --- | --- | --- |
| Open MPI 5/UCX, `STRIDED=IOV`, non-atomic request puts | New Open MPI/UCX request-RMA defect: the 255th outstanding `MPI_Rput` aborts at UCX's 8-bit endpoint reference limit over IB and IPoIB | Do not permit 255 outstanding Open MPI UCX request puts to one endpoint; force `STRIDED=DIRECT` or complete request batches below the limit |
| MPICH 5/OFI, `VECTOR=DIRECT`, `ATOMICITY=0` | New MPICH CH4/OFI packed-`MPI_Rput` NULL-request dereference | Force `VECTOR=BATCHED` or `ATOMICITY=1` until MPICH is fixed |
| MPICH 5/UCX, noncontiguous request gets | New MPICH CH4/UCX `MPI_Rget` completion bug: the noncontiguous fallback starts non-request `MPIDIG_mpi_get` and returns a completed request | Use non-request `MPI_Get`, `STRIDED=IOV`, or `RMA_ATOMICITY=1` until MPICH is fixed |
| Any request-atomic configuration with `FLUSH_REQUEST_ATOMICS=0` | ARMCI completion-semantics defect; `MPI_Wait` does not provide target completion | Force `FLUSH_REQUEST_ATOMICS=1` when request atomics are enabled |
| Open MPI 4/UCX IPoIB timeouts | Slow and variable, but the isolated test completes; the whole-suite witness still exceeded five minutes | Keep these tuples failed; no additional stuck MPI call was localized |
| Open MPI 5/UCX IPoIB `rmw_perf` timeout | Extreme atomic latency, not a hang | Treat the original bound as a test failure; no correctness workaround is indicated |
| Open MPI 5/OFI IB timeouts | Tests show forward progress but are extremely slow; the preferred whole-suite witness exceeded five minutes | Keep all original timeout tuples failed |

## Open MPI 5 UCX outstanding-`MPI_Rput` failure

`mcve/rput-many.c` issues scalar `MPI_Rput` operations to one peer without
completing the requests between calls.  The result is deterministic on both
Iris and Thor over native IB and TCP over IPoIB:

| Outstanding requests | Result |
| ---: | --- |
| 250 | pass |
| 254 | pass |
| 255 | abort in UCX |
| 256 | abort in UCX |
| 300 | abort in UCX |

The failing assertion and call path are:

```text
flush.c:574: Assertion `(next_ep_ext->ep)->refcount < UINT8_MAX' failed
ucp_worker_flush_nbx
ucp_worker_flush_nb
opal_common_ucx_wpmem_flush_ep_nb
ompi_osc_ucx_rput
PMPI_Rput
```

Open MPI's `ompi_osc_ucx_rput` implements request completion by calling
`opal_common_ucx_wpmem_flush_ep_nb`.  Despite its name, that function calls
`ucp_worker_flush_nb`.  Each unfinished worker flush takes another reference
on the same UCP endpoint.  UCX stores the endpoint reference count in eight
bits.  With the endpoint's existing reference, 254 flush requests fill the
counter and the 255th request aborts before incrementing it.

Representative logs are jobs 621108, 621109, 621136 on Iris and jobs 621122,
621123, 621147 on Thor.  IPoIB controls are jobs 621229 through 621232.  The
Open MPI 6 source snapshot installed in the MPI release tree retains the same
request-flush implementation, so this should be reported upstream rather than
treated as a 5.0.10-only application failure.

This is unrelated to Open MPI issues 14181, 14175, and 14173: the operation is
`MPI_Rput`, the datatype can be predefined and contiguous, and the failure is
an immediate UCX assertion at a precise outstanding-request count.

## MPICH CH4/OFI packed-`MPI_Rput` failure

Per-rank GDB runs of `armci-test` on OFI verbs/RXM and OFI TCP fault at:

```text
issue_packed_put, ofi_rma.c:224
MPIDI_OFI_INIT_CHUNK_CONTEXT(win, sigreq)
MPIDI_OFI_pack_put
MPIDI_OFI_do_put
MPIDI_NM_mpi_rput
PMPI_Rput
gmr_put_typed
ARMCII_Iov_op_datatype
ARMCI_NbPutV
test_vec_small
```

`MPIDI_OFI_pack_put` records the non-NULL address of the caller's request
pointer, but it never constructs the `MPIR_Request` to which that pointer must
refer.  `issue_packed_put` then invokes `MPIDI_OFI_INIT_CHUNK_CONTEXT`; the
macro executes `MPIR_cc_inc((*sigreq)->cc_ptr)` and dereferences the NULL
`*sigreq`.  The working no-pack path explicitly calls
`MPIDI_OFI_REQUEST_CREATE(*sigreq, ...)` first.  The packed path has no
equivalent call in MPICH 5.0.1 or in MPICH `main` inspected on 2026-07-20.

The standalone `mcve/rput-indexed.c` reproduces with one request, two origin
blocks of 20 doubles separated by one double, and two adjacent target blocks.
The adjacent target indexed datatype collapses to a contiguous target while
the sparse origin selects the packing path.  One block passes; two blocks
crash.  The source and target buffers are single valid allocations, and the
crash occurs inside `MPI_Rput` before datatype release, eliminating datatype
lifetime and disjoint C allocations as explanations.

The operation controls on Iris and Thor, over both OFI verbs/RXM and OFI TCP,
are:

| Operation | Result |
| --- | --- |
| `MPI_Put` | pass and verify |
| `MPI_Rput` | signal 11 in `issue_packed_put` |
| `MPI_Raccumulate(MPI_REPLACE)` | pass and verify |

The same `MPI_Rput` program passes over CH4/UCX IB and CH4/UCX TCP.  This is
not MPICH issue 7886: it is in CH4/OFI, uses `MPI_Rput`, has no active-message
header error, and needs only two small blocks.

The GDB evidence is in jobs 621158, 621159, 621164, and 621165.  The minimal
block-count controls are jobs 621220 through 621222.  Operation and transport
controls are jobs 621208 through 621219.

## Missing target completion for request atomics

`gmr_fetch_and_op` implements a blocking ARMCI swap with
`MPI_Rget_accumulate`, followed by `MPI_Wait`.  It calls `MPI_Win_flush` only
when `ARMCI_FLUSH_REQUEST_ATOMICS=1`.  Request completion makes the origin and
result buffers available but does not establish completion at the target.
Consequently, returning from blocking `ARMCI_Rmw` with the flush disabled does
not guarantee that the replacement is visible remotely.  The contended lock
test can spin forever after a release remains incomplete.

This signature is intermittent, which explains isolated rather than
predicate-wide matrix failures.  Job 621202 reaches `Testing atomic swap` and
times out after 180 seconds with request atomics enabled and flushing disabled.
The otherwise identical flush-enabled controls, jobs 621223 and 621224,
complete `armci-test` in 43--45 seconds on Iris and Thor.  A small standalone
swap often completes even without the flush, which is expected for a timing-
sensitive missing-completion bug and is not evidence that the optimization is
valid.

The mitigation should be unconditional: enabling request atomics must also
enable target flushes.  This should not be filed as an MPI implementation bug.

## Timeouts shown to be performance-bound

The preferred Open MPI configurations were also rerun with
`OMPI_MCA_btl_ofi_progress_interval=100`:

| MPI/backend/network | Iris | Thor | Interpretation |
| --- | --- | --- | --- |
| Open MPI 4/UCX/IPoIB | 43/43 in 228 seconds | 43/43 in 292 seconds | Passing tuple, although barely inside the five-minute bound on Thor |
| Open MPI 5/UCX/IPoIB | 42/43 in 266 seconds | 42/43 in 285 seconds | `benchmarks/rmw_perf` remains the sole failure |
| Open MPI 5/OFI/IB | timeout at 300 seconds | timeout at 300 seconds | No test-suite summary was produced before the bound |

The UCX/IPoIB launcher selects `pml=ob1`, `btl=self,tcp`, and `osc=ucx`, so an
OFI-BTL progress interval is not expected to affect its RMA path.  The Open
MPI 4 passes should therefore be treated as successful reruns, not proof that
this parameter fixed UCX.  The parameter does exercise the Open MPI 5 OFI
configuration, where it did not bring the preferred tuple inside the required
bound.  Logs are under `ompi-ucx-ipoib-ofi-progress-100` in the external
validation-results tree.

The Open MPI 4/UCX IPoIB `STRIDED=IOV` `armci-test` controls complete in 64
seconds on Iris and 70 seconds on Thor (jobs 621160 and 621166).  The prior
matrix timeouts therefore do not identify a permanent stall in that test.  A
whole-suite witness with a relaxed per-launch limit still exceeded five
minutes on both machines and was cancelled (jobs 621227 and 621228).  The
tuples remain failures under the project's required runtime bound.

For Open MPI 5/UCX IPoIB, `rmw_perf` completes 10,000 and 100,000 operations
and reports approximately 141--180 microseconds per fetch-and-add.  One million
operations completes in 135 seconds on Iris and 147 seconds on Thor when given
a 300-second bound (jobs 621191 and 621192).  The original 110/120-second
timeouts are explained by measured throughput rather than a hung atomic call.

Open MPI 5/OFI IB benchmark logs likewise print continued progress through
increasing message sizes.  In the preferred tuple, the only original failures
are `benchmarks/contiguous-bench` and `benchmarks/strided-bench`.  Relaxed
whole-suite witnesses still exceeded six minutes on Iris and Thor and were
cancelled (jobs 621225 and 621226).  This is sufficient to reject the tuples
under the required five-minute bound, while the retained benchmark output
supports severe performance as the observed cause rather than a demonstrated
stuck MPI call.

## Code-state warning

Commit `13aa409` was added concurrently while these diagnostics were running.
Its Open MPI 4 atomicity warning tests
`ARMCII_GLOBAL_STATE.rma_atomicty`, which misspells `rma_atomicity` and prevents
that configuration from compiling.  The diagnostic commits deliberately do
not modify the user-authored mitigation.
