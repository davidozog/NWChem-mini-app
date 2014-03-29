#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "work_queue.h"

#define DEBUG 0

int multicast_msqids_(int *msqids, int *ppn) {
  int rank, i, tag=0;

  MPI_Request *req = (MPI_Request *)malloc(*ppn * sizeof(MPI_Request));
  MPI_Status *stat = (MPI_Status *)malloc(*ppn * sizeof(MPI_Status));

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  for (i=1; i<*ppn; i++) {
    if (DEBUG) printf("%d: sending to %d\n", rank, rank+i);
    MPI_Isend(msqids, NUM_MSGQS, MPI_INT, rank+i, tag, MPI_COMM_WORLD, &req[i-1]);
  }
  MPI_Waitall(*ppn-1, req, stat);

  return 0;
}

int recv_msqids_(int *msqids, int *nodeid, int *ppn) {
  int rank, tag=0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Status stat;

  if (DEBUG) printf("%d: receiving from %d\n", rank, *nodeid*(*ppn));
  MPI_Recv(msqids, NUM_MSGQS, MPI_INT, *nodeid*(*ppn), tag, MPI_COMM_WORLD, &stat);

  if (DEBUG) printf("%d: received %d\n", rank, *msqids);

  return 0;
}

