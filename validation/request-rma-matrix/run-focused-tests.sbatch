#!/usr/bin/env bash

#SBATCH --nodes=2
#SBATCH --ntasks-per-node=4
#SBATCH --time=00:35:00
#SBATCH --constraint=x86_64

set -uo pipefail

: "${SWEEP_BUILD_ROOT:?}"
: "${SWEEP_RESULT_ROOT:?}"

mpi_releases=${MPI_RELEASES:-/global/home/users/jehammond/MPI/external/mpi-releases}
hpcx_root=${HPCX_ROOT:-/global/software/rocky-9.x86_64/modules/gcc/11/hpcx/2.50}
vibempi_root=${VIBEMPI_ROOT:-/global/home/users/jehammond/Claude/VibeMPI}
test_timeout=${SWEEP_TEST_TIMEOUT:-180}

profiles=(
    mpich5-ucx-ib
    mpich5-ucx-tcp-ipoib
    mpich5-ofi-verbs-rxm
    mpich5-ofi-psm3
    mpich5-ofi-tcp-ipoib
    ompi4-ucx-ib
    ompi4-ucx-tcp-ipoib
    ompi5-ucx-ib
    ompi5-ucx-tcp-ipoib
    ompi5-ofi-verbs-rxm
    ompi5-ofi-psm3
    ompi5-ofi-tcp-ipoib
    ompi6-ucx-ib
    ompi6-ucx-tcp-ipoib
    ompi6-ofi-verbs-rxm
    ompi6-ofi-psm3
    ompi6-ofi-tcp-ipoib
    mvapich4-ucx-ib
    mvapich4-ucx-tcp-ipoib
    mvapich4-ofi-verbs-rxm
    mvapich4-ofi-psm3
    mvapich4-ofi-tcp-ipoib
    vibempi-auto
    vibempi-tcp-ipoib
    vibempi-mux
    vibempi-ofi-verbs-rxm
    vibempi-ofi-tcp-ipoib
    vibempi-ucx-ib
    vibempi-ucx-tcp-ipoib
)

profile_index=${SLURM_ARRAY_TASK_ID:-0}
if (( profile_index < 0 || profile_index >= ${#profiles[@]} )); then
    echo "invalid profile index: ${profile_index}" >&2
    exit 2
fi

profile=${profiles[profile_index]}
case "${profile}" in
    mpich5-ucx-*) stack=mpich5-ucx ;;
    mpich5-ofi-*) stack=mpich5-ofi ;;
    ompi4-ucx-*) stack=ompi4-ucx ;;
    ompi5-ucx-*) stack=ompi5-ucx ;;
    ompi5-ofi-*) stack=ompi5-ofi ;;
    ompi6-ucx-*) stack=ompi6-ucx ;;
    ompi6-ofi-*) stack=ompi6-ofi ;;
    mvapich4-ucx-*) stack=mvapich4-ucx ;;
    mvapich4-ofi-*) stack=mvapich4-ofi ;;
    vibempi-*) stack=vibempi ;;
esac

result_dir="${SWEEP_RESULT_ROOT}/${profile}"
build_dir="${SWEEP_BUILD_ROOT}/${stack}"
summary="${result_dir}/summary.tsv"
manifest="${result_dir}/manifest.tsv"

mkdir -p "${result_dir}/logs" "${result_dir}/jobdirs"
printf 'profile\ttuple\ttest\trc\tseconds\tresult\n' > "${summary}"
printf 'tuple\tenvironment\n' > "${manifest}"

