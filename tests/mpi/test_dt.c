/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

int main(int argc, char ** argv) {
  int      rank, nproc;
  MPI_Datatype emptyvec;
  MPI_Aint lb, ext;
  int size;

  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  if (rank == 0) printf("Starting MPI datatype test with %d processes\n", nproc);

  MPI_Type_vector(0,0,0,MPI_DOUBLE,&emptyvec);
  MPI_Type_commit(&emptyvec);
  MPI_Type_get_extent(emptyvec, &lb, &ext);
  MPI_Type_size(emptyvec, &size);
  if (rank == 0) printf("lb = %zu ext = %zu size = %d \n", lb, ext, size);

  MPI_Bcast(NULL, 0, emptyvec, 0, MPI_COMM_WORLD);
  MPI_Bcast(NULL, 1, emptyvec, 0, MPI_COMM_WORLD);

  MPI_Type_free(&emptyvec);

  if (rank == 0) printf("Test complete: PASS.\n");

  MPI_Finalize();

  return 0;
}
