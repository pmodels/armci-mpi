#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mpi.h>
#include <armci.h>

#define MAX_SIZE   262144
#define NUM_ROUNDS 1000

int main(int argc, char **argv) {
  int        me, nproc, zero;
  int        msg_length, round, i;
  double     t_start, t_stop;
  u_int8_t  *snd_buf;  // Send buffer (byte array)
  u_int8_t **rcv_buf;  // Receive buffer (byte array)

  MPI_Init(&argc, &argv);
  ARMCI_Init();

  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  if (nproc != 2)
    ARMCI_Error("This benchmark should be run on exactly two processes", 1);

  if (me == 0)
    printf("ARMCI ping-pong latency test, performing %d rounds at each xfer size.\n\n", NUM_ROUNDS);

  rcv_buf = malloc(nproc*sizeof(void*));

  ARMCI_Malloc((void*)rcv_buf, MAX_SIZE);
  snd_buf = ARMCI_Malloc_local(MAX_SIZE);

  zero = 0;

  for (i = 0; i < MAX_SIZE; i++) {
    snd_buf[i] = 1;
  }

  for (msg_length = 1; msg_length <= MAX_SIZE; msg_length *= 2) {
    ARMCI_Barrier();
    t_start = MPI_Wtime();

    // Perform NUM_ROUNDS ping-pongs
    for (round = 0; round < NUM_ROUNDS*2; round++) {
      // printf("%d: msg_length = %d round = %d\n", me, msg_length, round);

      // I am the sender
      if (round % 2 == me) {
        // Clear start and end markers for next round
#ifdef DIRECT_ACCESS
        ((u_int8_t*)rcv_buf[me])[0] = 0;
        ((u_int8_t*)rcv_buf[me])[msg_length-1] = 0;
#else
        ARMCI_Put(&zero, &(((u_int8_t*)rcv_buf[me])[0]),            1, me);
        ARMCI_Put(&zero, &(((u_int8_t*)rcv_buf[me])[msg_length-1]), 1, me);
#endif

        ARMCI_Put(snd_buf, rcv_buf[(me+1)%2], msg_length, (me+1)%2);
        ARMCI_Fence((me+1)%2);

        ARMCI_Barrier();
      }

      // I am the receiver
      else {
        ARMCI_Barrier();

#ifdef DIRECT_ACCESS
        while (((volatile u_int8_t*)rcv_buf[me])[0] == 0) ;
        while (((volatile u_int8_t*)rcv_buf[me])[msg_length-1] == 0) ;
#else
        u_int8_t val;

        do {
          ARMCI_Get(&(((u_int8_t*)rcv_buf[me])[0]), &val, 1, me);
        } while (val == 0);

        do {
          ARMCI_Get(&(((u_int8_t*)rcv_buf[me])[msg_length-1]), &val, 1, me);
        } while (val == 0);
#endif
      }
    }

    ARMCI_Barrier();
    t_stop = MPI_Wtime();

    if (me == 0)
      printf("%8d bytes \t %12.8f us\n", msg_length, (t_stop-t_start)/NUM_ROUNDS*1.0e6);
  }

  ARMCI_Free(rcv_buf[me]);
  free(rcv_buf);
  ARMCI_Free_local(snd_buf);

  ARMCI_Finalize();
  MPI_Finalize();

  return 0;
}
