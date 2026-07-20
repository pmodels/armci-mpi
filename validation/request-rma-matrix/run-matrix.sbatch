#!/bin/bash

#SBATCH --nodes=2
#SBATCH --ntasks-per-node=2
#SBATCH --time=02:00:00
#SBATCH --constraint=x86_64

set -uo pipefail

: "${MATRIX_BUILD_ROOT:?set MATRIX_BUILD_ROOT to the shared build directory}"
: "${MATRIX_RESULTS_ROOT:?set MATRIX_RESULTS_ROOT to the shared result directory}"

mpi_releases=${MPI_RELEASES:-$HOME/MPI/external/mpi-releases}
hpcx_root=${HPCX_ROOT:-/global/software/rocky-9.x86_64/modules/gcc/11/hpcx/2.50}
libfabric_compat=${LIBFABRIC_COMPAT:-$MATRIX_BUILD_ROOT/libfabric-2.6.0-rdma114}

cases=()
for tree in master pr; do
    for stack in mpich5-ucx mpich5-ofi ompi5-ucx ompi5-ofi ompi4-ucx; do
        for network in ib tcp; do
            cases+=("$tree:$stack:$network")
        done
    done
done

case_index=${SLURM_ARRAY_TASK_ID:-0}
if (( case_index < 0 || case_index >= ${#cases[@]} )); then
    echo "invalid array index $case_index" >&2
    exit 2
fi

IFS=: read -r tree stack network <<<"${cases[$case_index]}"
platform=${SLURM_JOB_PARTITION:-unknown}
shared_build=$MATRIX_BUILD_ROOT/$tree-$stack
result_stem=$platform-$tree-$stack-$network
result_file=$MATRIX_RESULTS_ROOT/$result_stem.tsv
log_root=$MATRIX_RESULTS_ROOT/logs/$result_stem
local_build=/tmp/armci-request-matrix-${SLURM_ARRAY_JOB_ID:-$$}-$case_index

mkdir -p "$MATRIX_RESULTS_ROOT" "$log_root"
printf 'platform\ttree\tstack\tnetwork\tprofile\ttest\tresult\trc\tseconds\tdetail\n' >"$result_file"

if [[ ! -x $shared_build/tests/contrib/armci-test ]]; then
    echo "missing build: $shared_build" >&2
    exit 2
fi

srun --nodes=2 --ntasks=2 --ntasks-per-node=1 \
    bash -c 'destination=$1; source_dir=$2; rm -rf "$destination"; cp -a "$source_dir" "$destination"' \
    bash "$local_build" "$shared_build"
cd "$local_build"

case "$stack" in
    mpich5-ucx)
        source "$mpi_releases/env/activate-mpich-5.0.1-ucx.sh" >/dev/null
        launcher=(mpiexec -n 4 -ppn 2)
        backend=ucx
        ;;
    mpich5-ofi)
        source "$mpi_releases/env/activate-mpich-5.0.1-ofi.sh" >/dev/null
        launcher=(mpiexec -n 4 -ppn 2)
        backend=ofi
        ;;
    ompi5-ucx)
        source "$mpi_releases/env/activate-openmpi-5.0.10-ucx.sh" >/dev/null
        launcher=(mpirun --timeout 110 -n 4 --map-by ppr:2:node)
        backend=ucx
        ;;
    ompi5-ofi)
        source "$mpi_releases/env/activate-openmpi-5.0.10-ofi.sh" >/dev/null
        launcher=(mpirun --timeout 110 -n 4 --map-by ppr:2:node)
        backend=ofi
        ;;
    ompi4-ucx)
        export HPCX_ENABLE_OMPI4=1
        source "$hpcx_root/hpcx-init-ompi.sh"
        hpcx_load
        launcher=(mpirun --timeout 110 -n 4 --map-by ppr:2:node)
        backend=ucx
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
if [[ $network == ib ]]; then
    ib_device=$(choose_ib_device) || {
        echo "no active InfiniBand mlx5 device on $(hostname)" >&2
        exit 2
    }
fi

ipoib_interface=
if [[ $network == tcp ]]; then
    ipoib_interface=ib0_mlx5
    if ! srun --nodes="$SLURM_JOB_NUM_NODES" \
            --ntasks="$SLURM_JOB_NUM_NODES" \
            bash -c '
                interface=$1
                [[ -r /sys/class/net/$interface/type ]] || exit 1
                [[ $(</sys/class/net/$interface/type) == 32 ]] || exit 1
                [[ $(</sys/class/net/$interface/operstate) == up ]] || exit 1
                ip -o -4 addr show dev "$interface" scope global | grep -q .
            ' bash "$ipoib_interface"; then
        echo "IPoIB interface $ipoib_interface is not usable on every allocated node" >&2
        exit 2
    fi
