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

The final no-override production sweep, including OFI PSM3 and the VibeMPI
follow-up, is recorded in
[`DEFAULT-PRODUCTION-VALIDATION.md`](DEFAULT-PRODUCTION-VALIDATION.md).

The available HPC-X Open MPI 4 installation does not contain an OFI MTL, BTL,
or OSC component, so Open MPI 4/OFI is unsupported and is not mislabeled as a
tested combination.

`build-matrix.sh` creates five builds for each source tree.  `submit-matrix.sh`
submits one 20-element array on each requested partition.  Each array element
selects one source tree, MPI/backend build, and fabric (`ib` or `tcp`).

Both source trees run the complete `make check` suite for the 32-point Cartesian
product of these environment dimensions:

| Summary column | Environment variable | Values |
| --- | --- | --- |
| `allocate` | `ARMCI_USE_WIN_ALLOCATE` | `0`, `1` |
| `strided` | `ARMCI_STRIDED_METHOD` | `DIRECT`, `IOV` |
| `vector` | `ARMCI_IOV_METHOD` | `DIRECT`, `BATCHED` |
| `atomicity` | `ARMCI_RMA_ATOMICITY` | `0`, `1` |
| `flush_request_atomics` | `ARMCI_FLUSH_REQUEST_ATOMICS` | `0`, `1` |

`ARMCI_USE_REQUEST_ATOMICS=1` is fixed throughout the grid so both values of
`FLUSH_REQUEST_ATOMICS` exercise the request-based atomic path.

Each source tree runs the same grid.  A tuple passes only when `make check`
exits successfully and its Automake summary
reports exactly `TOTAL: 43`, `PASS: 43`, and `FAIL: 0`.  Every test runs with
`ARMCI_VERBOSE=2`.  The complete `make check` output, suite summary, and
individual test logs are retained in a profile-specific result directory.

All TCP cases run over IPoIB, not the nodes' native Ethernet adapters.  The
runner requires `ib0_mlx5` to be up, to report the InfiniBand ARP hardware type,
and to have an IPv4 address on every allocated node.  It binds UCX with
`UCX_NET_DEVICES`, libfabric with `FI_TCP_IFACE`, and Open MPI's TCP BTL and UCX
OSC with their interface/device selection variables.

For TCP, Open MPI uses transport-specific component selections.  UCX RMA uses
the UCX OSC while OB1/TCP carries MPI point-to-point traffic, because the UCX
PML intentionally refuses a TCP-only UCX configuration.  Open MPI 5 OFI/TCP is
reported as unavailable: its OFI BTL requires native RDMA and atomic
capabilities that the libfabric TCP provider does not supply, and this Open MPI
installation has no pt2pt OSC that could implement RMA over OFI MTL messages.
MPICH 5 OFI/TCP remains fully exercised with `FI_PROVIDER=tcp` and
`FI_TCP_IFACE=ib0_mlx5`.

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
