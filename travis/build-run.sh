#! /bin/sh

# Exit on error
set -ev

MPI_IMPL="$1"

# Environment variables
export CFLAGS="-std=c99"
#export MPICH_CC=$CC
export MPICC=mpicc

# Capture details of build
case "$MPI_IMPL" in
    mpich*)
        mpichversion
        mpicc -show
        ;;
    openmpi)
        ompi_info --arch --config --path
        mpicc --showme:command
        ;;
esac

# Configure and build
./autogen.sh
./configure --disable-static

# Run unit tests
export ARMCI_VERBOSE=1
case "$MPI_IMPL" in
    openmpi)
        # OpenMPI RMA datatype support was (is?) broken...
        export ARMCI_STRIDED_METHOD=IOV
        export ARMCI_IOV_METHOD=BATCHED
        make check
        ;;
    *)
        # MPICH and derivatives support RMA datatypes...
        make check
        ;;
esac
