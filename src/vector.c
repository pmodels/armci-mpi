/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <debug.h>


/** Generalized I/O vector one-sided put.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int ARMCI_PutV(armci_giov_t *iov, int iov_len, int proc) {
  int v, i;

  for (v = 0; v < iov_len; v++) {
    for (i = 0; i < iov[v].ptr_array_len; i++) {
      ARMCI_Put(iov[v].src_ptr_array[i], iov[v].dst_ptr_array[i], iov[v].bytes, proc);
    }
  }

  return 0;
}


/** Generalized I/O vector one-sided get.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int ARMCI_GetV(armci_giov_t *iov, int iov_len, int proc) {
  int v, i;

  for (v = 0; v < iov_len; v++) {
    for (i = 0; i < iov[v].ptr_array_len; i++) {
      ARMCI_Get(iov[v].src_ptr_array[i], iov[v].dst_ptr_array[i], iov[v].bytes, proc);
    }
  }

  return 0;
}


/** Generalized I/O vector one-sided accumulate.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int ARMCI_AccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc) {
  int v, i;

  for (v = 0; v < iov_len; v++) {
    for (i = 0; i < iov[v].ptr_array_len; i++) {
      ARMCI_Acc(datatype, scale, iov[v].src_ptr_array[i], iov[v].dst_ptr_array[i], iov[v].bytes, proc);
    }
  }

  return 0;
}


int ARMCI_NbPutV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return ARMCI_PutV(iov, iov_len, proc);
}

int ARMCI_NbGetV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return ARMCI_GetV(iov, iov_len, proc);
}

int ARMCI_NbAccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return ARMCI_AccV(datatype, scale, iov, iov_len, proc);
}


