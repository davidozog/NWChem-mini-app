#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <mpi.h>
#include "workq.h"

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

int multicast_dataqids(int *ppn) {
  int rank, i, tag=1;

  MPI_Request *req = (MPI_Request *)malloc(*ppn * sizeof(MPI_Request));
  MPI_Status *stat = (MPI_Status *)malloc(*ppn * sizeof(MPI_Status));

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  for (i=1; i<*ppn; i++) {
    if (DEBUG) printf("%d: sending %d to %d\n", rank, dataqids[0], rank+i);
    MPI_Isend(&dataqids, NUM_QUEUES, MPI_INT, rank+i, tag, MPI_COMM_WORLD, &req[i-1]);
  }
  MPI_Waitall(*ppn-1, req, stat);

  return 0;
}

int recv_dataqids_(int *nodeid, int *ppn) {
  int rank, tag=1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Status stat;

  if (DEBUG) printf("%d: receiving from %d\n", rank, *nodeid*(*ppn));
  MPI_Recv(&dataqids, NUM_QUEUES, MPI_INT, *nodeid*(*ppn), tag, MPI_COMM_WORLD, &stat);
  if (DEBUG) printf("%d: received %d\n", rank, dataqids[0]);
  return 0;
}

int workq_create_(int *msqids, int *rank, int *nodeid, int *ppn) {
    key_t key;
    int i, collector;

    collector = *rank - (*rank) % (*ppn);

#ifdef USE_MPI_SHM
    int shm_rank, shm_nproc, disp_unit, errors = 0;
    int *shm_base, *q_base, *my_shm_base, *my_q_base;
    MPI_Aint q_size, shm_size;
    MPI_Comm shm_comm;
    MPI_Win  shm_win[2];

    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, *rank, 
                        MPI_INFO_NULL, &shm_comm);
    MPI_Comm_rank(shm_comm, &shm_rank);
    MPI_Comm_size(shm_comm, &shm_nproc);

    MPI_Win_allocate_shared(sizeof(int)*NUM_MSGQS, sizeof(int), MPI_INFO_NULL, 
                             shm_comm, &my_q_base, &shm_win[0]);

    MPI_Win_allocate_shared(sizeof(int)*NUM_QUEUES, sizeof(int), MPI_INFO_NULL, 
                             shm_comm, &my_shm_base, &shm_win[1]);

    /* Locate absolute bases */
    MPI_Win_shared_query(shm_win[0], MPI_PROC_NULL, &q_size, &disp_unit, &q_base); 
    if (disp_unit != sizeof(int))
        errors++;
    if (q_size != NUM_MSGQS * sizeof(int))
        errors++;
    if ((shm_rank == 0) && (q_base != my_q_base))
        errors++;
    if (shm_rank && (q_base == my_q_base))
        errors++;

    MPI_Win_shared_query(shm_win[1], MPI_PROC_NULL, &shm_size, &disp_unit, &shm_base); 
    if (disp_unit != sizeof(int))
        errors++;
    if (shm_size != NUM_QUEUES* sizeof(int))
        errors++;
    if ((shm_rank == 0) && (shm_base != my_shm_base))
        errors++;
    if (shm_rank && (shm_base == my_shm_base))
        errors++;

    if (DEBUG || errors > 0) perror("GOT MPI_SHM ERRORS\n");

    MPI_Win_lock_all(MPI_MODE_NOCHECK, shm_win[0]);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, shm_win[1]);

#endif
    if (*rank == collector) {

      for (i=0; i<NUM_MSGQS; i++) {
        if ((key = ftok(FTOK_FILEPATH, (*nodeid+1)*i)) == -1) {
            perror("ftok1");
            exit(1);
        }
        if ((msqids[i] = msgget(key, 0644 | IPC_CREAT)) == -1) {
            perror("msgget: creating queue...");
            exit(1);
        }
        if (DEBUG) printf("msgqid[%d]=%d\n", i, msqids[i]);
      }

      for (i=0; i<NUM_QUEUES; i++) {
        if ((key = ftok(FTOK_DATAPATH, i)) == -1) {
            perror("ftok2");
            exit(1);
        }
        if ((dataqids[i] = msgget(key, 0644 | IPC_CREAT)) == -1) {
            perror("msgget: creating queue...");
            exit(1);
        }
        if (DEBUG) printf("dataqid[%d]=%d\n", i, dataqids[i]);
      }

#ifdef USE_MPI_SHM
      for (i=0; i<NUM_MSGQS; i++) {
        my_q_base[i] = msqids[i];
      }
      for (i=0; i<NUM_QUEUES; i++) {
        my_shm_base[i] = dataqids[i];
      }
    }

    MPI_Win_sync(shm_win[0]);
    MPI_Win_sync(shm_win[1]);
    MPI_Barrier(shm_comm);

    for (i=0; i<NUM_MSGQS; i++) {
      msqids[i] = q_base[i];
    }
    for (i=0; i<NUM_QUEUES; i++) {
      dataqids[i] = q_base[i];
    }
      
    if (DEBUG) printf("%d: msqids[0] = %d\n", *rank, q_base[0]);
    if (DEBUG) printf("%d: dataids[0] = %d\n", *rank, shm_base[0]);
    MPI_Win_unlock_all(shm_win[0]); MPI_Win_unlock_all(shm_win[1]);
    MPI_Win_free(&shm_win[0]); MPI_Win_free(&shm_win[1]);
    MPI_Comm_free(&shm_comm);

#else
      multicast_msqids_(msqids, ppn);
      multicast_dataqids(ppn);

    }

    /* Not collector */
    else {
      recv_msqids_(msqids, nodeid, ppn);
      recv_dataqids_(nodeid, ppn);
      printf("%d: msqids[0] = %d\n", *rank, msqids[0]);
      printf("%d: dataids[0] = %d\n", *rank, dataqids[0]);
    }

#endif

    workq_sem_init_(ppn);

#ifdef USE_POSIX_SHM
    if (*rank==0) printf("Using POSIX shmem\n");
#else
    if (*rank==0) printf("Using SysV shmem\n");
#endif


    return 0;
}


int workq_create_queue_(int *me, int *collector, int *msqids, int *nodeid, int *ppn) {

  if (*me == *collector) {
    workq_create_(msqids, me, nodeid, ppn);
    multicast_msqids_(msqids, ppn);
    workq_sem_init_(ppn);
  } 
  else {
    recv_msqids_(msqids, nodeid, ppn);
    recv_dataqids_(nodeid, ppn);
  }

  return 0;

}
