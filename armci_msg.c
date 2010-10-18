#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include "debug.h"
#include "armci.h"
#include "armci_internals.h"

/** Query process rank from messaging (MPI) layer.
  */
int armci_msg_me() {
  int me;
  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &me);
  return me;
}


/** Query number of processes.
  */
int armci_msg_nproc() {
  int nproc;
  MPI_Comm_size(ARMCI_GROUP_WORLD.comm, &nproc);
  return nproc;
}


/** Broadcast a message.  Collective.
  *
  * @param[in] buffer Source buffer on root, destination elsewhere.
  * @param[in] len    Length of the message in bytes.
  * @param[in] root   Rank of the root process.
  */
void armci_msg_bcast(void* buffer, int len, int root) {
  MPI_Bcast(buffer, len, MPI_BYTE, root, ARMCI_GROUP_WORLD.comm);
}


/** Barrier from the messaging layer.
  */
void armci_msg_barrier() {
  MPI_Barrier(ARMCI_GROUP_WORLD.comm);
}


/** Message barrier on a group.
  *
  * @param[in] group Group on which to perform barrier
  */
void armci_msg_group_barrier(ARMCI_Group *group) {
  MPI_Barrier(group->comm);
}


/** Send a two-sided message.
  *
  * @param[in] tag    Message tag (must match on sender and receiver)
  * @param[in] buf    Buffer containing the message
  * @param[in] nbytes Length of the message in bytes
  * @param[in] dest   Destination process id
  */
void armci_msg_snd(int tag, void *buf, int nbytes, int dest) {
  MPI_Send(buf, nbytes, MPI_BYTE, dest, tag, ARMCI_GROUP_WORLD.comm);
}


/** Receive a two-sided message.
  *
  * @param[in]  tag    Message tag (must match on sender and receiver)
  * @param[in]  buf    Buffer containing the message
  * @param[in]  nbytes_buf Size of the buffer in bytes
  * @param[out] nbytes_msg Length of the message received in bytes (NULL to ignore)
  * @param[in]  dest   Destination process id
  */
void armci_msg_rcv(int tag, void *buf, int nbytes_buf, int *nbytes_msg, int src) {
  MPI_Status status;
  MPI_Recv(buf, nbytes_buf, MPI_BYTE, src, tag, ARMCI_GROUP_WORLD.comm, &status);

  if (nbytes_msg != NULL)
    MPI_Get_count(&status, MPI_BYTE, nbytes_msg);
}


void armci_msg_bintree(int scope, int* Root, int *Up, int *Left, int *Right) {
  int root, up, left, right, index, nproc;

  assert(scope == SCOPE_ALL); // TODO: Don't support others

  root  = 0;
  nproc = ARMCI_GROUP_WORLD.size;
  index = ARMCI_GROUP_WORLD.rank - root;
  up    = (index-1)/2 + root; if ( up < root) up = -1;
  left  = 2*index + 1 + root; if (left >= root+nproc) left = -1;
  right = 2*index + 2 + root; if (right >= root+nproc)right = -1;

  *Up = up;
  *Left = left;
  *Right = right;
  *Root = root;
}
