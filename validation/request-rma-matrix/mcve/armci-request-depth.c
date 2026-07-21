#include <armci.h>
#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

#define REQUEST_COUNT 512

static void check(int rc, const char *operation)
{
    if (rc != 0) {
        fprintf(stderr, "%s failed with status %d\n", operation, rc);
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    ARMCI_Init();

    int rank;
    int size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (size < 2) MPI_Abort(MPI_COMM_WORLD, 1);

    int **base = malloc((size_t)size * sizeof(*base));
    if (base == NULL) MPI_Abort(MPI_COMM_WORLD, 1);
    check(ARMCI_Malloc((void **)base, REQUEST_COUNT * sizeof(**base)),
          "ARMCI_Malloc");

    ARMCI_Access_begin(base[rank]);
    for (int i = 0; i < REQUEST_COUNT; ++i) base[rank][i] = -1;
    ARMCI_Access_end(base[rank]);
    ARMCI_Barrier();

    if (rank == 1) {
        int values[REQUEST_COUNT];
        armci_hdl_t handle;
        ARMCI_INIT_HANDLE(&handle);

        for (int i = 0; i < REQUEST_COUNT; ++i) {
            values[i] = i;
            check(ARMCI_NbPut(&values[i], &base[0][i], sizeof(values[i]), 0,
                              &handle),
                  "ARMCI_NbPut");
        }
        check(ARMCI_Wait(&handle), "ARMCI_Wait");
    }

    ARMCI_Barrier();
    if (rank == 0) {
        ARMCI_Access_begin(base[rank]);
        for (int i = 0; i < REQUEST_COUNT; ++i) {
            if (base[rank][i] != i) {
                fprintf(stderr, "element %d: expected %d, found %d\n", i, i,
                        base[rank][i]);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        ARMCI_Access_end(base[rank]);
    }

    ARMCI_Barrier();
    check(ARMCI_Free(base[rank]), "ARMCI_Free");
    free(base);

    ARMCI_Finalize();
    MPI_Finalize();
    return 0;
}
