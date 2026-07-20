# Deferred MPICH 5/OFI `MPI_Rput` MCVE plan

## Status and scheduling gate

This investigation is deliberately deferred. Do not create the MCVE, run
additional reproductions, modify ARMCI-MPI to diagnose it, or file an MPICH
issue until the ARMCI-MPI request-RMA test-matrix project is complete and its
results have been accepted. This document exists to preserve the evidence and
the next steps without diverting the current validation effort.

## Observed failure

The ARMCI-MPI pull-request tree at source commit
`dba36888eecf9ea98e44bc0ff4145ee8c6c3fc9c` consistently completes 42 of the
43 `make check` tests with MPICH 5.0.1 CH4/OFI when all of the following are
true:

```text
ARMCI_IOV_METHOD=DIRECT
ARMCI_RMA_ATOMICITY=0
ARMCI_USE_REQUEST_ATOMICS=1
```

The sole failure is `tests/contrib/armci-test`. It exits on signal 11 while
entering the `test_vec_small` phase:

```text
Testing non-blocking vector gets and puts

BAD TERMINATION OF ONE OF YOUR APPLICATION PROCESSES
EXIT CODE: 11
YOUR APPLICATION TERMINATED WITH THE EXIT STRING: Segmentation fault (signal 11)
```

The crash occurs before the test reports that the nonblocking vector PUT data
are being verified. The precise instruction and call stack have not yet been
collected because debugging is outside the current matrix scope.

The failure reproduces on both Iris and Thor, with both the OFI verbs/RXM
InfiniBand path and the OFI TCP path over `ib0_mlx5`. It is independent of:

- `ARMCI_USE_WIN_ALLOCATE=0` or `1`;
- `ARMCI_STRIDED_METHOD=DIRECT` or `IOV`; and
- `ARMCI_FLUSH_REQUEST_ATOMICS=0` or `1`.

The regular pattern accounts for eight failing tuples in each platform/network
cell: two allocation modes times two strided methods times two flush modes.
Thor OFI/TCP also recorded two isolated failures outside this regular pattern;
those should be retested separately and must not be conflated with the direct
vector failure until they are shown to be reproducible.

The following changes avoid the regular failure:

- `ARMCI_RMA_ATOMICITY=1`, which changes the nonblocking PUT implementation
  from `MPI_Rput` to `MPI_Raccumulate(..., MPI_REPLACE, ...)`; or
- `ARMCI_IOV_METHOD=BATCHED`, which avoids the single direct derived-datatype
  operation.

MPICH 5 CH4/UCX passes the same direct, non-atomic request configuration on IB
and TCP. The evidence therefore points to the CH4/OFI request path rather than
to allocation mode, the strided implementation, flush behavior, or a generic
ARMCI vector error.

## ARMCI-MPI call path

`test_vec_small` constructs several `armci_giov_t` descriptions and calls
`ARMCI_NbPutV` with a user handle. The relevant path is:

```text
ARMCI_NbPutV
  -> PARMCI_NbPutV
  -> ARMCII_Iov_op_dispatch
  -> ARMCII_Iov_op_datatype
  -> gmr_put_typed
  -> MPI_Rput
```

`ARMCII_Iov_op_datatype` creates and commits indexed-block origin and target
datatypes relative to real base pointers. With request RMA enabled and a
non-null ARMCI handle, `gmr_put_typed` executes the equivalent of:

```c
MPI_Rput(origin_base, 1, origin_indexed_type,
         target_rank, target_displacement, 1, target_indexed_type,
         window, &request);
```

ARMCI-MPI adds the returned request to the user handle and frees the datatype
handles after initiating the operation. MPI datatype lifetime rules should
permit `MPI_Type_free` after an operation has retained the datatype, but the
MCVE must explicitly test whether moving each free after `MPI_Wait` changes the
CH4/OFI behavior. That experiment will distinguish a likely implementation
lifetime defect from an invalid assumption in the application.

The current log only localizes the crash to the nonblocking vector phase. It
does not yet prove whether the fault occurs in `MPI_Rput`, datatype cleanup,
request aggregation, `MPI_Wait`, or progress made by a subsequent call.

## Distinction from MPICH issue 7886

