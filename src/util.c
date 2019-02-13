/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>
#include <gmr.h>


/** Fatal error, print the message and abort the program with the provided
  * error code.
  */
void ARMCI_Error(const char *msg, int code) {
  fprintf(stderr, "[%d] ARMCI Error: %s\n", ARMCI_GROUP_WORLD.rank, msg);
  fflush(NULL);
  MPI_Abort(ARMCI_GROUP_WORLD.comm, code);
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Barrier = PARMCI_Barrier
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Barrier ARMCI_Barrier
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Barrier as PARMCI_Barrier
#endif
/* -- end weak symbols block -- */

/** Barrier synchronization.  Collective on the world group (not the default
  * group!).
  */
void PARMCI_Barrier(void) {
  gmr_t *cur_mreg = gmr_list;

  PARMCI_AllFence();
  MPI_Barrier(ARMCI_GROUP_WORLD.comm);

  while (cur_mreg) {
    gmr_sync(cur_mreg);
    cur_mreg = cur_mreg->next;
  }
}

/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Fence = PARMCI_Fence
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Fence ARMCI_Fence
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Fence as PARMCI_Fence
#endif
/* -- end weak symbols block -- */

/** Wait for remote completion on one-sided operations targeting process proc.
  * In MPI-2, this is a no-op since get/put/acc already guarantee remote
  * completion.
  *
  * @param[in] proc Process to target
  */
void PARMCI_Fence(int proc) {
  gmr_t *cur_mreg = gmr_list;

  while (cur_mreg) {
    gmr_flush(cur_mreg, proc, 0);
    cur_mreg = cur_mreg->next;
  }
  return;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_AllFence = PARMCI_AllFence
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_AllFence ARMCI_AllFence
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_AllFence as PARMCI_AllFence
#endif
/* -- end weak symbols block -- */

/** Wait for remote completion on all one-sided operations.  In MPI-2, this is
  * a no-op since get/put/acc already guarantee remote completion.
  */
void PARMCI_AllFence(void) {
  gmr_t *cur_mreg = gmr_list;

  while (cur_mreg) {
    gmr_flushall(cur_mreg, 0);
    cur_mreg = cur_mreg->next;
  }
  return;
}


int ARMCI_Uses_shm(void) {
  return 0;
}

/** Limit the amount of memory ARMCI will allocate.
  * This matters for slab allocation but is otherwise ignored.
  *
  * A limit of 0 specifies no limit.
  */
void ARMCI_Set_shm_limit(unsigned long shmemlimit) {
  if (shmemlimit < SIZE_MAX) {
      ARMCII_GLOBAL_STATE.memory_limit = (size_t)shmemlimit;
  } else {
      ARMCII_Warning("ARMCI_Set_shm_limit: invalid input (%lu) - ignoring...\n", shmemlimit);
      ARMCII_GLOBAL_STATE.memory_limit = 0;
  }
}


int ARMCI_Uses_shm_grp(ARMCI_Group *group) {
  return 0;
}


/** Copy local data.
  *
  * @param[in]  src  Source buffer
  * @param[out] dst  Destination buffer
  * @param[in]  size Number of bytes to copy
  */
void ARMCI_Copy(void *src, void *dst, int size) {
  memmove(dst, src, size);
}


/** Zero out the given buffer.
  */
void ARMCII_Bzero(void *buf, armci_size_t size) {
  memset(buf, 0, (size_t)size);
}

/** Retrieve the value of a boolean environment variable.
  */
int ARMCII_Getenv_bool(const char *varname, int default_value) {
  const char *var = getenv(varname);

  if (var == NULL) {
    return default_value;
  }
  if (var[0] == 'T' || var[0] == 't' || var[0] == '1' || var[0] == 'y' || var[0] == 'Y') {
    return 1;
  } else {
    return 0;
  }
}


/** Retrieve the value of a environment variable.
  */
char *ARMCII_Getenv(const char *varname) {
  return getenv(varname);
}


/** Retrieve the value of an integer environment variable.
  */
int ARMCII_Getenv_int(const char *varname, int default_value) {
  const char *var = getenv(varname);
  if (var) {
    return atoi(var);
  } else {
    return default_value;
  }
}

/** Retrieve the value of a long integer environment variable.
  */
long ARMCII_Getenv_long(const char *varname, long default_value) {
  const char *var = getenv(varname);
  if (var) {
    return atol(var);
  } else {
    return default_value;
  }
}

void ARMCIX_Progress(void)
{
    gmr_progress();
}

/** Determine if a window supports the unified memory model.
  *
  * @param[in]  win  Window
  * @param[out] ret  1=UNIFIED, 0=SEPARATE, -1=N/A
  */
int ARMCII_Is_win_unified(MPI_Win win)
{
  void    *attr_ptr;
  int      attr_flag;
  /* this function will always return flag=false in MPI-2 */
  MPI_Win_get_attr(win, MPI_WIN_MODEL, &attr_ptr, &attr_flag);
  if (attr_flag) {
    int * attr_val = (int*)attr_ptr;
    if ( (*attr_val)==MPI_WIN_UNIFIED ) {
      return 1;
    } else if ( (*attr_val)==MPI_WIN_UNIFIED ) {
      return 0;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}