activate_stack()
{
    case "${stack}" in
        mpich5-ucx)
            source "${mpi_releases}/env/activate-mpich-5.0.1-ucx.sh" >/dev/null
            launcher_kind=hydra
            launcher=(mpiexec)
            ;;
        mpich5-ofi)
            source "${mpi_releases}/env/activate-mpich-5.0.1-ofi.sh" >/dev/null
            launcher_kind=hydra
            launcher=(mpiexec)
            ;;
        ompi4-ucx)
            export HPCX_ENABLE_OMPI4=1
            source "${hpcx_root}/hpcx-init-ompi.sh" >/dev/null
            hpcx_load >/dev/null
            launcher_kind=ompi
            launcher=(mpirun --timeout "${test_timeout}")
            ;;
        ompi5-ucx)
            source "${mpi_releases}/env/activate-openmpi-5.0.10-ucx.sh" >/dev/null
            launcher_kind=ompi
            launcher=(mpirun --timeout "${test_timeout}")
            ;;
        ompi5-ofi)
            source "${mpi_releases}/env/activate-openmpi-5.0.10-ofi.sh" >/dev/null
            launcher_kind=ompi
            launcher=(mpirun --timeout "${test_timeout}")
            export OMPI_MCA_osc=rdma
            export OMPI_MCA_osc_rdma_btls=ofi
            ;;
        ompi6-ucx)
            source "${mpi_releases}/env/activate-openmpi-6.0.x-ucx.sh" >/dev/null
            launcher_kind=ompi
            launcher=(mpirun --timeout "${test_timeout}")
            ;;
        ompi6-ofi)
            source "${mpi_releases}/env/activate-openmpi-6.0.x-ofi.sh" >/dev/null
            launcher_kind=ompi
            launcher=(mpirun --timeout "${test_timeout}")
            export OMPI_MCA_osc=rdma
            export OMPI_MCA_osc_rdma_btls=ofi
            ;;
        mvapich4-ucx)
            source "${mpi_releases}/env/activate-mvapich-4.1-ucx.sh" >/dev/null
            launcher_kind=hydra
            launcher=(mpiexec)
            ;;
        mvapich4-ofi)
            source "${mpi_releases}/env/activate-mvapich-4.1-ofi.sh" >/dev/null
            launcher_kind=hydra
            launcher=(mpiexec)
            ;;
        vibempi)
            export PATH="${vibempi_root}/install/bin:${PATH}"
            export LD_LIBRARY_PATH="${vibempi_root}/build:${vibempi_root}/build/_deps/libfabric-install/lib:${LD_LIBRARY_PATH:-}"
            export LD_PRELOAD="${vibempi_root}/build/libmpi.so"
            launcher_kind=srun
            launcher=(srun)
            ;;
    esac
}

choose_ib_device()
{
    local sysdev state layer

    for sysdev in /sys/class/infiniband/mlx5_*; do
        [[ -r ${sysdev}/ports/1/state &&
           -r ${sysdev}/ports/1/link_layer ]] || continue
        read -r state < "${sysdev}/ports/1/state"
        read -r layer < "${sysdev}/ports/1/link_layer"
        if [[ ${state} == *ACTIVE* && ${layer} == InfiniBand ]]; then
            basename "${sysdev}"
            return 0
        fi
    done
    return 1
}

activate_stack

ib_device=
ipoib_interface=
case "${profile}" in
    *-ucx-ib|*-ofi-verbs-rxm)
        ib_device=$(choose_ib_device) || {
            echo "no active InfiniBand mlx5 device" >&2
            exit 2
        }
        ;;
    *-tcp-ipoib)
        ipoib_interface=ib0_mlx5
        if ! srun -N 2 -n 2 --ntasks-per-node=1 \
            bash -c 'interface=$1; [[ -r /sys/class/net/${interface}/type ]]; [[ $(</sys/class/net/${interface}/type) == 32 ]]; [[ $(</sys/class/net/${interface}/operstate) == up ]]' \
            bash "${ipoib_interface}"; then
            echo "IPoIB interface is not usable on every allocated node" >&2
            exit 2
        fi
        ;;
esac

