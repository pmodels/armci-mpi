#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
    const int split_origin = argc > 5 ? atoi(argv[5]) : 0;
    const int stride = block_length;
    const int origin_stride = argc > 6 ? atoi(argv[6]) : stride;
    if (request_count <= 0 || block_count <= 0 || block_length <= 0 ||
        origin_stride < block_length) {
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    const int request_span = block_count * stride;
    const int target_element_count = request_count * request_span;
    const int origin_element_count =
        request_count * block_count * origin_stride + block_length;
    const int source_element_count =
        origin_element_count > target_element_count
            ? origin_element_count
            : target_element_count;
    double *base = NULL;
    MPI_Win window = MPI_WIN_NULL;
    const MPI_Aint bytes = rank == 0
                               ? (MPI_Aint)target_element_count *
                                     (MPI_Aint)sizeof(*base)
                               : 0;
    check(MPI_Win_allocate(bytes, sizeof(*base), MPI_INFO_NULL, MPI_COMM_WORLD,
                           &base, &window),
          "MPI_Win_allocate");
    check(MPI_Win_set_errhandler(window, MPI_ERRORS_RETURN),
          "MPI_Win_set_errhandler");
    if (rank == 0) {
        for (int i = 0; i < target_element_count; ++i) base[i] = 0.0;
    }

    check(MPI_Win_lock_all(0, window), "MPI_Win_lock_all");
    check(MPI_Barrier(MPI_COMM_WORLD), "MPI_Barrier before Rput");

    if (rank == 1) {
        double *source = split_origin
                             ? NULL
                             : malloc((size_t)source_element_count * sizeof(*source));
        double **segments = split_origin
                                ? malloc((size_t)request_count *
                                         (size_t)block_count * sizeof(*segments))
                                : NULL;
        int *displacements = malloc((size_t)block_count * sizeof(*displacements));
        int *target_displacements =
            malloc((size_t)block_count * sizeof(*target_displacements));
        MPI_Request *requests = malloc((size_t)request_count * sizeof(*requests));
        MPI_Datatype *origin_types =
            malloc((size_t)request_count * sizeof(*origin_types));
        MPI_Datatype *target_types =
            malloc((size_t)request_count * sizeof(*target_types));
        if ((!split_origin && source == NULL) ||
            (split_origin && segments == NULL) || displacements == NULL ||
            target_displacements == NULL || requests == NULL ||
            origin_types == NULL || target_types == NULL) {
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
        if (split_origin) {
            for (int request = 0; request < request_count; ++request) {
                for (int block = 0; block < block_count; ++block) {
                    const int segment = request * block_count + block;
                    segments[segment] = malloc((size_t)block_length *
                                               sizeof(*segments[segment]));
                    if (segments[segment] == NULL) MPI_Abort(MPI_COMM_WORLD, 2);
                    const int first = request * request_span + block * stride;
                    for (int element = 0; element < block_length; ++element) {
                        segments[segment][element] = (double)(first + element + 1);
                    }
                }
            }
        } else {
            for (int i = 0; i < source_element_count; ++i) source[i] = 0.0;
        }

        for (int request = 0; request < request_count; ++request) {
            double *origin_base = source;
            if (split_origin) {
                origin_base = segments[request * block_count];
                for (int block = 1; block < block_count; ++block) {
                    double *candidate = segments[request * block_count + block];
                    if ((uintptr_t)candidate < (uintptr_t)origin_base) {
                        origin_base = candidate;
                    }
                }
            }
            for (int block = 0; block < block_count; ++block) {
                target_displacements[block] =
                    request * request_span + block * stride;
                if (split_origin) {
                    double *segment = segments[request * block_count + block];
                    const uintptr_t bytes =
                        (uintptr_t)segment - (uintptr_t)origin_base;
                    if (bytes % sizeof(*segment) != 0 ||
                        bytes / sizeof(*segment) > INT32_MAX) {
                        MPI_Abort(MPI_COMM_WORLD, 2);
                    }
                    displacements[block] = (int)(bytes / sizeof(*segment));
                } else {
                    displacements[block] =
                        (request * block_count + block) * origin_stride;
                    const int first = request * request_span + block * stride;
                    for (int element = 0; element < block_length; ++element) {
                        source[displacements[block] + element] =
                            (double)(first + element + 1);
                    }
                }
            }
            check(MPI_Type_create_indexed_block(block_count, block_length,
                                                displacements, MPI_DOUBLE,
                                                &origin_types[request]),
                  "MPI_Type_create_indexed_block origin");
            check(MPI_Type_create_indexed_block(block_count, block_length,
                                                target_displacements, MPI_DOUBLE,
                                                &target_types[request]),
                  "MPI_Type_create_indexed_block target");
            check(MPI_Type_commit(&origin_types[request]), "MPI_Type_commit origin");
            check(MPI_Type_commit(&target_types[request]), "MPI_Type_commit target");
            check(MPI_Rput(origin_base, 1, origin_types[request], 0, 0, 1,
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
        free(target_displacements);
        free(displacements);
        if (split_origin) {
            for (int segment = 0; segment < request_count * block_count; ++segment) {
                free(segments[segment]);
            }
        }
        free(segments);
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
        printf("requests=%d blocks=%d length=%d free_before_wait=%d "
               "split_origin=%d origin_stride=%d wrong=%d %s\n",
               request_count, block_count, block_length, free_before_wait,
               split_origin, origin_stride, wrong, wrong == 0 ? "OK" : "WRONG");
    }
    check(MPI_Win_free(&window), "MPI_Win_free");
    check(MPI_Finalize(), "MPI_Finalize");
    return 0;
}
