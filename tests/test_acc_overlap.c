#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mpi.h>
#include <armci.h>

#include "armci_acc_test_types.h"

#define TARGET_ELEMENTS 32
#define PATCH_ELEMENTS 16
#define ITERATIONS 64

static unsigned int next_random(unsigned int *state)
{
    unsigned int value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int target_index(int method, int element)
{
    if (method == 0) {
        return element;
    }
    if (method == 1) {
        return 2 * element;
    }
    return 8 + element;
}

static int issue_accumulate(
    int method,
    const armci_acc_test_type_t *type,
    void *scale,
    void *source,
    void *target,
    int proc)
{
    const int bytes = (int)type->element_size;

    if (method == 0) {
        return ARMCI_Acc(
            type->datatype,
            scale,
            source,
            target,
            PATCH_ELEMENTS * bytes,
            proc);
    }

    if (method == 1) {
        const int source_stride = bytes;
        const int target_stride = 2 * bytes;
        const int count[2] = {bytes, PATCH_ELEMENTS};
        return ARMCI_AccS(
            type->datatype,
            scale,
            source,
            (int *)&source_stride,
            target,
            (int *)&target_stride,
            (int *)count,
            1,
            proc);
    }

    void *source_ptrs[PATCH_ELEMENTS];
    void *target_ptrs[PATCH_ELEMENTS];
    for (int element = 0; element < PATCH_ELEMENTS; ++element) {
        source_ptrs[element] = (char *)source + element * bytes;
        target_ptrs[element] =
            (char *)target + target_index(method, element) * bytes;
    }
    armci_giov_t iov = {
        source_ptrs,
        target_ptrs,
        bytes,
        PATCH_ELEMENTS
    };
    return ARMCI_AccV(type->datatype, scale, &iov, 1, proc);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    ARMCI_Init();

    int rank;
    int nranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    void **base_ptrs = malloc((size_t)nranks * sizeof(*base_ptrs));
    const size_t allocation_size =
        TARGET_ELEMENTS * 2 * sizeof(double);
    ARMCI_Malloc(base_ptrs, (armci_size_t)allocation_size);

    int errors = 0;
    for (size_t type_index = 0;
         type_index < ARMCI_ACC_TEST_TYPE_COUNT;
         ++type_index) {
        const armci_acc_test_type_t *type =
            &armci_acc_test_types[type_index];
        const size_t patch_bytes =
            PATCH_ELEMENTS * type->element_size;
        void *sources[3] = {
            malloc(patch_bytes),
            malloc(patch_bytes),
            malloc(patch_bytes)
        };

        for (int method = 0; method < 3; ++method) {
            const long value = (long)(method + 1) * (rank + 1);
            for (int element = 0; element < PATCH_ELEMENTS; ++element) {
                armci_acc_test_set(
                    type,
                    sources[method],
                    (size_t)element,
                    value);
            }
        }

        if (rank == 0) {
            ARMCI_Access_begin(base_ptrs[0]);
            memset(base_ptrs[0], 0, allocation_size);
            ARMCI_Access_end(base_ptrs[0]);
        }
        ARMCI_Barrier();

        armci_acc_test_scale_t scale_storage;
        void * const scale =
            armci_acc_test_scale(type, &scale_storage, 2);
        unsigned int random_state =
            0x9e3779b9U ^ (unsigned int)rank ^
            ((unsigned int)type_index << 16);

        for (int iteration = 0; iteration < ITERATIONS; ++iteration) {
            int order[3] = {0, 1, 2};
            for (int index = 2; index > 0; --index) {
                const int other =
                    (int)(next_random(&random_state) %
                          (unsigned int)(index + 1));
                const int temporary = order[index];
                order[index] = order[other];
                order[other] = temporary;
            }

            for (int index = 0; index < 3; ++index) {
                const int method = order[index];
                const int status = issue_accumulate(
                    method,
                    type,
                    scale,
                    sources[method],
                    base_ptrs[0],
                    0);
                if (status != 0) {
                    fprintf(
                        stderr,
                        "rank %d: %s accumulate method %d returned %d\n",
                        rank,
                        type->name,
                        method,
                        status);
                    MPI_Abort(MPI_COMM_WORLD, status);
                }
            }
        }

        ARMCI_Barrier();
        if (rank == 0) {
            const long rank_sum =
                (long)nranks * (nranks + 1) / 2;
            ARMCI_Access_begin(base_ptrs[0]);
            for (int element = 0; element < TARGET_ELEMENTS; ++element) {
                long expected = 0;
                if (element < PATCH_ELEMENTS) {
                    expected += 2 * rank_sum;
                }
                if ((element % 2) == 0) {
                    expected += 4 * rank_sum;
                }
                if (element >= 8 && element < 8 + PATCH_ELEMENTS) {
                    expected += 6 * rank_sum;
                }
                expected *= ITERATIONS;

                if (!armci_acc_test_equal(
                        type,
                        base_ptrs[0],
                        (size_t)element,
                        expected)) {
                    if (errors < 16) {
                        fprintf(
                            stderr,
                            "%s element %d does not equal %ld\n",
                            type->name,
                            element,
                            expected);
                    }
                    ++errors;
                }
            }
            ARMCI_Access_end(base_ptrs[0]);
        }

        for (int method = 0; method < 3; ++method) {
            free(sources[method]);
        }
        ARMCI_Barrier();
    }

    int total_errors;
    MPI_Allreduce(
        &errors,
        &total_errors,
        1,
        MPI_INT,
        MPI_SUM,
        MPI_COMM_WORLD);

    ARMCI_Free(base_ptrs[rank]);
    free(base_ptrs);
    ARMCI_Finalize();
    MPI_Finalize();

    if (rank == 0) {
        printf(
            "overlapping contiguous/strided/vector accumulate: %s\n",
            total_errors == 0 ? "PASS" : "FAIL");
    }
    return total_errors == 0 ? 0 : 1;
}
