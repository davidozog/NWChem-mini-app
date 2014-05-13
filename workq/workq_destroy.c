#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include "workq.h"

int workq_destroy_(int *msqids) {
  int i; 
  for (i=0; i<NUM_MSGQS; i++) {
    if (msgctl(msqids[i], IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }
  }
  return 0;
}

int workq_destroy_sem_() {
  key_t key;
  int semid;

  if ((key = ftok(FTOK_FILEPATH, 'S')) == -1) {
        perror("ftok:sem collector");
        exit(1);
  }
  semid = semget(key, 1, 0);
  if (semid >= 0) { 
    semctl(semid, 0, IPC_RMID); /* clean up */
  }
  else {
    perror("error destroying semaphore");
    exit(1);
  }

  return 0;
}

int workq_destroy_data_(int *shmid) {
  if (shmctl(*shmid, IPC_RMID, NULL) == -1) {
    perror("shmctl");
    exit(1);
  }
  return 0;
}

