#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <errno.h>
#include "workq.h"

#define DEBUG 0

/* For the semaphore status */
union semun {
    int val;
    struct semid_ds *buf;
    ushort *array;
};

int workq_sem_init_(int *ppn) {
  key_t key;
  int semid;
  struct sembuf sb;
  union semun arg;

  if (DEBUG) printf("initializing sem...\n");

  if ((key = ftok(FTOK_FILEPATH, 'S')) == -1) {
        perror("ftok:sem collector");
        exit(1);
  }

  if (DEBUG) printf("init key: %d...\n", key);

  semid = semget(key, 1, 0644 | IPC_CREAT);

  if (DEBUG) printf("got ID: %d...\n", semid);

//  Fail if semaphore already exists:
//  semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0644);


  if (semid >= 0) { /* I got it first */
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;

  if (DEBUG) printf("setting sem to: %d...\n", sb.sem_op);

    if (semop(semid, &sb, 1) == -1) {
        int e = errno;
        semctl(semid, 0, IPC_RMID); /* clean up */
        errno = e;
        perror("semop"); /* error, check errno */
        exit(1);
    }
    else {
        arg.val = 0;
        semctl(semid, 0, SETVAL, arg);
    }
    //if (semop(semid, &sb, 1) == -1) {
    //    int e = errno;
    //    semctl(semid, 0, IPC_RMID); /* clean up */
    //    errno = e;
    //    perror("semop"); /* error, check errno */
    //    exit(1);
    //}
  }

  if (DEBUG) printf("got sem...\n");

  return 0;
}

int workq_sem_post_(int *nodeid) {
  key_t key;
  int semid;
  struct sembuf sb;
  union semun arg;
  struct semid_ds buf;
  int ready = 0;

  if ((key = ftok(FTOK_FILEPATH, 'S')) == -1) {
        perror("ftok:sem worker");
        exit(1);
  }

  if (DEBUG) printf("post key: %d...\n", key);

  semid = semget(key, 1, 0);
  if (semid < 0) { 
    perror("semid is negative (post)\n");
    exit(1);
  }
  /* wait for other process to initialize the semaphore: */
  arg.buf = &buf;
//  for(i = 0; i < MAX_RETRIES && !ready; i++) {
  while (ready == 0) {
    semctl(semid, 0, IPC_STAT, arg);
    if (arg.buf->sem_otime != 0) {
      ready = 1;
    } 
  }
//  if (!ready) {
//      errno = ETIME;
//      return -1;
//  }

  sb.sem_num = 0;
  sb.sem_op = 1;
  sb.sem_flg = 0;
  arg.val = 1;

  if (semop(semid, &sb, 1) == -1) {
      semctl(semid, 0, IPC_RMID); /* clean up */
      perror("semop"); 
      exit(1);
  }

  if (DEBUG) printf("%d: incremented sem...\n", *nodeid);

  return 0;
}

int workq_sem_release_(int *nodeid) {
  key_t key;
  int semid;
  struct sembuf sb;
  union semun arg;
  struct semid_ds buf;
  int ready = 0;

  if ((key = ftok(FTOK_FILEPATH, 'S')) == -1) {
        perror("ftok:sem worker");
        exit(1);
  }

  semid = semget(key, 1, 0);
  if (semid < 0 ) {
    perror("semid is negative (release)\n");
    exit(1);
  }
  /* wait for other process to initialize the semaphore: */
  arg.buf = &buf;
//  for(i = 0; i < MAX_RETRIES && !ready; i++) {
  while (ready == 0) {
    semctl(semid, 0, IPC_STAT, arg);
    if (arg.buf->sem_otime != 0) {
      ready = 1;
    } else {
      if (DEBUG) printf("sem not ready...\n");
      printf("sem not ready...\n");
    } 
  }
//  if (!ready) {
//      errno = ETIME;
//      return -1;
//  }

  sb.sem_num = 0;
  sb.sem_op = -1;
  sb.sem_flg = 0;

  if (semop(semid, &sb, 1) == -1) {
      semctl(semid, 0, IPC_RMID); /* clean up */
      perror("semop"); 
      exit(1);
  }

  ushort ar;
  arg.array = &ar;
  semctl(semid, 0, GETALL, arg);

  if (DEBUG) printf("%d: decremented sem to %d...\n", *nodeid, arg.array[0]);

  return 0;
}


int workq_sem_wait_() {
  key_t key;
  int semid;
  struct sembuf sb;
  sb.sem_num = 0;
  sb.sem_op = 0;
  sb.sem_flg = 0;

  if (DEBUG) printf("waiting on sem...\n");

  if ((key = ftok(FTOK_FILEPATH, 'S')) == -1) {
    perror("ftok:sem wait...");
    exit(1);
  }

  semid = semget(key, 1, 0);
  if (semid < 0 ) {
    perror("semid is negative (wait)\n");
    exit(1);
  }
  if (semop(semid, &sb, 1) == -1) {
      semctl(semid, 0, IPC_RMID); /* clean up */
      perror("semop"); 
      exit(1);
  }

  if (DEBUG) printf("done with sem...\n");

  return 0;
}

int workq_sem_getvalue_(int *value, int *nodeid) {
  key_t key;
  int semid;
  union semun arg;
  ushort ar;

  if ((key = ftok(FTOK_FILEPATH, 'S')) == -1) {
        perror("ftok:sem worker");
        exit(1);
  }

  semid = semget(key, 1, 0);
  if (semid < 0 ) {
    perror("semid is negative (getvalue)\n");
    exit(1);
  }

  arg.array = &ar;
  semctl(semid, 0, GETALL, arg);

  if (DEBUG) printf("%d: SEMAPHORE:%d\n", *nodeid, arg.array[0]);
  *value = arg.array[0];

  return 0;
}