fi

if [[ $platform == rome ]]; then
    if [[ $backend == ucx ]]; then
        source "$mpi_releases/env/preload-ucx-1.21.x-rdma114.sh" >/dev/null
        export UCX_IB_MLX5_DEVX=try
        export UCX_IB_PCI_RELAXED_ORDERING=auto
    elif [[ $backend == ofi ]]; then
        if [[ ! -f $libfabric_compat/lib/libfabric.so ]]; then
            echo "missing Rome-compatible libfabric: $libfabric_compat" >&2
            exit 2
        fi
        export LD_LIBRARY_PATH=$libfabric_compat/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    fi
fi

if [[ $backend == ucx ]]; then
    unset FI_PROVIDER FI_VERBS_DEVICE_NAME FI_VERBS_IFACE
    if [[ $network == ib ]]; then
        export UCX_TLS=rc_x,sm,self
        export UCX_NET_DEVICES=$ib_device:1
    else
        export UCX_TLS=tcp,sm,self
        export UCX_NET_DEVICES=$ipoib_interface
        if [[ $stack == ompi* ]]; then
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
    if [[ $network == ib ]]; then
        export FI_PROVIDER='verbs;ofi_rxm'
        export FI_VERBS_DEVICE_NAME=$ib_device
        unset FI_VERBS_IFACE FI_VERBS_PREFER_XRC FI_OFI_RXM_USE_SRX
    else
        export FI_PROVIDER=tcp
        export FI_TCP_IFACE=$ipoib_interface
        unset FI_VERBS_DEVICE_NAME FI_VERBS_IFACE FI_VERBS_PREFER_XRC FI_OFI_RXM_USE_SRX
        if [[ $stack == ompi5-ofi ]]; then
            export OMPI_MCA_pml=cm
            export OMPI_MCA_mtl=ofi
            export OMPI_MCA_osc=pt2pt
            export OMPI_MCA_opal_common_ofi_provider_include=tcp
            export OMPI_MCA_opal_common_ofi_provider_exclude=
        fi
    fi
fi

echo "case=$result_stem nodes=$SLURM_NODELIST"
echo "mpi=$(${launcher[0]} --version 2>&1 | head -1)"
echo "backend=$backend network=$network ib_device=${ib_device:-none} ipoib_interface=${ipoib_interface:-none}"
echo "source_commit=$(<matrix-source-commit.txt)"
echo "placement=$(timeout 120 "${launcher[@]}" hostname 2>&1 | sort | uniq -c | tr '\n' ' ')"

armci_variables=(
    ARMCI_VERBOSE ARMCI_USE_REQUEST_ATOMICS ARMCI_FLUSH_REQUEST_ATOMICS
    ARMCI_EXPLICIT_NB_PROGRESS ARMCI_RMA_ATOMICITY ARMCI_NO_FLUSH_LOCAL
    ARMCI_RMA_NOCHECK ARMCI_USE_WIN_ALLOCATE ARMCI_USE_ALLOC_SHM
    ARMCI_DISABLE_SHM_ACC ARMCI_USE_SAME_OP ARMCI_RMA_ORDERING
    ARMCI_MSG_BARRIER_SYNCS ARMCI_SHR_BUF_METHOD ARMCI_IOV_METHOD
    ARMCI_IOV_BATCHED_LIMIT ARMCI_IOV_CHECKS ARMCI_STRIDED_METHOD
)

clear_armci_environment()
{
    local variable
    for variable in "${armci_variables[@]}"; do
        unset "$variable"
    done
}

