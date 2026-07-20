#!/bin/bash

#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --time=00:15:00
#SBATCH --constraint=x86_64

set -euo pipefail

: "${DIAG_STACK:?set DIAG_STACK}"
: "${DIAG_NETWORK:?set DIAG_NETWORK to ib or tcp}"
: "${DIAG_PROGRAM:?set DIAG_PROGRAM}"
: "${DIAG_ROOT:?set DIAG_ROOT}"

mpi_releases=${MPI_RELEASES:-$HOME/MPI/external/mpi-releases}
hpcx_root=${HPCX_ROOT:-/global/software/rocky-9.x86_64/modules/gcc/11/hpcx/2.50}
timeout_seconds=${DIAG_TIMEOUT:-300}
arguments=${DIAG_ARGUMENTS:-}
debugger=${DIAG_GDB:-0}

case $DIAG_STACK in
    mpich5-ucx)
        source "$mpi_releases/env/activate-mpich-5.0.1-ucx.sh" >/dev/null
        launcher=(mpiexec -n 2 -ppn 1)
        backend=ucx
        ;;
    mpich5-ofi)
        source "$mpi_releases/env/activate-mpich-5.0.1-ofi.sh" >/dev/null
        launcher=(mpiexec -n 2 -ppn 1)
        backend=ofi
        ;;
    ompi5-ucx)
        source "$mpi_releases/env/activate-openmpi-5.0.10-ucx.sh" >/dev/null
        launcher=(mpirun --timeout "$timeout_seconds" -n 2 --map-by ppr:1:node)
        backend=ucx
        ;;
    ompi5-ofi)
        source "$mpi_releases/env/activate-openmpi-5.0.10-ofi.sh" >/dev/null
        launcher=(mpirun --timeout "$timeout_seconds" -n 2 --map-by ppr:1:node)
        backend=ofi
        ;;
    ompi4-ucx)
        export HPCX_ENABLE_OMPI4=1
        source "$hpcx_root/hpcx-init-ompi.sh"
        hpcx_load
        launcher=(mpirun --timeout "$timeout_seconds" -n 2 --map-by ppr:1:node)
        backend=ucx
        ;;
    *)
        echo "unsupported stack: $DIAG_STACK" >&2
        exit 2
        ;;
esac

choose_ib_device()
{
    local sysdev state layer
    for sysdev in /sys/class/infiniband/mlx5_*; do
        [[ -r $sysdev/ports/1/state && -r $sysdev/ports/1/link_layer ]] || continue
        read -r state <"$sysdev/ports/1/state"
        read -r layer <"$sysdev/ports/1/link_layer"
        if [[ $state == *ACTIVE* && $layer == InfiniBand ]]; then
            basename "$sysdev"
            return 0
        fi
    done
    return 1
}

ib_device=
ipoib_interface=
if [[ $DIAG_NETWORK == ib ]]; then
    ib_device=$(choose_ib_device)
elif [[ $DIAG_NETWORK == tcp ]]; then
    ipoib_interface=ib0_mlx5
else
    echo "unsupported network: $DIAG_NETWORK" >&2
    exit 2
fi

if [[ $backend == ucx ]]; then
    unset FI_PROVIDER FI_VERBS_DEVICE_NAME FI_TCP_IFACE
    if [[ $DIAG_NETWORK == ib ]]; then
        export UCX_TLS=rc_x,sm,self
        export UCX_NET_DEVICES=$ib_device:1
    else
        export UCX_TLS=tcp,sm,self
        export UCX_NET_DEVICES=$ipoib_interface
        if [[ $DIAG_STACK == ompi* ]]; then
            export OMPI_MCA_pml=ob1
            export OMPI_MCA_btl=self,tcp
            export OMPI_MCA_btl_tcp_if_include=$ipoib_interface
            export OMPI_MCA_osc=ucx
            export OMPI_MCA_osc_ucx_tls=tcp
            export OMPI_MCA_osc_ucx_devices=$ipoib_interface
        fi
    fi
else
    unset UCX_TLS UCX_NET_DEVICES
    if [[ $DIAG_NETWORK == ib ]]; then
        export FI_PROVIDER='verbs;ofi_rxm'
        export FI_VERBS_DEVICE_NAME=$ib_device
        unset FI_TCP_IFACE
    else
        export FI_PROVIDER=tcp
        export FI_TCP_IFACE=$ipoib_interface
        unset FI_VERBS_DEVICE_NAME
    fi
fi

export ARMCI_VERBOSE=2
mkdir -p "$DIAG_ROOT/runs/$SLURM_JOB_ID"
cd "$DIAG_ROOT/runs/$SLURM_JOB_ID"

echo "stack=$DIAG_STACK backend=$backend network=$DIAG_NETWORK nodes=$SLURM_NODELIST"
echo "mpi=$(${launcher[0]} --version 2>&1 | head -1)"
echo "program=$DIAG_PROGRAM arguments=$arguments timeout=$timeout_seconds gdb=$debugger"
echo "ARMCI_VERBOSE=$ARMCI_VERBOSE"
echo "ARMCI_USE_WIN_ALLOCATE=${ARMCI_USE_WIN_ALLOCATE:-unset}"
echo "ARMCI_STRIDED_METHOD=${ARMCI_STRIDED_METHOD:-unset}"
echo "ARMCI_IOV_METHOD=${ARMCI_IOV_METHOD:-unset}"
echo "ARMCI_RMA_ATOMICITY=${ARMCI_RMA_ATOMICITY:-unset}"
echo "ARMCI_FLUSH_REQUEST_ATOMICS=${ARMCI_FLUSH_REQUEST_ATOMICS:-unset}"
echo "ARMCI_USE_REQUEST_ATOMICS=${ARMCI_USE_REQUEST_ATOMICS:-unset}"
echo "placement=$(timeout 60 "${launcher[@]}" hostname 2>&1 | sort | uniq -c | tr '\n' ' ')"

read -r -a argument_array <<<"$arguments"
command=("$DIAG_PROGRAM" "${argument_array[@]}")
if [[ $debugger == 1 ]]; then
    command=(gdb -q -batch -ex run -ex "thread apply all bt full" --args "${command[@]}")
fi

timeout --kill-after=10s "${timeout_seconds}s" "${launcher[@]}" "${command[@]}"
