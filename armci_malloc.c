#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "debug.h"

#include "armci.h"
#include "seg_hdl.h"


/** Allocate a shared memory segment.  Collective.
  *
  * @param[out] base_ptrs Array of length nproc that will contain pointers to
  *                       the base address of each process' patch of the
  *                       segment.
  * @param[in]       size Number of bytes to allocate on the local process.
  */
int ARMCI_Malloc(void **base_ptrs, int size) {
  MPI_Group grp;
  int   rank, nproc, i;
  seg_hdl_t *seg;
  
  seg = seg_hdl_alloc();
  seg->nbytes = size;

  MPI_Alloc_mem(size, MPI_INFO_NULL, &seg->base);
  MPI_Win_create(seg->base, size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &seg->window);

  MPI_Win_get_group(seg->window, &grp);
  MPI_Group_rank(grp, &rank);
  MPI_Group_size(grp, &nproc);
  MPI_Group_free(&grp);

  for (i = 0; i < nproc; i++)
    base_ptrs[i] = seg->base;

  return 0;
}


/** Free a shared memory allocation.  Collective.
  *
  * @param[in] ptr Pointer to the local patch of the allocation
  */
void ARMCI_Free(void *ptr) {
  int flag, ierr;
  void *base;
  seg_hdl_t *seg;
  
  seg = seg_hdl_lookup(ptr);

  ierr = MPI_Win_get_attr(seg->window, MPI_WIN_BASE, &base, &flag);
  assert(ierr == 0);

  MPI_Win_free(&seg->window);
  MPI_Free_mem(base);
  seg_hdl_free(ptr);
}


/** Allocate a local buffer suitable for use in one-sided communication
  *
  * @param[in] size Number of bytes to allocate
  * @return         Pointer to the local buffer
  */
void *ARMCI_Malloc_local(int size) {
  void *buf;
  MPI_Alloc_mem(size, MPI_INFO_NULL, &buf);
  return buf;
}


/** Free memory allocated with ARMCI_Malloc_local
  *
  * @param[in] buf Pointer to local buffer to free
  */
int ARMCI_Free_local(void *buf) {
  MPI_Free_mem(buf);
  return 0;
}