run_test()
{
    local profile=$1 test_name=$2 settings=$3
    local test_path=tests/$test_name
    local safe_test=${test_name//\//-}
    local log_file=$log_root/$profile-$safe_test.log
    local started finished elapsed rc result detail

    clear_armci_environment
    if [[ -n $settings ]]; then
        eval "export $settings"
    fi
    export ARMCI_VERBOSE=2

    started=$(date +%s)
    timeout --kill-after=10s 120s "${launcher[@]}" "$test_path" >"$log_file" 2>&1
    rc=$?
    finished=$(date +%s)
    elapsed=$((finished - started))

    if [[ $rc -eq 124 || $rc -eq 137 ]]; then
        result=HANG
    elif LC_ALL=C grep -aEqi 'Bailing out|CORRUPT|WRONG|Segmentation fault|Assertion.*failed|ERROR:' "$log_file"; then
        result=FAIL
    elif [[ $rc -ne 0 ]]; then
        result=FAIL
    else
        result=PASS
    fi

    detail=$(LC_ALL=C grep -aEi 'Bailing out|CORRUPT|WRONG|Segmentation fault|Assertion.*failed|ERROR:|IOV_METHOD' "$log_file" \
        | tail -1 | tr '\t\n' '  ')
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%s\n' \
        "$platform" "$tree" "$stack" "$network" "$profile" "$test_name" \
        "$result" "$rc" "$elapsed" "$detail" >>"$result_file"
    printf '%-24s %-38s %-4s rc=%-3d %3ds %s\n' \
        "$profile" "$test_name" "$result" "$rc" "$elapsed" "$detail"
}

profiles=(
    'default|'
    'request_atomics_0|ARMCI_USE_REQUEST_ATOMICS=0'
    'request_atomics_1|ARMCI_USE_REQUEST_ATOMICS=1'
    'request_atomics_flush|ARMCI_USE_REQUEST_ATOMICS=1 ARMCI_FLUSH_REQUEST_ATOMICS=1'
    'explicit_progress_0|ARMCI_EXPLICIT_NB_PROGRESS=0'
    'explicit_progress_1|ARMCI_EXPLICIT_NB_PROGRESS=1'
    'rma_atomicity_0|ARMCI_RMA_ATOMICITY=0'
    'rma_atomicity_1|ARMCI_RMA_ATOMICITY=1'
    'no_flush_local_1|ARMCI_NO_FLUSH_LOCAL=1'
    'rma_nocheck_0|ARMCI_RMA_NOCHECK=0'
    'rma_nocheck_1|ARMCI_RMA_NOCHECK=1'
    'win_create|ARMCI_USE_WIN_ALLOCATE=0'
    'alloc_shm_0|ARMCI_USE_ALLOC_SHM=0'
    'disable_shm_acc_1|ARMCI_DISABLE_SHM_ACC=1'
    'same_op_1|ARMCI_USE_SAME_OP=1'
    'ordering_none|ARMCI_RMA_ORDERING=none'
    'barrier_syncs_1|ARMCI_MSG_BARRIER_SYNCS=1'
    'shr_copy|ARMCI_SHR_BUF_METHOD=COPY'
    'shr_noguard|ARMCI_SHR_BUF_METHOD=NOGUARD'
    'iov_auto|ARMCI_IOV_METHOD=AUTO'
    'iov_consrv|ARMCI_IOV_METHOD=CONSRV'
    'iov_batched|ARMCI_IOV_METHOD=BATCHED'
    'iov_direct|ARMCI_IOV_METHOD=DIRECT'
    'iov_batched_limit_1|ARMCI_IOV_METHOD=BATCHED ARMCI_IOV_BATCHED_LIMIT=1'
    'iov_checks_1|ARMCI_IOV_CHECKS=1'
    'strided_iov|ARMCI_STRIDED_METHOD=IOV'
    'strided_direct|ARMCI_STRIDED_METHOD=DIRECT'
    'strict_completion|ARMCI_USE_REQUEST_ATOMICS=1 ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_NO_FLUSH_LOCAL=1 ARMCI_RMA_NOCHECK=0 ARMCI_EXPLICIT_NB_PROGRESS=0'
)

if [[ $tree == pr ]]; then
    for profile_entry in "${profiles[@]}"; do
        IFS='|' read -r profile settings <<<"$profile_entry"
        run_test "$profile" contrib/armci-test "$settings"
    done
else
    run_test default contrib/armci-test ''
fi

focused_tests=(
    test_onesided
    test_onesided_shared
    test_putv
    test_accs
    test_puts_gets
    test_rmw_fadd
    test_mutex
    contrib/non-blocking/simple
)
for test_name in "${focused_tests[@]}"; do
    if [[ -x tests/$test_name ]]; then
        run_test default "$test_name" ''
    fi
done

clear_armci_environment
echo "summary: $(awk -F '\t' 'NR>1 {n[$7]++} END {for (r in n) printf "%s=%d ", r, n[r]}' "$result_file")"
echo "result_file=$result_file"
