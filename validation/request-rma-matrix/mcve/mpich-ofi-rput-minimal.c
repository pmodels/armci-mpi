#include <mpi.h>

#include <stdio.h>

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

    double *target = NULL;
    MPI_Win window;
    const MPI_Aint bytes = rank == 0 ? 40 * (MPI_Aint)sizeof(*target) : 0;
    check(MPI_Win_allocate(bytes, sizeof(*target), MPI_INFO_NULL, MPI_COMM_WORLD,
                           &target, &window),
          "MPI_Win_allocate");

    check(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");
    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");

    if (rank == 1) {
        double origin[41];
        for (int i = 0; i < 20; ++i) {
            origin[i] = i;
            origin[21 + i] = 20 + i;
        }

        const int origin_displacements[2] = {0, 21};
        const int target_displacements[2] = {0, 20};
        MPI_Datatype origin_type;
        MPI_Datatype target_type;
        check(MPI_Type_create_indexed_block(2, 20, origin_displacements,
                                            MPI_DOUBLE, &origin_type),
              "MPI_Type_create_indexed_block origin");
        check(MPI_Type_create_indexed_block(2, 20, target_displacements,
                                            MPI_DOUBLE, &target_type),
              "MPI_Type_create_indexed_block target");
        check(MPI_Type_commit(&origin_type), "MPI_Type_commit origin");
        check(MPI_Type_commit(&target_type), "MPI_Type_commit target");

#ifdef USE_NONREQUEST_PUT
        check(MPI_Put(origin, 1, origin_type, 0, 0, 1, target_type, window),
              "MPI_Put");
#else
        MPI_Request request;
        check(MPI_Rput(origin, 1, origin_type, 0, 0, 1, target_type, window,
                       &request),
              "MPI_Rput");
        check(MPI_Wait(&request, MPI_STATUS_IGNORE), "MPI_Wait");
#endif
        check(MPI_Win_flush(0, window), "MPI_Win_flush");
        check(MPI_Type_free(&target_type), "MPI_Type_free target");
        check(MPI_Type_free(&origin_type), "MPI_Type_free origin");
    }

    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier");
    check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    check(MPI_Win_free(&window), "MPI_Win_free");
    check(MPI_Finalize(), "MPI_Finalize");
    return 0;
}
