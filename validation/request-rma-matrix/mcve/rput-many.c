#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

/*
 * Open MPI 5 osc/ucx creates a nonblocking UCX worker flush for every
 * MPI_Rput request.  UCX holds an endpoint reference for each unfinished
 * worker flush, so request 255 aborts at the uint8_t endpoint-refcount limit.
 * Request counts through 254 are the adjacent working control.
 */

static void check(int rc, const char *operation)
{
    if (rc != MPI_SUCCESS) {
        char error[MPI_MAX_ERROR_STRING];
        int length = 0;
        MPI_Error_string(rc, error, &length);
        fprintf(stderr, "%s failed: %.*s\n", operation, length, error);
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
}

int main(int argc, char **argv)
{
    check(MPI_Init(&argc, &argv), "MPI_Init");

    int rank = -1;
    int size = 0;
    check(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    check(MPI_Comm_size(MPI_COMM_WORLD, &size), "MPI_Comm_size");
    if (size != 2) {
        if (rank == 0) fprintf(stderr, "this test requires exactly two ranks\n");
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    const int count = argc > 1 ? atoi(argv[1]) : 256;
    if (count <= 0) MPI_Abort(MPI_COMM_WORLD, 2);

    int *base = NULL;
    MPI_Win window = MPI_WIN_NULL;
    const MPI_Aint bytes = rank == 0 ? (MPI_Aint)sizeof(*base) : 0;
    check(MPI_Win_allocate(bytes, sizeof(*base), MPI_INFO_NULL, MPI_COMM_WORLD,
                           &base, &window),
          "MPI_Win_allocate");
    check(MPI_Win_set_errhandler(window, MPI_ERRORS_RETURN),
          "MPI_Win_set_errhandler");
    if (rank == 0) *base = 0;

    check(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");
    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier before Rput");

    if (rank == 1) {
        int *values = malloc((size_t)count * sizeof(*values));
        MPI_Request *requests = malloc((size_t)count * sizeof(*requests));
        if (values == NULL || requests == NULL) MPI_Abort(MPI_COMM_WORLD, 2);

        for (int i = 0; i < count; ++i) {
            values[i] = i + 1;
            check(MPI_Rput(&values[i], 1, MPI_INT, 0, 0, 1, MPI_INT, window,
                           &requests[i]),
                  "MPI_Rput");
        }
        check(MPI_Waitall(count, requests, MPI_STATUSES_IGNORE), "MPI_Waitall");
        check(MPI_Win_flush(0, window), "MPI_Win_flush");
        free(requests);
        free(values);
    }

    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier after Rput");
    check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    if (rank == 0) printf("count=%d completed value=%d OK\n", count, *base);
    check(MPI_Win_free(&window), "MPI_Win_free");
    check(MPI_Finalize(), "MPI_Finalize");
    return 0;
}
