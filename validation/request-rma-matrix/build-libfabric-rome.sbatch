#!/bin/bash

#SBATCH --partition=rome
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --constraint=x86_64

set -euo pipefail

: "${LIBFABRIC_SOURCE:?set LIBFABRIC_SOURCE to the libfabric source tree}"
: "${LIBFABRIC_INSTALL:?set LIBFABRIC_INSTALL to a shared installation path}"

source_dir=$(cd "$LIBFABRIC_SOURCE" && pwd -P)
install_parent=$(dirname "$LIBFABRIC_INSTALL")
mkdir -p "$install_parent"
install_parent=$(cd "$install_parent" && pwd -P)
install_dir=$install_parent/$(basename "$LIBFABRIC_INSTALL")
build_dir=/tmp/libfabric-rdma114-${SLURM_JOB_ID:-$$}

if [[ -e $install_dir ]]; then
    echo "refusing to overwrite existing install directory: $install_dir" >&2
    exit 1
fi

mkdir -p "$build_dir" "$install_dir/logs"
git -C "$source_dir" archive HEAD | tar -x -C "$build_dir"

cd "$build_dir"
./autogen.sh >"$install_dir/logs/autogen.log" 2>&1
./configure --prefix="$install_dir" --disable-efa CC=gcc CFLAGS='-O2 -g' \
    >"$install_dir/logs/configure.log" 2>&1
make -j "${BUILD_JOBS:-16}" >"$install_dir/logs/build.log" 2>&1
make install >"$install_dir/logs/install.log" 2>&1

{
    echo "host=$(hostname)"
    echo "source_commit=$(git -C "$source_dir" rev-parse HEAD)"
    echo "rdma_core=$(rpm -q rdma-core libibverbs 2>/dev/null | tr '\n' ' ')"
    ldd "$install_dir/lib/libfabric.so"
    LD_LIBRARY_PATH=$install_dir/lib "$install_dir/bin/fi_info" -p tcp | head -40
    LD_LIBRARY_PATH=$install_dir/lib "$install_dir/bin/fi_info" -p 'verbs;ofi_rxm' | head -80
} >"$install_dir/logs/verification.log" 2>&1

echo "installed Rome-compatible libfabric in $install_dir"
echo "logs are in $install_dir/logs"
