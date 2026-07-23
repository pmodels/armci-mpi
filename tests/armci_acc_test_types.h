#ifndef ARMCI_ACC_TEST_TYPES_H
#define ARMCI_ACC_TEST_TYPES_H

#include <stddef.h>

#include <armci.h>

typedef struct {
    int datatype;
    const char *name;
    size_t element_size;
} armci_acc_test_type_t;

typedef union {
    int integer;
    long long_integer;
    float real;
    double double_real;
    float complex_real[2];
    double double_complex_real[2];
} armci_acc_test_scale_t;

static const armci_acc_test_type_t armci_acc_test_types[] = {
    {ARMCI_ACC_INT, "int", sizeof(int)},
    {ARMCI_ACC_LNG, "long", sizeof(long)},
    {ARMCI_ACC_FLT, "float", sizeof(float)},
    {ARMCI_ACC_DBL, "double", sizeof(double)},
    {ARMCI_ACC_CPL, "complex", 2 * sizeof(float)},
    {ARMCI_ACC_DCP, "double-complex", 2 * sizeof(double)}
};

#define ARMCI_ACC_TEST_TYPE_COUNT \
    (sizeof(armci_acc_test_types) / sizeof(armci_acc_test_types[0]))

static void armci_acc_test_set(
    const armci_acc_test_type_t *type,
    void *buffer,
    size_t index,
    long value)
{
    switch (type->datatype) {
        case ARMCI_ACC_INT:
            ((int *)buffer)[index] = (int)value;
            break;
        case ARMCI_ACC_LNG:
            ((long *)buffer)[index] = value;
            break;
        case ARMCI_ACC_FLT:
            ((float *)buffer)[index] = (float)value;
            break;
        case ARMCI_ACC_DBL:
            ((double *)buffer)[index] = (double)value;
            break;
        case ARMCI_ACC_CPL:
            ((float *)buffer)[2 * index] = (float)value;
            ((float *)buffer)[2 * index + 1] = (float)-value;
            break;
        case ARMCI_ACC_DCP:
            ((double *)buffer)[2 * index] = (double)value;
            ((double *)buffer)[2 * index + 1] = (double)-value;
            break;
    }
}

static int armci_acc_test_equal(
    const armci_acc_test_type_t *type,
    const void *buffer,
    size_t index,
    long expected)
{
    switch (type->datatype) {
        case ARMCI_ACC_INT:
            return ((const int *)buffer)[index] == (int)expected;
        case ARMCI_ACC_LNG:
            return ((const long *)buffer)[index] == expected;
        case ARMCI_ACC_FLT:
            return ((const float *)buffer)[index] == (float)expected;
        case ARMCI_ACC_DBL:
            return ((const double *)buffer)[index] == (double)expected;
        case ARMCI_ACC_CPL:
            return ((const float *)buffer)[2 * index] == (float)expected &&
                   ((const float *)buffer)[2 * index + 1] == (float)-expected;
        case ARMCI_ACC_DCP:
            return ((const double *)buffer)[2 * index] == (double)expected &&
                   ((const double *)buffer)[2 * index + 1] == (double)-expected;
    }
    return 0;
}

static void *armci_acc_test_scale(
    const armci_acc_test_type_t *type,
    armci_acc_test_scale_t *scale,
    long value)
{
    switch (type->datatype) {
        case ARMCI_ACC_INT:
            scale->integer = (int)value;
            return &scale->integer;
        case ARMCI_ACC_LNG:
            scale->long_integer = value;
            return &scale->long_integer;
        case ARMCI_ACC_FLT:
            scale->real = (float)value;
            return &scale->real;
        case ARMCI_ACC_DBL:
            scale->double_real = (double)value;
            return &scale->double_real;
        case ARMCI_ACC_CPL:
            scale->complex_real[0] = (float)value;
            scale->complex_real[1] = 0.0f;
            return scale->complex_real;
        case ARMCI_ACC_DCP:
            scale->double_complex_real[0] = (double)value;
            scale->double_complex_real[1] = 0.0;
            return scale->double_complex_real;
    }
    return NULL;
}

#endif
