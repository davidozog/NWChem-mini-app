#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "work_queue.h"

int work_queue_destroy_(int *msqids) {
  int i; 
  for (i=0; i<NUM_MSGQS; i++) {
    if (msgctl(msqids[i], IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }
  }
  return 0;
}

int work_queue_destroy_sem_() {
  key_t key;
  int semid;

  if ((key = ftok("/global/homes/o/ozog/somefile", 'S')) == -1) {
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

int work_queue_destroy_data_(int *shmid) {
  if (shmctl(*shmid, IPC_RMID, NULL) == -1) {
    perror("shmctl");
    exit(1);
  }
  return 0;
}

