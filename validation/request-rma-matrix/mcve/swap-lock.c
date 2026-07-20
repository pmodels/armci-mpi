#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCKED (-1)

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

static int swap_value(const char *mode, int value, int *result, MPI_Win window)
{
    if (strcmp(mode, "fetch") == 0) {
        int rc = MPI_Fetch_and_op(&value, result, MPI_INT, 0, 0, MPI_REPLACE,
                                  window);
        if (rc == MPI_SUCCESS) rc = MPI_Win_flush(0, window);
        return rc;
    }
    if (strcmp(mode, "getacc") == 0) {
        int rc = MPI_Get_accumulate(&value, 1, MPI_INT, result, 1, MPI_INT, 0, 0,
                                    1, MPI_INT, MPI_REPLACE, window);
        if (rc == MPI_SUCCESS) rc = MPI_Win_flush(0, window);
        return rc;
    }

    MPI_Request request = MPI_REQUEST_NULL;
    int rc = MPI_Rget_accumulate(&value, 1, MPI_INT, result, 1, MPI_INT, 0, 0,
                                 1, MPI_INT, MPI_REPLACE, window, &request);
    if (rc == MPI_SUCCESS) rc = MPI_Wait(&request, MPI_STATUS_IGNORE);
    if (rc == MPI_SUCCESS && strcmp(mode, "rget-flush") == 0) {
        rc = MPI_Win_flush(0, window);
    }
    return rc;
}

int main(int argc, char **argv)
{
    check(MPI_Init(&argc, &argv), "MPI_Init");

    int rank = -1;
    int size = 0;
    check(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "MPI_Comm_rank");
    check(MPI_Comm_size(MPI_COMM_WORLD, &size), "MPI_Comm_size");
    const char *mode = argc > 1 ? argv[1] : "rget";
    const int loops = argc > 2 ? atoi(argv[2]) : 200;
    const char *window_mode = argc > 3 ? argv[3] : "allocate";
    if (strcmp(mode, "fetch") != 0 && strcmp(mode, "getacc") != 0 &&
        strcmp(mode, "rget") != 0 && strcmp(mode, "rget-flush") != 0) {
        if (rank == 0) fprintf(stderr, "invalid mode: %s\n", mode);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }
    if (strcmp(window_mode, "allocate") != 0 &&
        strcmp(window_mode, "create") != 0) {
        if (rank == 0) fprintf(stderr, "invalid window mode: %s\n", window_mode);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    int *base = NULL;
    MPI_Win window = MPI_WIN_NULL;
    const MPI_Aint bytes = rank == 0 ? (MPI_Aint)sizeof(*base) : 0;
    if (strcmp(window_mode, "allocate") == 0) {
        check(MPI_Win_allocate(bytes, sizeof(*base), MPI_INFO_NULL, MPI_COMM_WORLD,
                               &base, &window),
              "MPI_Win_allocate");
    } else {
        if (rank == 0) {
            base = malloc(sizeof(*base));
            if (base == NULL) MPI_Abort(MPI_COMM_WORLD, 2);
        }
        check(MPI_Win_create(base, bytes, sizeof(*base), MPI_INFO_NULL,
                             MPI_COMM_WORLD, &window),
              "MPI_Win_create");
    }
    check(MPI_Win_set_errhandler(window, MPI_ERRORS_RETURN),
          "MPI_Win_set_errhandler");
    if (rank == 0) *base = 0;
    check(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");
    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier before swaps");

    for (int iteration = 0; iteration < loops; ++iteration) {
        int value = LOCKED;
        do {
            int result = 0;
            check(swap_value(mode, value, &result, window), "atomic acquire");
            value = result;
        } while (value == LOCKED);
        ++value;
        int result = 0;
        check(swap_value(mode, value, &result, window), "atomic release");
    }

    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier after swaps");
    check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    if (rank == 0) {
        const int expected = loops * size;
        printf("mode=%s loops=%d window=%s final=%d expected=%d %s\n", mode,
               loops, window_mode, *base, expected,
               *base == expected ? "OK" : "WRONG");
    }
    check(MPI_Win_free(&window), "MPI_Win_free");
    if (strcmp(window_mode, "create") == 0) free(base);
    check(MPI_Finalize(), "MPI_Finalize");
    return 0;
}
