#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

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

    const int request_count = argc > 1 ? atoi(argv[1]) : 9;
    const int block_count = argc > 2 ? atoi(argv[2]) : 10;
    const int block_length = argc > 3 ? atoi(argv[3]) : 20;
    const int free_before_wait = argc > 4 ? atoi(argv[4]) : 1;
    if (request_count <= 0 || block_count <= 0 || block_length <= 0) {
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    const int stride = block_length + 3;
    const int request_span = block_count * stride;
    const int element_count = request_count * request_span;
    double *base = NULL;
    MPI_Win window = MPI_WIN_NULL;
    const MPI_Aint bytes = rank == 0
                               ? (MPI_Aint)element_count * (MPI_Aint)sizeof(*base)
                               : 0;
    check(MPI_Win_allocate(bytes, sizeof(*base), MPI_INFO_NULL, MPI_COMM_WORLD,
                           &base, &window),
          "MPI_Win_allocate");
    check(MPI_Win_set_errhandler(window, MPI_ERRORS_RETURN),
          "MPI_Win_set_errhandler");
    if (rank == 0) {
        for (int i = 0; i < element_count; ++i) base[i] = 0.0;
    }

    check(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");
    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier before Rput");

    if (rank == 1) {
        double *source = malloc((size_t)element_count * sizeof(*source));
        int *displacements = malloc((size_t)block_count * sizeof(*displacements));
        MPI_Request *requests = malloc((size_t)request_count * sizeof(*requests));
        MPI_Datatype *origin_types =
            malloc((size_t)request_count * sizeof(*origin_types));
        MPI_Datatype *target_types =
            malloc((size_t)request_count * sizeof(*target_types));
        if (source == NULL || displacements == NULL || requests == NULL ||
            origin_types == NULL || target_types == NULL) {
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
        for (int i = 0; i < element_count; ++i) source[i] = (double)(i + 1);

        for (int request = 0; request < request_count; ++request) {
            for (int block = 0; block < block_count; ++block) {
                displacements[block] = request * request_span + block * stride;
            }
            check(MPI_Type_create_indexed_block(block_count, block_length,
                                                displacements, MPI_DOUBLE,
                                                &origin_types[request]),
                  "MPI_Type_create_indexed_block origin");
            check(MPI_Type_create_indexed_block(block_count, block_length,
                                                displacements, MPI_DOUBLE,
                                                &target_types[request]),
                  "MPI_Type_create_indexed_block target");
            check(MPI_Type_commit(&origin_types[request]), "MPI_Type_commit origin");
            check(MPI_Type_commit(&target_types[request]), "MPI_Type_commit target");
            check(MPI_Rput(source, 1, origin_types[request], 0, 0, 1,
                           target_types[request], window, &requests[request]),
                  "MPI_Rput");
            if (free_before_wait) {
                check(MPI_Type_free(&origin_types[request]), "MPI_Type_free origin");
                check(MPI_Type_free(&target_types[request]), "MPI_Type_free target");
            }
        }

        check(MPI_Waitall(request_count, requests, MPI_STATUSES_IGNORE),
              "MPI_Waitall");
        check(MPI_Win_flush(0, window), "MPI_Win_flush");
        if (!free_before_wait) {
            for (int request = 0; request < request_count; ++request) {
                check(MPI_Type_free(&origin_types[request]), "MPI_Type_free origin");
                check(MPI_Type_free(&target_types[request]), "MPI_Type_free target");
            }
        }
        free(target_types);
        free(origin_types);
        free(requests);
        free(displacements);
        free(source);
    }

    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier after Rput");
    check(MPI_Win_unlock_all(window), "MPI_Win_unlock_all");
    if (rank == 0) {
        int wrong = 0;
        for (int request = 0; request < request_count; ++request) {
            for (int block = 0; block < block_count; ++block) {
                const int first = request * request_span + block * stride;
                for (int element = 0; element < block_length; ++element) {
                    const int index = first + element;
                    if (base[index] != (double)(index + 1)) ++wrong;
                }
            }
        }
        printf("requests=%d blocks=%d length=%d free_before_wait=%d wrong=%d %s\n",
               request_count, block_count, block_length, free_before_wait, wrong,
               wrong == 0 ? "OK" : "WRONG");
    }
    check(MPI_Win_free(&window), "MPI_Win_free");
    check(MPI_Finalize(), "MPI_Finalize");
    return 0;
}
