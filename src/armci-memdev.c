/*
 * Copyright (C) 2019. See COPYRIGHT in top-level directory.
 */

#include <armci.h>
#include <armci_internals.h>

/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Malloc_memdev = PARMCI_Malloc_memdev
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Malloc_memdev ARMCI_Malloc_memdev
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Malloc_memdev as PARMCI_Malloc_memdev
#endif
/* -- end weak symbols block -- */
int PARMCI_Malloc_memdev(void **ptr_arr, armci_size_t bytes, const char* device)
{
    return ARMCI_Malloc_group(ptr_arr, bytes, &ARMCI_GROUP_WORLD);
}

/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Malloc_group_memdev = PARMCI_Malloc_group_memdev
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Malloc_group_memdev ARMCI_Malloc_group_memdev
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Malloc_group_memdev as PARMCI_Malloc_group_memdev
#endif
/* -- end weak symbols block -- */
int PARMCI_Malloc_group_memdev(void **ptr_arr, armci_size_t bytes, ARMCI_Group *group, const char *device)
{
    return ARMCI_Malloc_group(ptr_arr, bytes, group);
}

/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Free_memdev = PARMCI_Free_memdev
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Free_memdev ARMCI_Free_memdev
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Free_memdev as PARMCI_Free_memdev
#endif
/* -- end weak symbols block -- */
int PARMCI_Free_memdev(void *ptr)
{
    return ARMCI_Free(ptr);
}