case "${profile}" in
    *-ucx-ib)
        export UCX_TLS=rc_x,sm,self
        export UCX_NET_DEVICES="${ib_device}:1"
        if [[ ${stack} == ompi* ]]; then
            export OMPI_MCA_pml=ucx
            export OMPI_MCA_osc=ucx
        elif [[ ${stack} == vibempi ]]; then
            export VIBEMPI_TRANSPORT=ucx
            export VIBEMPI_FABRIC_LOCAL_SHM=0
            export VIBEMPI_RMA_OVER_FABRIC=1
        fi
        ;;
    *-ucx-tcp-ipoib)
        export UCX_TLS=tcp,sm,self
        export UCX_NET_DEVICES="${ipoib_interface}"
        if [[ ${stack} == ompi* ]]; then
            export OMPI_MCA_pml=ob1
            export OMPI_MCA_btl=self,tcp
            export OMPI_MCA_btl_tcp_if_include="${ipoib_interface}"
            export OMPI_MCA_osc=ucx
            export OMPI_MCA_osc_ucx_tls=tcp
            export OMPI_MCA_osc_ucx_devices="${ipoib_interface}"
        elif [[ ${stack} == vibempi ]]; then
            export VIBEMPI_TRANSPORT=ucx
            export VIBEMPI_FABRIC_LOCAL_SHM=0
            export VIBEMPI_RMA_OVER_AM=1
        fi
        ;;
    *-ofi-verbs-rxm)
        export FI_PROVIDER='verbs;ofi_rxm'
        export FI_VERBS_DEVICE_NAME="${ib_device}"
        if [[ ${stack} == vibempi ]]; then
            export VIBEMPI_TRANSPORT=ofi
            export VIBEMPI_OFI_PROVIDER='verbs;ofi_rxm'
            export VIBEMPI_FABRIC_LOCAL_SHM=0
            export VIBEMPI_RMA_OVER_FABRIC=1
        fi
        ;;
    *-ofi-psm3)
        export FI_PROVIDER=psm3
        ;;
    *-ofi-tcp-ipoib)
        export FI_PROVIDER=tcp
        export FI_TCP_IFACE="${ipoib_interface}"
        if [[ ${stack} == ompi* ]]; then
            export OMPI_MCA_btl_ofi_provider_include=tcp
        elif [[ ${stack} == vibempi ]]; then
            export VIBEMPI_TRANSPORT=ofi
            export VIBEMPI_OFI_PROVIDER='tcp;ofi_rxm'
            export VIBEMPI_FABRIC_LOCAL_SHM=0
            export VIBEMPI_RMA_OVER_FABRIC=1
        fi
        ;;
    vibempi-auto)
        unset VIBEMPI_TRANSPORT
        ;;
    vibempi-tcp-ipoib)
        export VIBEMPI_TRANSPORT=tcp
        export VIBEMPI_TCP_INTERFACE=ib0_mlx5
        ;;
    vibempi-mux)
        export VIBEMPI_TRANSPORT=mux
        ;;
esac

armci_variables=(
    ARMCI_USE_WIN_ALLOCATE
    ARMCI_STRIDED_METHOD
    ARMCI_IOV_METHOD
    ARMCI_RMA_ATOMICITY
    ARMCI_FLUSH_REQUEST_ATOMICS
    ARMCI_USE_REQUEST_ATOMICS
    ARMCI_ACCUMULATE_GRANULARITY
)

clear_armci_environment()
{
    local variable
    for variable in "${armci_variables[@]}"; do
        unset "${variable}"
    done
    export ARMCI_VERBOSE=2
}

run_one()
{
    local tuple=$1
    local test_name=$2
    local ranks=$3
    local executable=$4
    shift 4
    local tuple_env=("$@")
    local log="${result_dir}/logs/${tuple}-${test_name}.log"
    local start end rc result

    start=$(date +%s)
    {
        echo "# profile=${profile}"
        echo "# tuple=${tuple}"
        echo "# source_commit=$(<"${build_dir}/source-commit.txt")"
        echo "# ARMCI_VERBOSE=${ARMCI_VERBOSE}"
        printf '# armci_env='
        printf ' %q' "${tuple_env[@]}"
        printf '\n'
    } > "${log}"

    if [[ ${launcher_kind} == srun ]]; then
        timeout --kill-after=10s "${test_timeout}s" \
            env "${tuple_env[@]}" \
            VIBEMPI_JOB_DIR="${result_dir}/jobdirs/${tuple}-${test_name}-${SLURM_JOB_ID}" \
            "${launcher[@]}" -N 2 -n "${ranks}" --ntasks-per-node="$((ranks / 2))" \
            --cpus-per-task=2 --cpu-bind=cores --kill-on-bad-exit=1 --label \
            "${executable}" >> "${log}" 2>&1
    elif [[ ${launcher_kind} == hydra ]]; then
        timeout --kill-after=10s "${test_timeout}s" \
            env "${tuple_env[@]}" \
            "${launcher[@]}" -n "${ranks}" -ppn "$((ranks / 2))" \
            "${executable}" >> "${log}" 2>&1
    else
        timeout --kill-after=10s "${test_timeout}s" \
            env "${tuple_env[@]}" \
            "${launcher[@]}" -n "${ranks}" \
            --map-by "ppr:$((ranks / 2)):node" \
            "${executable}" >> "${log}" 2>&1
    fi
    rc=$?
    end=$(date +%s)
    result=PASS
    if [[ ${rc} -ne 0 ]] ||
       rg -q 'FAIL|CORRUPT|ARMCI Error|Found [1-9][0-9]*' "${log}"; then
        result=FAIL
    fi
    printf '%s\t%s\t%s\t%d\t%d\t%s\n' \
        "${profile}" "${tuple}" "${test_name}" "${rc}" \
        "$((end - start))" "${result}" >> "${summary}"
    echo "profile=${profile} tuple=${tuple} test=${test_name} result=${result} rc=${rc} seconds=$((end - start))"
    [[ ${result} == PASS ]]
}

