#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <mpi.h>
#include "work_queue.h"
#include "ga.h"
#ifdef USE_POSIX_SHM
  #include <sys/mman.h> 
  #include <fcntl.h>
#endif

#define MAX_RETRIES 100

#define DEBUG 0
#define TAUDB 1

extern void get_hash_block_(int *d_file, double *array, int *size, int *hash, int *key);
extern void get_hash_block_i_(int *d_file, double *array, int *size, int *hash, int *key, 
                              int *g2b, int *g1b, int *g4b, int *g3b);

/* For the semaphore status */
union semun {
    int val;
    struct semid_ds *buf;
    ushort *array;
};

int num_microtasks = 0;
int tot_microtasks = 0;
struct my_msgbuf bufs[MAXMICROTASKS];
struct bench_buf bench_bufs[MAXMICROTASKS];
//double *mydata;
double *shmdata;
int offset = 0;
size_t tot_size = 0;
int sem;
//int mid = 0;
//int msgqids[7];
int dataqids[NUM_QUEUES];


int work_queue_get_hash_block_(int *d_file, double *array, int *size, int *hash, int *key) {
//  get_hash_block_(d_file, array, size, hash, key);
  printf("get_hash here\n");
  return 0;
}

int work_queue_get_hash_block_i_(int *d_file, double *array, int *size, int *hash, int *key,
                                 int *g2b, int *g1b, int *g4b, int *g3b) {
//  get_hash_block_i_(d_file, array, size, hash, key, g2b, g1b, g4b, g3b);
  printf("get_hash_i here\n");
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

int work_queue_create_(int *msqids, int *rank, int *nodeid, int *ppn) {
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

    work_queue_sem_init_(ppn);

#ifdef USE_POSIX_SHM
    if (*rank==0) printf("Using POSIX shmem\n");
#else
    if (*rank==0) printf("Using SysV shmem\n");
#endif


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

int work_queue_alloc_task_( int *task_id, int *size) {

#ifdef USE_POSIX_SHM
  int fd;
  char shm_name[64]; 

  /* Create unique file object name from task_id */
  sprintf(shm_name, "%d", *task_id);

  int perms = 0600;           /* permissions */
  
  /* Create shared memory object and set its size */
  if (DEBUG) 
    printf("task %d shm_name=%s, size=%d\n", *task_id, shm_name, *size*sizeof(double));
  fd = shm_open(shm_name, O_CREAT | O_RDWR, perms);
  if (fd == -1) { 
    perror("shm_open"); exit(1); 
  }

  if (ftruncate(fd, *size*sizeof(double)) == -1) {
    perror("ftruncate1"); exit(1);
  }

  /* Map shared memory object */
  shmdata = mmap(NULL, *size*sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shmdata == MAP_FAILED) {
    printf("(add): task %d, size: %d\n", *task_id, *size*sizeof(double));
    perror("mmap"); exit(1);
  }

#else
//  int i;
//  struct num_tasks num;
  key_t key;
  int shmid;

// Note: all these microtasks have the same task_id/shm_key
// TODO: If calling this every time is a performance burden,
// on the receiving side, I can always set it in the struct above...
  if ((key = ftok(FTOK_FILEPATH, *task_id)) == -1) {
    perror("ftok3");
    exit(1);
  }

  /* connect to (and possibly create) the segment: */
  if ((shmid = shmget(key, *size*sizeof(double), 0644 | IPC_CREAT)) == -1) {
    perror("shmget1");
    exit(1);
  }
  /* attach to the segment to get a pointer to it: */
  shmdata = shmat(shmid, (double *)0, 0);
  //shmdata = shmat(shmid, mydata, SHM_RND);
  if (shmdata == (double *)(-1)) {
//    int e = errno;
//    errno = e;
//    printf("ERRNO:%d\n", e);
    perror("shmat");
    exit(1);
  }
#endif

  return 0;

}

int work_queue_append_task_single_(
                     int *task_id,
                     int *tile_dim,
                     int *g_a,
                     int *g_b,
                     int *ld, 
                     double *bufa,
                     double *bufb
    ) {

  struct bench_buf buf;
  int lo[2], hi[2], myld[1], tile_size;
  
  buf.mtype = *task_id+3;
  buf.task_id = *task_id;
  buf.tile_dim = *tile_dim;

  tile_size = *tile_dim * (*tile_dim);
  lo[1] = *task_id*tile_size;
  hi[1] = lo[1] + tile_size - 1;
  lo[0] = 0;
  hi[0] = 0;
  myld[0] = *ld;

//printf("lo={%d,%d}, hi={%d,%d}\n", lo[0], lo[1], hi[0], hi[1]);
  NGA_Get(*g_a, lo, hi, shmdata+offset, myld);
  offset += tile_size;
  NGA_Get(*g_b, lo, hi, shmdata+offset, myld);
  offset += tile_size;
  tot_size += sizeof(double)*(tile_size*2);

//int i,j;
//printf("bufa: ");
//for (i=0; i<*tile_dim; i++) {
//  for (j=0; j<*tile_dim; j++) {
//    printf("%f ", bufa[*tile_dim*i+j]);
//  }
//}

  bench_bufs[num_microtasks++] = buf;

  return 0;
}

int work_queue_append_task_(
                     int *task_id,
                     int *dima_sort,
                     int *dimb_sort,
                     int *dim_common,
                     int *nsuper1,
                     int *nsuper2,
                     int *e,
                     int *f,
                     int *d_a,
                     int *d_b,
                     int *hasha,
                     int *hashb,
                     int *keya,
                     int *keyb, 
                     int *da,
                     int *db,
                     int *intorb,
                     int *g2b,
                     int *g1b,
                     int *g4b,
                     int *g3b
    ) {

  struct my_msgbuf buf;
  int dima, dimb;
  
//  int i;
//  for (i=20; i<30; i++){
//  printf("k_bs[%d]=%E\n", i, k_bs[i]);
//  } 


  buf.mtype = *task_id+3;
  buf.task_id = *task_id;
  buf.dima_sort = *dima_sort;
  buf.dimb_sort = *dimb_sort;
  buf.dim_common = *dim_common;
  buf.nsuper1 = *nsuper1;
  buf.nsuper2 = *nsuper2;
  buf.e = *e;
  buf.f = *f;


  dima = *dima_sort * (*dim_common);
  dimb = *dimb_sort * (*dim_common);
  tot_size += sizeof(double)*(dima + dimb);
//  mydata = (double *)realloc(mydata, tot_size);

//  mydata[mid] = (double *)realloc(mydata[mid], tot_size);
//  posix_memalign((void **)mydata[mid], sysconf(_SC_PAGESIZE), tot_size);
//  mydata[mid] = (double *)memalign(sysconf(_SC_PAGESIZE), tot_size);
//  printf("mydata[mid] = %p\n", mydata[mid]);

//  memcpy(mydata+offset, k_a, sizeof(double)*(dima));

  //get_hash_block_(d_a, shmdata+offset, &dima, hasha, keya);
  work_queue_get_hash_block_(d_a, shmdata+offset, da, hasha, keya);
  //memcpy(shmdata+offset, k_a, sizeof(double)*(dima));
  offset += dima;

  if (! *intorb ) 
    work_queue_get_hash_block_(d_b, shmdata+offset, db, hashb, keyb);
  else
    work_queue_get_hash_block_i_(d_b, shmdata+offset, db, hashb, keyb, g2b, g1b, g4b, g3b);
  //memcpy(shmdata+offset, k_b, sizeof(double)*(dimb));
  offset += dimb;


  bufs[num_microtasks++] = buf;

//  for (i=20; i<30; i++){
//  printf("A[%d]=%E\n", i, *(data+i));
// // printf("B[0]=%f, B[len]=%f\n", data[*dima_sort+1], data[*dima_sort+*dimb_sort+1]);
//  } 

  return 0;
}

int work_queue_get_min_qlen_( int *nodeid, int *msqids, int *qlen, int *qid ) {
  struct msqid_ds qbuf;
  int i, min, min_q;

  min = 0;
  min_q = 0;
  for (i=0; i<NUM_MSGQS; i++) {
  if( msgctl( msqids[i], IPC_STAT, &qbuf) == -1) {
    perror("msgctl1 - get_qlen");
    exit(1);
  }
//  printf("n:%d r:%d qlen is  %d\n", *nodeid, i, qbuf.msg_qnum);
    if (i==0) {
      min = qbuf.msg_qnum;
    }
    else if (qbuf.msg_qnum < min) {
      min = qbuf.msg_qnum;
      min_q = i; 
    }

//    printf("qlen[%d] = %d\n", i, qbuf.msg_qnum);
  }

//  printf("min is now %d\n", min);
//  printf("min_q is now %d\n", min_q);

  *qlen = min;
  *qid = min_q;

  return 0;
}


int work_queue_add_single_( int *msqids,    // in
                     int *task_id,          // in
                     int *tile_dim,         // in
                     int *rank,             // in
                     int *nodeid,           // in
                     int *ppn,              // in
                     int *collector         // in  (bool)
                   ) {                      

  int i;
  int qlen, qid;
  struct num_tasks num;
  size_t size;

   if (DEBUG) printf("sending %d tasks...\n", num_microtasks);
   num.mtype = MSG_NUMB;
   num.ntasks = num_microtasks;
   num.dimc = *tile_dim;
   num.shm_key = *task_id;
   num.data_id = dataqids[*task_id % NUM_QUEUES];
   num.data_size = tot_size;
   
#ifdef USE_POSIX_SHM
  munmap(shmdata, tot_size);
#else
  /* detach from the segment: */
  if (shmdt(shmdata) == -1) {
      perror("shmdt");
      exit(1);
  }
#endif
 
   size = sizeof(struct num_tasks) - sizeof(long);
 
   if (*collector) {
     work_queue_get_min_qlen_(nodeid, msqids, &qlen, &qid);
 //    printf("min qlen is %d\n", qlen);
 //    printf("min qid is %d\n", qid);
   }
   else {
     qid = *rank % NUM_MSGQS;
 //    printf("%d adding a task of size %d\n", *rank, size);
   }
 
   if (msgsnd(msqids[qid], &num, size, 0) == -1)  {
     perror("msgsnd2");
     exit(1);
   }
 
   size = sizeof(struct my_msgbuf) - sizeof(long);
   if (DEBUG) printf("sending!\n");
   for (i=0; i<num_microtasks; i++) {
     //if (msgsnd(*msqid, &bufs[i], size, 0) == -1) 
     //  perror("msgsnd");
 //   printf("sending on data_id %d\n", num.data_id);
     if (msgsnd(num.data_id, &bench_bufs[i], size, 0) == -1) 
       perror("msgsnd - data");
   }
 
   //if (TAUDB) {
   //  tot_microtasks += num_microtasks;
   //}
 
   // TODO: free this at the very end of the application ...
   //free(mydata);
   num_microtasks = 0;
   tot_size = 0;
   offset = 0;
 
   if (DEBUG) printf("sent!\n");

  return 0;

}

int work_queue_add_( int *msqids, 
                     int *task_id,
                     int *dimc,
                     int *a,
                     int *b,
                     int *c,
                     int *d,
                     int *tce_key,
                     int *rank,
                     int *nodeid,
                     int *ppn,
                     int *collector
                   ) {

  int i;
  int qlen, qid;
  struct num_tasks num;
  size_t size;

  if (DEBUG) printf("sending %d tasks...\n", num_microtasks);
  num.mtype = MSG_NUMB;
  num.ntasks = num_microtasks;
  num.dimc = *dimc;
  num.shm_key = *task_id;
  num.data_id = dataqids[*task_id % NUM_QUEUES];
  num.data_size = tot_size;
  num.a = *a;
  num.b = *b;
  num.c = *c;
  num.d = *d;
  num.tce_key = *tce_key;
  
//  printf("tid:%d, memcpying %d\n", *task_id, tot_size);
//  /* write to the segment: */
//  memcpy(shmdata, mydata, tot_size);
//  printf("alive\n");
  // Can you just assign the pointer?...
  //shmdata = (double *)mydata;
  //shmdata = mydata;
  //mid++;

#ifdef USE_POSIX_SHM
  munmap(shmdata, tot_size);
#else
  /* detach from the segment: */
  if (shmdt(shmdata) == -1) {
      perror("shmdt");
      exit(1);
  }
#endif

  size = sizeof(struct num_tasks) - sizeof(long);

  if (*collector) {
    work_queue_get_min_qlen_(nodeid, msqids, &qlen, &qid);
//    printf("min qlen is %d\n", qlen);
//    printf("min qid is %d\n", qid);
  }
  else {
    qid = *rank % NUM_MSGQS;
//    printf("%d adding a task of size %d\n", *rank, size);
  }

  if (msgsnd(msqids[qid], &num, size, 0) == -1)  {
    perror("msgsnd2");
    exit(1);
  }

  size = sizeof(struct my_msgbuf) - sizeof(long);
  if (DEBUG) printf("sending!\n");
  for (i=0; i<num_microtasks; i++) {
    //if (msgsnd(*msqid, &bufs[i], size, 0) == -1) 
    //  perror("msgsnd");
//   printf("sending on data_id %d\n", num.data_id);
    if (msgsnd(num.data_id, &bufs[i], size, 0) == -1) 
      perror("msgsnd - data");
  }

  //if (TAUDB) {
  //  tot_microtasks += num_microtasks;
  //}

  // TODO: free this at the very end of the application ...
  //free(mydata);
  num_microtasks = 0;
  tot_size = 0;
  offset = 0;

  if (DEBUG) printf("sent!\n");

  return 0;

}

int work_queue_end_taskgroup_(int *msqids, int *ppn, int *ntasks) {

  if (DEBUG) printf("inside end_taskgroup\n");
  size_t size;
  struct num_tasks *buf;
  int i;
  buf = (struct num_tasks*) malloc(sizeof(struct num_tasks)); 
  buf->mtype = MSG_NUMB;
//  if (num_microtasks == 0) {
//    perror("why are there zero tasks here?\n");
//    exit(1);
//  } else {
  buf->ntasks = 0; /* This signals Fortran all tasks are done */
  buf->data_size = 0;
  buf->dimc = 0;
  buf->shm_key = 0;
//  }

 
  size = sizeof(struct num_tasks) - sizeof(long);
  for (i=0; i<*ppn; i++) {
    if (msgsnd(msqids[i%NUM_MSGQS], buf, size, 0) == -1)  {
            perror("msgsnd1");
            exit(1);
    }
  }
  free(buf);

  //if (TAUDB) {
  //  char str[15];
  //  sprintf(str, "%d", *ntasks);
  //  TAU_METADATA("t2_8 total tasks", str);
  //  sprintf(str, "%f", (float) (tot_microtasks / *ntasks));
  //  TAU_METADATA("t2_8 average microtasks", str);
  //  tot_microtasks = 0;
  //}



  if (DEBUG) printf("out of end_taskgroup\n");

  return 0;
}

int work_queue_get_max_qlen_( int *nodeid, int *msqids, int *qlen, int *qid ) {
  struct msqid_ds qbuf;
  int i, max, max_q;

  max = 0;
  max_q = 0;
  for (i=0; i<NUM_MSGQS; i++) {
    if( msgctl( msqids[i], IPC_STAT, &qbuf) == -1) {
      perror("msgctl2 - get_qlen");
      exit(1);
    }
//  printf("n:%d r:%d qlen is  %d\n", *nodeid, i, qbuf.msg_qnum);
    if (i==0) {
      max = qbuf.msg_qnum;
    }
    else if (qbuf.msg_qnum > max ) {
      max = qbuf.msg_qnum;
      max_q = i; 
    }

//    printf("qlen[%d] = %d\n", i, qbuf.msg_qnum);
  }

//  printf("min is now %d\n", min);
//  printf("min_q is now %d\n", min_q);

  *qlen = max;
  *qid = max_q;

  return 0;
}


int work_queue_sem_init_(int *ppn) {
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

int work_queue_sem_post_(int *nodeid) {
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

int work_queue_sem_release_(int *nodeid) {
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


int work_queue_sem_wait_() {
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

int work_queue_sem_getvalue_(int *value, int *nodeid) {
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
