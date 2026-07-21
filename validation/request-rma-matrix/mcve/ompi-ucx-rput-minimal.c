#include <mpi.h>

#include <stdio.h>

#ifndef REQUEST_COUNT
#define REQUEST_COUNT 255
#endif

static void check(int rc, const char *name)
{
    if (rc != MPI_SUCCESS) {
        fprintf(stderr, "%s failed\n", name);
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
}

int main(int argc, char **argv)
{
    check(MPI_Init(&argc, &argv), "MPI_Init");

    int rank;
    int size;
    check(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    check(MPI_Comm_size(MPI_COMM_WORLD, &size), "MPI_Comm_size");
    if (size != 2) MPI_Abort(MPI_COMM_WORLD, 1);

    int *target = NULL;
    MPI_Win window;
    const MPI_Aint bytes = rank == 0 ? (MPI_Aint)sizeof(*target) : 0;
    check(MPI_Win_allocate(bytes, sizeof(*target), MPI_INFO_NULL, MPI_COMM_WORLD,
                           &target, &window),
          "MPI_Win_allocate");
    if (rank == 0) *target = 0;

    check(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");
    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");

    if (rank == 1) {
        int values[REQUEST_COUNT];
        MPI_Request requests[REQUEST_COUNT];
        for (int i = 0; i < REQUEST_COUNT; ++i) {
            values[i] = i;
            check(MPI_Rput(&values[i], 1, MPI_INT, 0, 0, 1, MPI_INT, window,
                           &requests[i]),
                  "MPI_Rput");
        }
        check(MPI_Waitall(REQUEST_COUNT, requests, MPI_STATUSES_IGNORE),
              "MPI_Waitall");
    }

    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");
    check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    check(MPI_Win_free(&window), "MPI_Win_free");
    check(MPI_Finalize(), "MPI_Finalize");
    return 0;
}