run_tuple()
{
    local tuple=$1
    shift
    local tuple_env=("$@")
    local env_text

    clear_armci_environment
    printf -v env_text '%q ' "${tuple_env[@]}"
    printf '%s\t%s\n' "${tuple}" "${env_text% }" >> "${manifest}"

    if ! run_one "${tuple}" armci_test 2 \
        "${build_dir}/tests/contrib/armci-test" "${tuple_env[@]}"; then
        printf '%s\t%s\t%s\t%d\t%d\t%s\n' \
            "${profile}" "${tuple}" acc_overlap 0 0 SKIP >> "${summary}"
        printf '%s\t%s\t%s\t%d\t%d\t%s\n' \
            "${profile}" "${tuple}" location_consistency 0 0 SKIP >> "${summary}"
        echo "profile=${profile} tuple=${tuple} remaining_tests=SKIP reason=armci_test_FAIL"
        return
    fi
    run_one "${tuple}" acc_overlap 8 \
        "${build_dir}/tests/test_acc_overlap" "${tuple_env[@]}" || true
    run_one "${tuple}" location_consistency 8 \
        "${build_dir}/tests/test_location_consistency" "${tuple_env[@]}" || true
}

run_tuple default
run_tuple ideal \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=DIRECT ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_USE_REQUEST_ATOMICS=1
run_tuple portable \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=BATCHED ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_USE_REQUEST_ATOMICS=0
run_tuple request_no_flush \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=BATCHED ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=0 ARMCI_USE_REQUEST_ATOMICS=1
run_tuple no_request_no_flush \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=BATCHED ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=0 ARMCI_USE_REQUEST_ATOMICS=0
run_tuple allocate_zero \
    ARMCI_USE_WIN_ALLOCATE=0 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=BATCHED ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_USE_REQUEST_ATOMICS=0
run_tuple strided_iov \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=IOV \
    ARMCI_IOV_METHOD=BATCHED ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_USE_REQUEST_ATOMICS=0
run_tuple vector_direct_no_requests \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=DIRECT ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_USE_REQUEST_ATOMICS=0
run_tuple atomicity \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=BATCHED ARMCI_RMA_ATOMICITY=1 \
    ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_USE_REQUEST_ATOMICS=0
run_tuple atomicity_requests \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=DIRECT ARMCI_RMA_ATOMICITY=1 \
    ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_USE_REQUEST_ATOMICS=1
run_tuple granularity_one \
    ARMCI_USE_WIN_ALLOCATE=1 ARMCI_STRIDED_METHOD=DIRECT \
    ARMCI_IOV_METHOD=BATCHED ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=1 ARMCI_USE_REQUEST_ATOMICS=0 \
    ARMCI_ACCUMULATE_GRANULARITY=1
run_tuple conservative \
    ARMCI_USE_WIN_ALLOCATE=0 ARMCI_STRIDED_METHOD=IOV \
    ARMCI_IOV_METHOD=BATCHED ARMCI_RMA_ATOMICITY=1 \
    ARMCI_FLUSH_REQUEST_ATOMICS=0 ARMCI_USE_REQUEST_ATOMICS=0 \
    ARMCI_ACCUMULATE_GRANULARITY=1
run_tuple alternate_extreme \
    ARMCI_USE_WIN_ALLOCATE=0 ARMCI_STRIDED_METHOD=IOV \
    ARMCI_IOV_METHOD=DIRECT ARMCI_RMA_ATOMICITY=0 \
    ARMCI_FLUSH_REQUEST_ATOMICS=0 ARMCI_USE_REQUEST_ATOMICS=1

if awk -F '\t' 'NR > 1 && ($6 == "FAIL") {found=1} END {exit !found}' "${summary}"; then
    echo "profile_result=FAIL"
    exit 1
fi

echo "profile_result=PASS"
