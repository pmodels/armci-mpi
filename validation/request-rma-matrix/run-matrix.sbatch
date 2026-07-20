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
mpi_check_timeout=${MATRIX_MPI_CHECK_TIMEOUT:-110}
make_check_timeout=${MATRIX_MAKE_CHECK_TIMEOUT:-300}
profile_filter=${MATRIX_PROFILE_FILTER:-}
use_request_atomics=${MATRIX_USE_REQUEST_ATOMICS:-1}

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
printf 'platform\ttree\tstack\tnetwork\tprofile\ttest\tresult\trc\tseconds\tdetail\tallocate\tstrided\tvector\tatomicity\tflush_request_atomics\n' >"$result_file"

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
        launcher=(mpirun --timeout "$mpi_check_timeout" -n 4 --map-by ppr:2:node)
        backend=ucx
        ;;
    ompi5-ofi)
        source "$mpi_releases/env/activate-openmpi-5.0.10-ofi.sh" >/dev/null
        launcher=(mpirun --timeout "$mpi_check_timeout" -n 4 --map-by ppr:2:node)
        backend=ofi
        ;;
    ompi4-ucx)
        export HPCX_ENABLE_OMPI4=1
        source "$hpcx_root/hpcx-init-ompi.sh"
        hpcx_load
        launcher=(mpirun --timeout "$mpi_check_timeout" -n 4 --map-by ppr:2:node)
        backend=ucx
        ;;
esac

if [[ $stack == mpich* ]]; then
    check_launcher=(mpiexec -n 2 -ppn 1)
else
    check_launcher=(mpirun --timeout "$mpi_check_timeout" -n 2 --map-by ppr:1:node)
fi
mpi_command_timeout=$((mpi_check_timeout + 10))

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

if [[ $stack == ompi5-ofi && $network == tcp ]]; then
    detail="Open MPI 5 has no OSC component capable of RMA over the libfabric TCP provider"
    for allocate in 0 1; do
        for strided in DIRECT IOV; do
            for vector in DIRECT BATCHED; do
                for atomicity in 0 1; do
                    for flush_request_atomics in 0 1; do
                        profile="allocate_${allocate}-strided_${strided}-vector_${vector}-atomicity_${atomicity}-flush_${flush_request_atomics}"
                        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                            "$platform" "$tree" "$stack" "$network" "$profile" make-check \
                            SKIP 0 0 "$detail" "$allocate" "$strided" "$vector" \
                            "$atomicity" "$flush_request_atomics" >>"$result_file"
                    done
                done
            done
        done
    done
    echo "backend=$backend network=$network ib_device=none ipoib_interface=$ipoib_interface"
    echo "summary: SKIP=32 ($detail)"
    echo "result_file=$result_file"
    exit 0
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
    fi
fi

echo "case=$result_stem nodes=$SLURM_NODELIST"
echo "mpi=$(${launcher[0]} --version 2>&1 | head -1)"
echo "backend=$backend network=$network ib_device=${ib_device:-none} ipoib_interface=${ipoib_interface:-none}"
echo "use_request_atomics=$use_request_atomics"
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

run_check()
{
    local profile=$1 settings=$2 allocate=$3 strided=$4 vector=$5 atomicity=$6
    local flush_request_atomics=$7
    local profile_log_root=$log_root/$profile
    local check_log=$profile_log_root/check.log
    local suite_log=$profile_log_root/test-suite.log
    local started finished elapsed rc result detail total passed failed skipped

    clear_armci_environment
    if [[ -n $settings ]]; then
        eval "export $settings"
    fi
    export ARMCI_VERBOSE=2
    mkdir -p "$profile_log_root"
    find . -type f -name '*.log' -delete

    started=$(date +%s)
    timeout --kill-after=30s "${make_check_timeout}s" \
        make check MPIEXEC="timeout --kill-after=10s ${mpi_command_timeout}s ${check_launcher[*]}" \
        >"$check_log" 2>&1
    rc=$?
    finished=$(date +%s)
    elapsed=$((finished - started))

    find . -type f -name '*.log' -exec cp --parents '{}' "$profile_log_root" ';'
    total=$(awk '/^# TOTAL:/ {print $3}' "$suite_log")
    passed=$(awk '/^# PASS:/ {print $3}' "$suite_log")
    failed=$(awk '/^# FAIL:/ {print $3}' "$suite_log")
    skipped=$(awk '/^# SKIP:/ {print $3}' "$suite_log")
    total=${total:-0}
    passed=${passed:-0}
    failed=${failed:-0}
    skipped=${skipped:-0}

    if [[ $rc -eq 0 && $total -eq 43 && $passed -eq 43 && $failed -eq 0 ]]; then
        result=PASS
    else
        result=FAIL
    fi

    detail="$passed/$total passed; FAIL=$failed SKIP=$skipped"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$platform" "$tree" "$stack" "$network" "$profile" make-check \
        "$result" "$rc" "$elapsed" "$detail" "$allocate" "$strided" \
        "$vector" "$atomicity" "$flush_request_atomics" >>"$result_file"
    printf '%-24s %-4s rc=%-3d %4ds %s\n' \
        "$profile" "$result" "$rc" "$elapsed" "$detail"
}

for allocate in 0 1; do
    for strided in DIRECT IOV; do
        for vector in DIRECT BATCHED; do
            for atomicity in 0 1; do
                for flush_request_atomics in 0 1; do
                    profile="allocate_${allocate}-strided_${strided}-vector_${vector}-atomicity_${atomicity}-flush_${flush_request_atomics}"
                    if [[ -n $profile_filter && $profile != $profile_filter ]]; then
                        continue
                    fi
                    settings="ARMCI_USE_WIN_ALLOCATE=$allocate ARMCI_STRIDED_METHOD=$strided ARMCI_IOV_METHOD=$vector ARMCI_RMA_ATOMICITY=$atomicity ARMCI_USE_REQUEST_ATOMICS=$use_request_atomics ARMCI_FLUSH_REQUEST_ATOMICS=$flush_request_atomics"
                    run_check "$profile" "$settings" "$allocate" "$strided" \
                        "$vector" "$atomicity" "$flush_request_atomics"
                done
            done
        done
    done
done

clear_armci_environment
echo "summary: $(awk -F '\t' 'NR>1 {n[$7]++} END {for (r in n) printf "%s=%d ", r, n[r]}' "$result_file")"
echo "result_file=$result_file"
