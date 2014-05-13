#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <mpi.h>
#include "workq.h"
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


int workq_get_hash_block_(int *d_file, double *array, int *size, int *hash, int *key) {
//  get_hash_block_(d_file, array, size, hash, key);
  printf("get_hash here\n");
  return 0;
}

int workq_get_hash_block_i_(int *d_file, double *array, int *size, int *hash, int *key,
                                 int *g2b, int *g1b, int *g4b, int *g3b) {
//  get_hash_block_i_(d_file, array, size, hash, key, g2b, g1b, g4b, g3b);
  printf("get_hash_i here\n");
  return 0;
}

int workq_alloc_task_( int *task_id, int *size) {

#ifdef USE_POSIX_SHM
  int fd;
  char shm_name[64]; 

  /* Create unique file object name from task_id */
  sprintf(shm_name, "%d", *task_id);

  int perms = 0600;           /* permissions */
  
  /* Create shared memory object and set its size */
  if (DEBUG) 
    printf("task %d shm_name=%s, size=%ld\n", *task_id, shm_name, *size*sizeof(double));
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
    printf("(add): task %d, size: %ld\n", *task_id, *size*sizeof(double));
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

int workq_append_task_single_(
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

int workq_append_task_(
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
  workq_get_hash_block_(d_a, shmdata+offset, da, hasha, keya);
  //memcpy(shmdata+offset, k_a, sizeof(double)*(dima));
  offset += dima;

  if (! *intorb ) 
    workq_get_hash_block_(d_b, shmdata+offset, db, hashb, keyb);
  else
    workq_get_hash_block_i_(d_b, shmdata+offset, db, hashb, keyb, g2b, g1b, g4b, g3b);
  //memcpy(shmdata+offset, k_b, sizeof(double)*(dimb));
  offset += dimb;


  bufs[num_microtasks++] = buf;

//  for (i=20; i<30; i++){
//  printf("A[%d]=%E\n", i, *(data+i));
// // printf("B[0]=%f, B[len]=%f\n", data[*dima_sort+1], data[*dima_sort+*dimb_sort+1]);
//  } 

  return 0;
}

int workq_get_min_qlen_( int *nodeid, int *msqids, int *qlen, int *qid ) {
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


int workq_enqueue_single_( int *msqids,    // in
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
     workq_get_min_qlen_(nodeid, msqids, &qlen, &qid);
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

int workq_enqueue_( int *msqids, 
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
    workq_get_min_qlen_(nodeid, msqids, &qlen, &qid);
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

int workq_end_taskgroup_(int *msqids, int *ppn, int *ntasks) {

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

int workq_get_max_qlen_( int *nodeid, int *msqids, int *qlen, int *qid ) {
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



