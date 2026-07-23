# Request-RMA standalone reproducers

These two-rank MPI programs preserve implementation defects found
by the ARMCI-MPI request-RMA matrix.  They do not depend on ARMCI-MPI.  The
generic `../run-mcve.sbatch` driver compiles a selected source, places one rank
on each of two nodes, selects native InfiniBand or TCP over `ib0_mlx5`, applies
a hard timeout, and retains the complete Slurm output.

The full investigation table, backtraces, job IDs, and ARMCI mitigations are
in [`../ROOT-CAUSE-RESULTS.md`](../ROOT-CAUSE-RESULTS.md).

The issue-submission versions are deliberately fixed-case programs:

| File | Default result | Adjacent working control |
| --- | --- | --- |
| `ompi-ucx-rput-minimal.c` | Open MPI/UCX aborts on request 255 | Compile with `-DREQUEST_COUNT=254` |
| `mpich-ofi-rput-minimal.c` | MPICH CH4/OFI segfaults in `MPI_Rput` | Compile with `-DUSE_NONREQUEST_PUT` |
| `mpich-ucx-rget-vector.c` | MPICH CH4/UCX completes an empty `MPI_Rget` | Compile with `-DUSE_GET=1` |

`rput-many.c` and `rput-indexed.c` retain configurable forms used to establish
the thresholds and operation matrix.  All programs are pure MPI C; none
includes an ARMCI header or links an ARMCI library.

## MPICH 5/UCX noncontiguous `MPI_Rget`

`mpich-ucx-rget-vector.c` seeds a remote window with a contiguous `MPI_Put`,
then fetches 16 two-double blocks with `MPI_Rget` and vector datatypes:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    mpich-ucx-rget-vector.c -o mpich-ucx-rget-vector
mpiexec -n 2 ./mpich-ucx-rget-vector
```

MPICH 5.0.1 CH4/UCX returns successfully from `MPI_Wait`, but every selected
element remains at its sentinel value.  The defect occurs over both native
InfiniBand and TCP over IPoIB.

Compile the non-request control with `-DUSE_GET=1`.  It passes with the same
vector datatypes.  `MPI_Rget` also passes when both datatypes are predefined,
and the vector case passes over CH4/OFI.  Defining either
`ORIGIN_VECTOR=0` or `TARGET_VECTOR=0` independently still fails, so a
noncontiguous datatype in either argument is sufficient.

The UCX netmod's noncontiguous branch calls the non-request
`MPIDIG_mpi_get`, ignoring the request pointer supplied by `MPI_Rget`.
The outer wrapper sees a NULL request and returns an already-completed request
while the active-message get remains outstanding.  The same code is present
in MPICH 5.0.1 and MPICH `main` as inspected on 2026-07-23.

## Open MPI 5/UCX outstanding request puts

The fixed issue MCVE is `ompi-ucx-rput-minimal.c`:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    ompi-ucx-rput-minimal.c -o ompi-ucx-rput-minimal
mpiexec -n 2 ./ompi-ucx-rput-minimal
```

Compile the neighboring passing case with `-DREQUEST_COUNT=254`.

`rput-many.c` issues scalar `MPI_Rput` operations to one peer without
completing any request until all operations have been initiated.  With Open
MPI 5.0.10rc2 and UCX 1.21, the boundary is deterministic on Iris and Thor
over both native InfiniBand and TCP over IPoIB:

| Outstanding `MPI_Rput` requests | Result |
| ---: | --- |
| 254 | `OK` |
| 255 | UCX abort: endpoint reference count is already `UINT8_MAX` |

Compile and run directly in an already selected MPI environment:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror rput-many.c -o rput-many
mpiexec -n 2 ./rput-many 254
mpiexec -n 2 ./rput-many 255
```

The failure path is:

```text
MPI_Rput
ompi_osc_ucx_rput
opal_common_ucx_wpmem_flush_ep_nb
ucp_worker_flush_nb
ucp_worker_flush_req_set_next_ep
ucp_ep_refcount_add
```

Despite the Open MPI helper's `flush_ep` name, it starts a worker flush for
each RMA request.  Each unfinished worker flush holds another reference on the
same UCX endpoint.  The endpoint already has one reference, so 254 request
flushes fill the eight-bit counter and request 255 triggers the assertion.

This is not Open MPI issue 14181, 14175, or 14173.  It requires no derived
datatype or accumulate operation and fails at a precise outstanding-request
count.

## MPICH 5/OFI packed indexed `MPI_Rput`

The fixed issue MCVE is `mpich-ofi-rput-minimal.c`:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror \
    mpich-ofi-rput-minimal.c -o mpich-ofi-rput-minimal
mpiexec -n 2 ./mpich-ofi-rput-minimal
```

Compile the non-request-based `MPI_Put` control with
`-DUSE_NONREQUEST_PUT`.

`rput-indexed.c` builds a sparse indexed origin datatype and an adjacent-block
indexed target datatype.  The smallest tested failure has two blocks of 20
doubles with a one-double gap in the origin:

```sh
mpicc -std=c99 -O2 -g -Wall -Wextra -Werror rput-indexed.c -o rput-indexed
mpiexec -n 2 ./rput-indexed 1 1 20 1 0 21 rput
mpiexec -n 2 ./rput-indexed 1 2 20 1 0 21 rput
```

The one-block control prints `OK`.  The two-block case receives signal 11 in:

```text
issue_packed_put
MPIDI_OFI_INIT_CHUNK_CONTEXT
MPIDI_OFI_pack_put
MPIDI_OFI_do_put
MPIDI_NM_mpi_rput
MPI_Rput
```

`MPIDI_OFI_pack_put` retains a pointer to the output request but never creates
the request object.  `MPIDI_OFI_INIT_CHUNK_CONTEXT` then dereferences that NULL
request to increment its completion counter.  MPICH's working no-pack path
explicitly creates the request before initializing completion chunks.

Hold the datatype and transport constant while selecting working operation
controls with the final argument:

```sh
mpiexec -n 2 ./rput-indexed 1 2 20 1 0 21 put
mpiexec -n 2 ./rput-indexed 1 2 20 1 0 21 raccumulate
```

Both controls verify successfully with MPICH CH4/OFI.  The original `rput`
case fails over OFI verbs/RXM and OFI TCP on Iris and Thor, while the same
program passes over CH4/UCX.  This is not MPICH issue 7886, which is a CH4/UCX
active-message header overflow involving accumulate and a much larger type.

The positional arguments to `rput-indexed.c` are:

| Position | Meaning | Reproducing value |
| ---: | --- | --- |
| 1 | request count | `1` |
| 2 | indexed blocks per request | `2` |
| 3 | doubles per block | `20` |
| 4 | free datatypes before waiting | `1` |
| 5 | use separately allocated origins | `0` |
| 6 | origin stride in doubles | `21` |
| 7 | `put`, `rput`, or `raccumulate` | `rput` |

The crash occurs inside `MPI_Rput`, before datatype release, so changing
argument 4 does not repair it.

## Two-node transport driver

Submit either program through the retained driver by exporting these values:

```text
DIAG_STACK=ompi5-ucx or mpich5-ofi
DIAG_NETWORK=ib or tcp
DIAG_SOURCE=absolute path to one source above
DIAG_ROOT=absolute result directory
DIAG_ARGUMENTS=the program arguments
DIAG_TIMEOUT=120
```

For `DIAG_NETWORK=tcp`, the driver explicitly selects IPoIB interface
`ib0_mlx5`.  It never selects a native Ethernet interface.
