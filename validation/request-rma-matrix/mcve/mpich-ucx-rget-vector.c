#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

#ifndef USE_GET
#define USE_GET 0
#endif

#ifndef ORIGIN_VECTOR
#define ORIGIN_VECTOR 1
#endif

#ifndef TARGET_VECTOR
#define TARGET_VECTOR 1
#endif

enum {
    elements = 64,
    blocks = 16,
    block_length = 2,
    stride = 4
};

static void check_mpi(int error, const char *operation)
{
    if (error != MPI_SUCCESS) {
        char text[MPI_MAX_ERROR_STRING];
        int length = 0;

        MPI_Error_string(error, text, &length);
        fprintf(stderr, "%s failed: %.*s\n", operation, length, text);
        MPI_Abort(MPI_COMM_WORLD, error);
    }
}

#if ORIGIN_VECTOR || TARGET_VECTOR
static MPI_Datatype make_vector(void)
{
    MPI_Datatype datatype;

    check_mpi(
        MPI_Type_vector(blocks, block_length, stride, MPI_DOUBLE, &datatype),
        "MPI_Type_vector");
    check_mpi(MPI_Type_commit(&datatype), "MPI_Type_commit");
    return datatype;
}
#endif

int main(int argc, char **argv)
{
    check_mpi(MPI_Init(&argc, &argv), "MPI_Init");

    int rank;
    int size;
    check_mpi(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    check_mpi(MPI_Comm_size(MPI_COMM_WORLD, &size), "MPI_Comm_size");
    if (size != 2) {
        if (rank == 0) {
            fprintf(stderr, "run with exactly two MPI processes\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    double *window_buffer = NULL;
    MPI_Win window;
    check_mpi(
        MPI_Win_allocate(
            elements * (MPI_Aint)sizeof(double),
            sizeof(double),
            MPI_INFO_NULL,
            MPI_COMM_WORLD,
            &window_buffer,
            &window),
        "MPI_Win_allocate");
    check_mpi(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");

    double source[elements];
    double result[elements];
    for (int i = 0; i < elements; ++i) {
        source[i] = i;
        result[i] = -1.0;
    }

    if (rank == 0) {
        check_mpi(
            MPI_Put(
                source,
                elements,
                MPI_DOUBLE,
                1,
                0,
                elements,
                MPI_DOUBLE,
                window),
            "MPI_Put");
        check_mpi(MPI_Win_flush(1, window), "MPI_Win_flush");
    }
    check_mpi(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");

    int errors = 0;
    if (rank == 0) {
#if ORIGIN_VECTOR
        MPI_Datatype origin_type = make_vector();
        const int origin_count = 1;
#else
        MPI_Datatype origin_type = MPI_DOUBLE;
        const int origin_count = blocks * block_length;
#endif
#if TARGET_VECTOR
        MPI_Datatype target_type = make_vector();
        const int target_count = 1;
#else
        MPI_Datatype target_type = MPI_DOUBLE;
        const int target_count = blocks * block_length;
#endif

#if USE_GET
        check_mpi(
            MPI_Get(
                result,
                origin_count,
                origin_type,
                1,
                0,
                target_count,
                target_type,
                window),
            "MPI_Get");
        check_mpi(MPI_Win_flush(1, window), "MPI_Win_flush");
#else
        MPI_Request request;
        check_mpi(
            MPI_Rget(
                result,
                origin_count,
                origin_type,
                1,
                0,
                target_count,
                target_type,
                window,
                &request),
            "MPI_Rget");
        check_mpi(MPI_Wait(&request, MPI_STATUS_IGNORE), "MPI_Wait");
#endif

#if ORIGIN_VECTOR
        check_mpi(MPI_Type_free(&origin_type), "MPI_Type_free(origin)");
#endif
#if TARGET_VECTOR
        check_mpi(MPI_Type_free(&target_type), "MPI_Type_free(target)");
#endif

        for (int block = 0; block < blocks; ++block) {
            for (int element = 0; element < block_length; ++element) {
#if ORIGIN_VECTOR
                const int result_index = block * stride + element;
#else
                const int result_index = block * block_length + element;
#endif
#if TARGET_VECTOR
                const int expected_index = block * stride + element;
#else
                const int expected_index = block * block_length + element;
#endif
                if (result[result_index] != source[expected_index]) {
                    if (errors < 8) {
                        fprintf(
                            stderr,
                            "index=%d expected=%.1f actual=%.1f\n",
                            result_index,
                            source[expected_index],
                            result[result_index]);
                    }
                    ++errors;
                }
            }
        }
    }

    check_mpi(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    check_mpi(MPI_Win_free(&window), "MPI_Win_free");

    if (rank == 0) {
        printf(
            "MPI_%s with %s/%s datatypes: %s\n",
            USE_GET ? "Get" : "Rget",
            ORIGIN_VECTOR ? "vector" : "MPI_DOUBLE",
            TARGET_VECTOR ? "vector" : "MPI_DOUBLE",
            errors == 0 ? "PASS" : "FAIL");
    }

    check_mpi(MPI_Finalize(), "MPI_Finalize");
    return errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
