#! /bin/sh

# Exit on error
set -ev

os=`uname`
TRAVIS_ROOT="$1"
MPI_IMPL="$2"

# Environment variables
export CFLAGS="-std=c99"
#export MPICH_CC=$CC
export MPICC=mpicc

case "$os" in
    Darwin)
        ;;
    Linux)
       export PATH=$TRAVIS_ROOT/mpich/bin:$PATH
       export PATH=$TRAVIS_ROOT/open-mpi/bin:$PATH
       ;;
esac

# Capture details of build
case "$MPI_IMPL" in
    mpich)
        mpichversion
        mpicc -show
        ;;
    openmpi)
        # this is missing with Mac build it seems
        #ompi_info --arch --config
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
        make check
        ;;
    *)
        # MPICH and derivatives support RMA datatypes...
        make check
        ;;
esac
