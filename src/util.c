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
#include <mem_region.h>


/** Fatal error, print the message and abort the program with the provided
  * error code.
  */
void ARMCI_Error(char *msg, int code) {
  fprintf(stderr, "[%d] ARMCI Error: %s\n", ARMCI_GROUP_WORLD.rank, msg);
  fflush(NULL);
  MPI_Abort(ARMCI_GROUP_WORLD.comm, code);
}


/** Barrier synchronization.  Collective on the world group (not the default
  * group!).
  */
void ARMCI_Barrier(void) {
  ARMCI_AllFence();
  MPI_Barrier(ARMCI_GROUP_WORLD.comm);
}

/** Wait for remote completion on one-sided operations targeting process proc.
  * In MPI-2, this is a no-op since get/put/acc already guarantee remote
  * completion.
  *
  * @param[in] proc Process to target
  */
void ARMCI_Fence(int proc) {
  return;
}


/** Wait for remote completion on all one-sided operations.  In MPI-2, this is
  * a no-op since get/put/acc already guarantee remote completion.
  */
void ARMCI_AllFence(void) {
  return;
}


int ARMCI_Uses_shm(void) {
  return 0;
}


void ARMCI_Set_shm_limit(unsigned long shmemlimit) {
  return;
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
  memcpy(dst, src, size);
}


/** Zero out the given buffer.
  */
void ARMCII_Bzero(void *buf, int size) {
  int      i;
  uint8_t *buf_b = (uint8_t *)buf;

  for (i = 0; i < size; i++)
    buf_b[i] = 0;
}


static const char log2_table[256] = 
    { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };

/** Calculate the base 2 logarithm of a given integer.
  */
int ARMCII_Log2(int val) {
  int lg;

  if (val <= 0) return -1;

#ifdef BITWISE_LOG2
  for (lg = -1; val > 0; lg++)
    val = val >> 1;
#else /* BYTEWISE (fast) LOG2 */
  {
    int q    = 0;
    int last = 0;
    int i;

    lg = 0;

    for (i = 0; i < sizeof(int); i++) {
      const int byte = val & 0xFF;

      if (byte == 0) {
        q += 8;
      } else {
        last = byte;
        lg  += 8 + q;
        q    = 0;
      }

      val = val >> 8;
    }

    lg = lg - 8 + log2_table[last];
  }
#endif

  return lg;
}