This does not match
[pmodels/mpich#7886](https://github.com/pmodels/mpich/issues/7886).

Issue 7886 concerns CH4/UCX on the `rc_mlx5` InfiniBand transport. A large,
irregular datatype used by `MPI_Accumulate` exceeds the UCX active-message
header limit and produces an explicit header-length error. The present failure:

- uses CH4/OFI and reproduces over both verbs/RXM and TCP;
- involves the nonblocking direct vector PUT path and therefore `MPI_Rput` when
  atomicity is disabled;
- occurs with the much smaller `test_vec_small` indexed datatype; and
- terminates with a segmentation fault rather than a UCX header-size error.

A new MPICH issue is appropriate only after a standalone MPI program reproduces
the fault. The future report should mention issue 7886 as a related
derived-datatype RMA problem and explain why it is not a duplicate.

## Evidence retained from the matrix

The canonical matrix artifacts are outside the repository at:

```text
/global/home/users/jehammond/Claude/armci-val/five-axis-results-fca13e6/combined.tsv
/global/home/users/jehammond/Claude/armci-val/five-axis-results-fca13e6/failures.tsv
/global/home/users/jehammond/Claude/armci-val/five-axis-results-fca13e6/logs/
/global/home/users/jehammond/Claude/armci-val/five-axis-results-fca13e6/slurm/
```

Representative failure logs are:

```text
logs/iris-pr-mpich5-ofi-ib/allocate_1-strided_DIRECT-vector_DIRECT-atomicity_0-flush_1/check.log
logs/iris-pr-mpich5-ofi-ib/allocate_1-strided_DIRECT-vector_DIRECT-atomicity_0-flush_1/test-suite.log
logs/iris-pr-mpich5-ofi-ib/allocate_1-strided_DIRECT-vector_DIRECT-atomicity_0-flush_1/tests/contrib/armci-test.log
slurm/iris-621008-12.out
slurm/thor-621026-12.out
```

All matrix tests used `ARMCI_VERBOSE=2`. The relevant source tree was built in:

```text
/global/home/users/jehammond/Claude/armci-val/request-matrix-builds-dba3688/pr-mpich5-ofi
```

## Deferred MCVE plan

After the matrix project is complete, perform the work in the following order.

### 1. Confirm and localize without changing semantics

1. Re-run only the known failing ideal-performance tuple on two nodes with
   MPICH 5.0.1 CH4/OFI, `ARMCI_VERBOSE=2`, and a five-minute bound.
2. Capture a debugger backtrace or core from every rank and identify whether
   the first fault is inside `MPI_Rput`, OFI progress, datatype release,
   `MPI_Wait`, or ARMCI request-list handling.
3. Repeat the exact run once with CH4/UCX as the known-good control.
4. Preserve commands, MPI configuration, libfabric provider selection,
   placement, backtraces, and complete logs in a dedicated MCVE directory.

### 2. Reduce the ARMCI test before writing standalone MPI code

1. Run only `test_vec_small` and remove all earlier `armci-test` phases.
2. Reduce from all peer ranks to one origin and one target.
3. Reduce `GIOV_ARR_LEN`, `PTR_ARR_LEN`, and element count while retaining the
   crash.
4. Separate NbPutV from NbGetV. Start with NbPutV because the existing output
   suggests failure before PUT verification, but verify this rather than
   assuming it.
5. Reduce from multiple requests per ARMCI handle to one request.

Record the smallest crashing block count and displacement pattern, plus the
first neighboring configuration that succeeds.

### 3. Produce a standalone MPI MCVE

The first standalone program should contain only:

1. `MPI_Init`, rank/size discovery, and two-rank validation;
2. one `MPI_Win_allocate` window;
3. a local source buffer and a remotely addressed target region;
4. one indexed-block origin datatype and one indexed-block target datatype
   matching the reduced ARMCI displacements;
5. `MPI_Win_lock_all`;
6. one `MPI_Rput` and `MPI_Wait`;
7. completion, synchronization, and target-data verification; and
8. datatype, window, and MPI cleanup.

Keep the program deterministic, warning-clean C99, and independent of ARMCI.
It should print `OK`, `WRONG`, or a concise MPI error. Do not include unrelated
ARMCI abstractions or performance measurements.

### 4. Run discriminating experiments

Change one factor at a time from the reproducing MPI program:

| Experiment | Purpose |
| --- | --- |
| `MPI_Rput` versus `MPI_Put` | Isolate request-based RMA behavior. |
| Wait before versus after `MPI_Type_free` | Test datatype-retention lifetime. |
| Indexed origin only | Identify which datatype position triggers the fault. |
| Indexed target only | Identify which datatype position triggers the fault. |
| Indexed versus contiguous predefined types | Prove derived-type dependence. |
| One versus multiple blocks | Find the minimum triggering datatype. |
| Remote rank versus self target | Separate network/progress from datatype handling. |
| `MPI_Win_allocate` versus `MPI_Win_create` | Confirm window allocation independence. |
| CH4/OFI verbs/RXM versus OFI TCP | Confirm provider independence. |
| CH4/UCX IB and TCP | Preserve known-good device controls. |
| `MPI_Raccumulate` with `MPI_REPLACE` | Preserve the working atomicity control. |

Do not add experiments speculatively once a minimal distinction is established.

### 5. Establish whether the defect is in MPICH or ARMCI-MPI

Before filing, verify all of the following:

- the standalone MPI program is valid under MPI datatype, window epoch,
  request-completion, and buffer-lifetime rules;
- every datatype displacement and extent is within the corresponding buffer or
  window;
- datatype handles and user buffers remain valid for the duration required by
  MPI;
- all MPI return codes are checked with `MPI_ERRORS_RETURN` enabled where
  practical;
- the failure reproduces in an unmodified MPICH 5.0.1 installation; and
- at least one control MPI/device completes and verifies the same operation.

If delaying `MPI_Type_free` until after completion fixes the program, consult
the MPI lifetime rules and retain both versions in the report. If the original
program is standard-conforming, this is strong evidence of an MPICH lifetime
bug. If it is not conforming, fix ARMCI-MPI and do not file an MPICH issue.

### 6. Prepare the MPICH report

The eventual report should include:

- a title identifying MPICH 5.0.1, CH4/OFI, `MPI_Rput`, and indexed datatypes;
- the single-file MCVE and exact compile/run commands;
- observed output, signal, and a symbolized backtrace;
- MPICH configure arguments and `mpichversion` output;
- libfabric version and selected providers/devices;
- the IB and TCP reproduction table and UCX control results;
- the minimum block/displacement threshold;
- the datatype-free timing result;
- expected behavior and the relevant MPI lifetime reasoning;
- a link to ARMCI-MPI pull request 53 as discovery context; and
- a note distinguishing the report from issue 7886.

Do not file until the program reproduces without ARMCI-MPI and the report can be
reviewed from saved artifacts alone.

## Completion criterion for the future MCVE task

The deferred task is complete only when either:

1. a valid standalone MPI program reliably reproduces the CH4/OFI fault, all
   required controls have been recorded, and a review-ready MPICH issue draft
   exists; or
2. reduction proves that ARMCI-MPI violates MPI requirements, the application
   defect is identified precisely, and no MPICH issue is warranted.

