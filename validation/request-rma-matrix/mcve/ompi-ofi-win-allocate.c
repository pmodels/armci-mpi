#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void mpi_check(int error, const char *operation)
{
    if (error != MPI_SUCCESS) {
        char message[MPI_MAX_ERROR_STRING];
        int length = 0;

        MPI_Error_string(error, message, &length);
        fprintf(stderr, "%s failed: %.*s\n", operation, length, message);
        MPI_Abort(MPI_COMM_WORLD, error);
    }
}

int main(int argc, char **argv)
{
    mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    int rank = -1;
    int nranks = 0;

    mpi_check(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    mpi_check(MPI_Comm_size(MPI_COMM_WORLD, &nranks), "MPI_Comm_size");

    if (argc != 2 ||
        (strcmp(argv[1], "allocate") != 0 &&
         strcmp(argv[1], "create") != 0)) {
        if (rank == 0) {
            fprintf(stderr, "usage: %s {allocate|create}\n", argv[0]);
        }
        MPI_Finalize();
        return 2;
    }

    const int use_allocate = strcmp(argv[1], "allocate") == 0;
    const MPI_Aint bytes = sizeof(double);
    MPI_Win window = MPI_WIN_NULL;
    double *target = NULL;

    if (use_allocate) {
        mpi_check(
            MPI_Win_allocate(
                bytes,
                1,
                MPI_INFO_NULL,
                MPI_COMM_WORLD,
                &target,
                &window),
            "MPI_Win_allocate");
    } else {
        mpi_check(
            MPI_Alloc_mem(bytes, MPI_INFO_NULL, &target),
            "MPI_Alloc_mem");
        mpi_check(
            MPI_Win_create(
                target,
                bytes,
                1,
                MPI_INFO_NULL,
                MPI_COMM_WORLD,
                &window),
            "MPI_Win_create");
    }

    mpi_check(
        MPI_Win_lock_all(MPI_MODE_NOCHECK, window),
        "MPI_Win_lock_all");
    *target = 0.0;
    mpi_check(MPI_Win_sync(window), "MPI_Win_sync");
    mpi_check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");

    const double source = 6.0 * (rank + 1);

    mpi_check(
        MPI_Accumulate(
            &source,
            1,
            MPI_DOUBLE,
            0,
            0,
            1,
            MPI_DOUBLE,
            MPI_SUM,
            window),
        "MPI_Accumulate");
    mpi_check(MPI_Win_flush_all(window), "MPI_Win_flush_all");
    mpi_check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");
    mpi_check(MPI_Win_sync(window), "MPI_Win_sync");

    int errors = 0;

    if (rank == 0) {
        const double rank_sum = nranks * (nranks + 1) / 2.0;
        const double expected = 6.0 * rank_sum;

        if (*target != expected) {
            fprintf(
                stderr,
                "target = %.17g, expected %.17g\n",
                *target,
                expected);
            ++errors;
        }
    }

    mpi_check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    mpi_check(MPI_Win_free(&window), "MPI_Win_free");
    if (!use_allocate) {
        mpi_check(MPI_Free_mem(target), "MPI_Free_mem");
    }

    int total_errors = 0;

    mpi_check(
        MPI_Allreduce(
            &errors,
            &total_errors,
            1,
            MPI_INT,
            MPI_SUM,
            MPI_COMM_WORLD),
        "MPI_Allreduce");
    if (rank == 0) {
        printf(
            "window=%s: %s\n",
            argv[1],
            total_errors == 0 ? "PASS" : "FAIL");
    }

    mpi_check(MPI_Finalize(), "MPI_Finalize");
    return total_errors == 0 ? 0 : 1;
}
