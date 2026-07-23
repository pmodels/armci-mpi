#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mpi.h>
#include <armci.h>

#include "armci_acc_test_types.h"

#define TARGET_ELEMENTS 32
#define PATCH_ELEMENTS 16
#define ITERATIONS 24

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

static int issue_put(
    int method,
    const armci_acc_test_type_t *type,
    void *source,
    void *target,
    int proc)
{
    const int bytes = (int)type->element_size;
    if (method == 0) {
        return ARMCI_Put(
            source,
            target,
            PATCH_ELEMENTS * bytes,
            proc);
    }
    if (method == 1) {
        const int source_stride = bytes;
        const int target_stride = 2 * bytes;
        const int count[2] = {bytes, PATCH_ELEMENTS};
        return ARMCI_PutS(
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
    return ARMCI_PutV(&iov, 1, proc);
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

static int issue_get(
    int method,
    const armci_acc_test_type_t *type,
    void *source,
    void *target,
    int proc)
{
    const int bytes = (int)type->element_size;
    if (method == 0) {
        return ARMCI_Get(
            source,
            target,
            PATCH_ELEMENTS * bytes,
            proc);
    }
    if (method == 1) {
        const int source_stride = 2 * bytes;
        const int target_stride = bytes;
        const int count[2] = {bytes, PATCH_ELEMENTS};
        return ARMCI_GetS(
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
        source_ptrs[element] =
            (char *)source + target_index(method, element) * bytes;
        target_ptrs[element] = (char *)target + element * bytes;
    }
    armci_giov_t iov = {
        source_ptrs,
        target_ptrs,
        bytes,
        PATCH_ELEMENTS
    };
    return ARMCI_GetV(&iov, 1, proc);
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
        void *put_source = malloc(patch_bytes);
        void *acc_sources[3] = {
            malloc(patch_bytes),
            malloc(patch_bytes),
            malloc(patch_bytes)
        };
        void *result = malloc(patch_bytes);
        armci_acc_test_scale_t scale_storage;
        void * const scale =
            armci_acc_test_scale(type, &scale_storage, 2);
        unsigned int random_state =
            0x85ebca6bU ^ (unsigned int)rank ^
            ((unsigned int)type_index << 12);

        for (int iteration = 0; iteration < ITERATIONS; ++iteration) {
            const long put_value =
                20 + (long)type_index + iteration;
            for (int element = 0; element < PATCH_ELEMENTS; ++element) {
                armci_acc_test_set(
                    type,
                    put_source,
                    (size_t)element,
                    put_value);
                for (int method = 0; method < 3; ++method) {
                    armci_acc_test_set(
                        type,
                        acc_sources[method],
                        (size_t)element,
                        method + 1);
                }
            }

            ARMCI_Access_begin(base_ptrs[rank]);
            memset(base_ptrs[rank], 0, allocation_size);
            ARMCI_Access_end(base_ptrs[rank]);
            ARMCI_Barrier();

            const int peer = (rank + 1) % nranks;
            const int put_method =
                (int)(next_random(&random_state) % 3U);
            const int get_method =
                (int)(next_random(&random_state) % 3U);
            int acc_order[3] = {0, 1, 2};
            for (int index = 2; index > 0; --index) {
                const int other =
                    (int)(next_random(&random_state) %
                          (unsigned int)(index + 1));
                const int temporary = acc_order[index];
                acc_order[index] = acc_order[other];
                acc_order[other] = temporary;
            }

            int status = issue_put(
                put_method,
                type,
                put_source,
                base_ptrs[peer],
                peer);
            if (status != 0) {
                MPI_Abort(MPI_COMM_WORLD, status);
            }
            for (int index = 0; index < 3; ++index) {
                const int method = acc_order[index];
                status = issue_accumulate(
                    method,
                    type,
                    scale,
                    acc_sources[method],
                    base_ptrs[peer],
                    peer);
                if (status != 0) {
                    MPI_Abort(MPI_COMM_WORLD, status);
                }
            }
            memset(result, 0, patch_bytes);
            status = issue_get(
                get_method,
                type,
                base_ptrs[peer],
                result,
                peer);
            if (status != 0) {
                MPI_Abort(MPI_COMM_WORLD, status);
            }

            for (int element = 0; element < PATCH_ELEMENTS; ++element) {
                const int physical = target_index(get_method, element);
                long expected = 0;
                if ((put_method == 0 && physical < PATCH_ELEMENTS) ||
                    (put_method == 1 && (physical % 2) == 0) ||
                    (put_method == 2 &&
                     physical >= 8 &&
                     physical < 8 + PATCH_ELEMENTS)) {
                    expected += put_value;
                }
                if (physical < PATCH_ELEMENTS) {
                    expected += 2;
                }
                if ((physical % 2) == 0) {
                    expected += 4;
                }
                if (physical >= 8 &&
                    physical < 8 + PATCH_ELEMENTS) {
                    expected += 6;
                }

                if (!armci_acc_test_equal(
                        type,
                        result,
                        (size_t)element,
                        expected)) {
                    if (errors < 16) {
                        fprintf(
                            stderr,
                            "rank %d %s iteration %d put=%d get=%d "
                            "element=%d physical=%d expected=%ld\n",
                            rank,
                            type->name,
                            iteration,
                            put_method,
                            get_method,
                            element,
                            physical,
                            expected);
                    }
                    ++errors;
                }
            }
            ARMCI_Barrier();
        }

        free(put_source);
        for (int method = 0; method < 3; ++method) {
            free(acc_sources[method]);
        }
        free(result);
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
            "mixed put-acc-get location consistency without flush: %s\n",
            total_errors == 0 ? "PASS" : "FAIL");
    }
    return total_errors == 0 ? 0 : 1;
}
