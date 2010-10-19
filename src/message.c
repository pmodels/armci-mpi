/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <debug.h>
#include <armci.h>
#include <armci_internals.h>

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


/** Abort the application.
  *
  * @param[in] code Exit error code
  */
void armci_msg_abort(int code) {
  MPI_Abort(ARMCI_GROUP_WORLD.comm, code);
}


/** Get the wall clock time.
  *
  * @return Wall clock time
  */
double armci_timer() {
  return MPI_Wtime();
}


/** Broadcast a message.  Collective.
  *
  * @param[in] buffer Source buffer on root, destination elsewhere.
  * @param[in] len    Length of the message in bytes.
  * @param[in] root   Rank of the root process.
  */
void armci_msg_bcast(void *buffer, int len, int root) {
  MPI_Bcast(buffer, len, MPI_BYTE, root, ARMCI_GROUP_WORLD.comm);
}


/** Broadcast a message.  Collective.
  *
  * @param[in] buffer Source buffer on root, destination elsewhere.
  * @param[in] len    Length of the message in bytes.
  * @param[in] root   Rank of the root process.
  */
void armci_msg_brdcast(void *buffer, int len, int root) {
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


/** Broadcast on a group. Collective.
  *
  * @param[in]    scope ARMCI scope
  * @param[inout] buf   Input on the root, output on all other processes
  * @param[in]    len   Number of bytes in the message
  * @param[in]    root  Rank of the process at the root of the broadcast
  * @param[in]    group ARMCI group on which to perform communication
  */
void armci_msg_group_bcast_scope(int scope, void *buf, int len, int root, ARMCI_Group *group) {
  assert(scope == SCOPE_ALL); // TODO: Only support SCOPE_ALL

  MPI_Bcast(buf, len, MPI_BYTE, root, group->comm);
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
  * @param[in]  src    Source process id
  */
void armci_msg_rcv(int tag, void *buf, int nbytes_buf, int *nbytes_msg, int src) {
  MPI_Status status;
  MPI_Recv(buf, nbytes_buf, MPI_BYTE, src, tag, ARMCI_GROUP_WORLD.comm, &status);

  if (nbytes_msg != NULL)
    MPI_Get_count(&status, MPI_BYTE, nbytes_msg);
}


/** Receive a two-sided message from any source.
  *
  * @param[in]  tag    Message tag (must match on sender and receiver)
  * @param[in]  buf    Buffer containing the message
  * @param[in]  nbytes_buf Size of the buffer in bytes
  * @param[out] nbytes_msg Length of the message received in bytes (NULL to ignore)
  * @return            Rank of the message source
  */
int armci_msg_rcvany(int tag, void *buf, int nbytes_buf, int *nbytes_msg) {
  MPI_Status status;
  MPI_Recv(buf, nbytes_buf, MPI_BYTE, MPI_ANY_SOURCE, tag, ARMCI_GROUP_WORLD.comm, &status);

  if (nbytes_msg != NULL)
    MPI_Get_count(&status, MPI_BYTE, nbytes_msg);

  return status.MPI_SOURCE;
}


void armci_msg_reduce(void *x, int n, char *op, int type) {
  armci_msg_reduce_scope(SCOPE_ALL, x, n, op, type);
}


void armci_msg_reduce_scope(int scope, void *x, int n, char *op, int type) {
  ARMCI_Error("armci_msg_reduce_scope: unimplemented", 10);
}


/** Map process IDs onto a binary tree.
  *
  * @param[in]  scope Scope of processes involved
  * @param[out] root  Process id of the root
  * @param[out] up    Process id of my parent
  * @param[out] left  Process id of my left child
  * @param[out] right Process if of my right child
  */
void armci_msg_bintree(int scope, int *root, int *up, int *left, int *right) {
  int me, nproc;

  assert(scope == SCOPE_ALL); // TODO: Other scopes are not currently supported

  me    = armci_msg_me();
  nproc = armci_msg_nproc();

  *root = 0;
  *up   =  (me == 0) ? -1 : (me - 1) / 2;

  *left = 2*me + 1;
  if (*left >= nproc) *left = -1;

  *right = 2*me + 2;
  if (*right >= nproc) *right = -1;
}


/** FIXME: Unimpemented.
  */
void armci_msg_sel(void *x, int n, char *op, int type, int contribute) {
  ARMCI_Error("armci_msg_sel: unimplemented", 10);
}


/** FIXME: Unimpemented.
  */
void armci_msg_sel_scope(int scope, void *x, int n, char *op, int type, int contribute) {
  ARMCI_Error("armci_msg_sel_scope: unimplemented", 10);
}
