#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>
#include "work_queue.h"
#include "cblas.h"

#define DEBUG 0
#define TIME 0

extern void tce_sort_4_(double *unsorted, double *sorted, 
                        int *a, int *b, int *c, int *d, 
                        int *i, int *j, int *k, int *l, double *factor);

struct my_msgbuf buf;
struct my_msgbuf bufs[MAXMICROTASKS];
struct bench_buf bench_bufs[MAXMICROTASKS];
struct num_tasks mynum;
double *shm_data;
int shm_offset = 0;

int work_queue_get_info_(
                              int *msqids,
                              int *qid,
                              int *data_id,
                              int *shmid,
                              int *more_tasks,
                              int *data_size,
                              int *dimc,
                              int *a,
                              int *b,
                              int *c,
                              int *d,
                              int *tce_key, 
                              int *shm_key,
                              int *rank,
                              int *ppn
                        )
{
  key_t key;
//  struct num_tasks *num;
  size_t size;

  /* get the key to the queue */
//  if ((key = ftok(FTOK_FILEPATH, 'B')) == -1) {
//      perror("ftok");
//      exit(1);
//  }
//  /* connect to the queue */
//  work_queue_connect(msqid, key);
  
  if (DEBUG) {
    printf("%d: worker receiving messages...\n", *rank);
  }

  mynum.ntasks = 0;

  work_queue_rcv_info(msqids, qid, *rank, *ppn);

  if (DEBUG) {
    printf("%d: THIS MANY TASKS: %d\n", *rank, mynum.ntasks);
    printf("%d: THIS MANY BYTES: %d\n", *rank, mynum.data_size);
    printf("%d: THIS SHMEM KEY :: %d\n", *rank, mynum.shm_key);
  }

  *more_tasks = mynum.ntasks;
  *dimc = mynum.dimc;
  *data_id = mynum.data_id;
  *shm_key = mynum.shm_key;
  *data_size = mynum.data_size;
  *a = mynum.a;
  *b = mynum.b;
  *c = mynum.c;
  *d = mynum.d;
  *tce_key = mynum.tce_key;

  if (*more_tasks > 0) 
    work_queue_get_shm_segment(shm_key, shmid, data_size);

  return 0;
}

/* connect to the queue */
int work_queue_connect(int *msqid, int key) {
  while (1) {
    if ((*msqid = msgget(key, 0644)) == -1) { 
      /* perror("msgget: no queue yet..."); */
      /* exit(1);                           */
    } else break;
  }
  return 0;
}


int work_queue_get_shm_segment(int *shm_key, 
                               int *shmid,
                               int *data_size) {
  key_t key;
  /* make the key: */
  if ((key = ftok(FTOK_FILEPATH, *shm_key)) == -1) {
    perror("ftok4");
    exit(1);
  }

  /* connect to the segment: */
  if ((*shmid = shmget(key, *data_size, 0644 | IPC_CREAT)) == -1) {
      perror("shmget2");
      exit(1);
  }

  /* attach to the segment to get a pointer to it: */
  shm_data = shmat(*shmid, (double *)0, 0);
  if (shm_data == (double *)(-1)) {
      perror("shmat");
      exit(1);
  }

  return 0;
}


int work_queue_rcv_info(int *msqids, int *qid, int rank, int ppn) {
  size_t size = sizeof(struct num_tasks)-sizeof(long);

//  printf("%d: getting info at %d\n", rank, msqids[rank%ppn]);

  struct msqid_ds qbuf;
  //if( msgctl( msqids[rank%NUM_MSGQS], IPC_STAT, &qbuf) == -1) {
  if( msgctl( msqids[*qid], IPC_STAT, &qbuf) == -1) {
    perror("msgctl3 - get_qlen");
    exit(1);
  }
//  printf("%d: qlen is %d\n", rank, qbuf.msg_qnum);

  struct timeval tim;
  gettimeofday(&tim, NULL);
  double t1=tim.tv_sec+(tim.tv_usec/1000000.0);
  //if (msgrcv(msqids[rank%NUM_MSGQS], &mynum, size, MSG_NUMB, 0) == -1) {
  if (msgrcv(msqids[*qid], &mynum, size, MSG_NUMB, 0) == -1) {
      perror("msgrcv1");
      //exit(1);
  }
  gettimeofday(&tim, NULL);
  double t2=tim.tv_sec+(tim.tv_usec/1000000.0);
  //if (TIME) printf("%d: %.6lf seconds elapsed\n", rank, t2-t1);
  if (TIME) printf("%d: %E seconds elapsed\n", rank, t2-t1);
  //if( msgctl( msqids[rank%NUM_MSGQS], IPC_STAT, &qbuf) == -1) {
  if( msgctl( msqids[*qid], IPC_STAT, &qbuf) == -1) {
    perror("msgctl4 - get_qlen");
    exit(1);
  }
//  printf("%d: qlen is now %d\n", rank, qbuf.msg_qnum);

  return 0;
}

int work_queue_get_next_single_(
                          int *rank,
                          int *data_id,
                          int *task_id,
                          int *tile_dim,
                          int *i
                   )
{
  size_t size;

  size = sizeof(struct my_msgbuf) - sizeof(long);
//  printf("DATA_ID = %d\n", *data_id);
  if (msgrcv(*data_id, &bench_bufs[*i], size, *task_id+3, 0) == -1) {
      perror("msgrcv2");
      exit(1);
  }

  *tile_dim = bench_bufs[*i].tile_dim;

//  printf("me:%d, spock2(mtype) %ld\n",*rank,bufs[*i].mtype);
//  printf("me:%d, spock2(task_id) %d\n",*rank,bufs[*i].task_id);
//  printf("me:%d, spock2(dima_sort) %d\n",*rank,bufs[*i].dima_sort);
//  printf("me:%d, spock2(dimb_sort) %d\n",*rank,bufs[*i].dimb_sort);
//  printf("me:%d, spock2(dim_common) %d\n",*rank,bufs[*i].dim_common);
//  printf("me:%d, spock2(nsuper1) %d\n",*rank,bufs[*i].nsuper1);
//  printf("me:%d, spock2(nsuper2) %d\n",*rank,bufs[*i].nsuper2);
//  printf("me:%d ---------------------\n");

  return 0;

}

int work_queue_get_next_(
                          int *rank,
                          int *data_id,
                          int *task_id,
                          int *dima_sort,
                          int *dimb_sort,
                          int *dim_common,
                          int *nsuper1,
                          int *nsuper2,
                          int *e,
                          int *f,
                          int *i
                   )
{
  size_t size;

  size = sizeof(struct my_msgbuf) - sizeof(long);
//  printf("DATA_ID = %d\n", *data_id);
  if (msgrcv(*data_id, &bufs[*i], size, *task_id+3, 0) == -1) {
      perror("msgrcv2");
      exit(1);
  }
//  printf("****************************** data **************************\n");

//  *task_id = bufs[*i].task_id;
  *dima_sort = bufs[*i].dima_sort;
  *dimb_sort = bufs[*i].dimb_sort;
  *dim_common = bufs[*i].dim_common;
  *nsuper1 = bufs[*i].nsuper1;
  *nsuper2 = bufs[*i].nsuper2;
  *e = bufs[*i].e;
  *f = bufs[*i].f;


//  printf("me:%d, spock2(mtype) %ld\n",*rank,bufs[*i].mtype);
//  printf("me:%d, spock2(task_id) %d\n",*rank,bufs[*i].task_id);
//  printf("me:%d, spock2(dima_sort) %d\n",*rank,bufs[*i].dima_sort);
//  printf("me:%d, spock2(dimb_sort) %d\n",*rank,bufs[*i].dimb_sort);
//  printf("me:%d, spock2(dim_common) %d\n",*rank,bufs[*i].dim_common);
//  printf("me:%d, spock2(nsuper1) %d\n",*rank,bufs[*i].nsuper1);
//  printf("me:%d, spock2(nsuper2) %d\n",*rank,bufs[*i].nsuper2);
//  printf("me:%d ---------------------\n");

  return 0;

}

int work_queue_execute_task_single_(double *k_cs,
                             int *tile_dim,
                             double *alpha, double *factor,
                             char *c1, char *c2,
                             int *rank) {

//  work_queue_dgemm_(c1,c2,alpha,factor,dima_sort,dimb_sort,dim_common,
//                    a_sorted,b_sorted,k_cs);

  int dima, dimb;
  dima = *tile_dim * (*tile_dim);
  dimb = dima;

  if (DEBUG) {
    printf("data: ");
    int i;
    for (i=0; i<*tile_dim*(*tile_dim)*2; i++) {
      printf("%f,",shm_data[shm_offset+i]);
    }
    printf("\n\n");
  }

  work_queue_dgemm_(c1,c2,alpha,factor,tile_dim,tile_dim,tile_dim,
                    shm_data + shm_offset,
                    shm_data + shm_offset + dima, k_cs);
  shm_offset += dima + dimb;

  return 0;

}


int work_queue_execute_task_(double *k_cs,
                             int *dima_sort, int *dimb_sort, int *dim_common,
                             int *a, int *b, int *c, int *d, int *e, int *f,
                             int *i, int *j, int *k, int *l, double *factor,
                             double *alpha, char *c1, char *c2, int *rank) {

  int dima = *dima_sort * (*dim_common);
  int dimb = *dimb_sort * (*dim_common);
  double *a_sorted = (double *) malloc (dima * sizeof(double));
  double *b_sorted = (double *) malloc (dimb * sizeof(double));

  work_queue_tce_sort_4_(shm_data + shm_offset, a_sorted, e,f,d,c, i,j,k,l, factor);
  shm_offset += dima;

  work_queue_tce_sort_4_(shm_data + shm_offset, b_sorted, b,a,e,f, k,l,i,j, factor);
  shm_offset += dimb;

  work_queue_dgemm_(c1,c2,alpha,factor,dima_sort,dimb_sort,dim_common,
                    a_sorted,b_sorted,k_cs);

  free(a_sorted);
  free(b_sorted);
  return 0;

}

// TODO: This probably uses NWCHEM blas...  I want OpenBLAS.
int work_queue_dgemm_(char *c1, char *c2, double *alpha, double *factor,
                      int *dima_sort, int *dimb_sort, int *dim_common,
                      double *a_sorted, double *b_sorted, double *k_c) {

  cblas_dgemm(CblasColMajor, CblasTrans, CblasNoTrans, 
                *dim_common, *dim_common, *dim_common, *alpha, a_sorted, 
                *dim_common, b_sorted, *dim_common, *factor, k_c, *dim_common);
//dgemm_(c1,c2,dima_sort,dimb_sort,dim_common,alpha,a_sorted,dim_common,
//b_sorted,dim_common,factor,k_c,dima_sort);

  return 0;
}

int work_queue_tce_sort_4_(double *unsorted, double *sorted,
                           int *a, int *b, int *c, int *d, 
                           int *i, int *j, int *k, int *l, double *factor) {
//  printf("unsorted[0]=%f\n", unsorted[0]);
//  printf("sorted[0]=%f\n", sorted[0]);
//  printf("a=%d\n", *a);
//  printf("b=%d\n", *b);
//  printf("c=%d\n", *c);
//  printf("d=%d\n", *d);
//  printf("i=%d\n", *i);
//  printf("j=%d\n", *j);
//  printf("k=%d\n", *k);
//  printf("l=%d\n", *l);
//  printf("factor=%f\n", *factor);
//  tce_sort_4_(unsorted, sorted, a, b, c, d, i, j, k, l, factor);
  printf("work_queue_dgemm here\n");
//  printf("out alive\n");
  return 0;
}

int work_queue_get_data_(
                          int *dima,
                          double *k_a,
                          int *dimb,
                          double *k_b,
                          int *rank
                        )
{

/* Read next chunk of shm_data... */
  if (DEBUG) {
  printf("%d: here0, %d, %d, %d\n", *rank, (*dima), (*dimb), shm_offset);
  printf("%d: here0, %ld, %ld, %ld\n", *rank, sizeof(double)*(*dima), sizeof(double)*(*dimb), shm_offset*sizeof(double));
  }
  memcpy(k_a, shm_data + shm_offset, sizeof(double)*(*dima));
  //k_a = shm_data + shm_offset;
  shm_offset += *dima;
  if (DEBUG) {
  printf("%d: here1, %d, %d, %d\n", *rank, (*dima), (*dimb), shm_offset);
  printf("%d: here1, %ld, %ld, %ld\n", *rank, sizeof(double)*(*dima), sizeof(double)*(*dimb), shm_offset*sizeof(double));
  }
  memcpy(k_b, shm_data + shm_offset, sizeof(double)*(*dimb));
  //k_b = shm_data + shm_offset;
  shm_offset += *dimb;
  if (DEBUG) {
  printf("%d: here2, %d, %d, %d\n", *rank, (*dima), (*dimb), shm_offset);
  printf("%d: here2, %ld, %ld, %ld\n", *rank, sizeof(double)*(*dima), sizeof(double)*(*dimb), shm_offset*sizeof(double));
  }
  return 0;
}

int work_queue_free_shm_( int *shmid ) {
 /* detach from the segment: */
 if (shmdt(shm_data) == -1) {
   perror("shmdt");
   exit(1);
 }

 //TODO: this at end of application...
 //if (shmctl(*shmid, IPC_RMID, NULL) == -1) {
 //  perror("shmctl");
 //  exit(1);
 //}
 shm_offset = 0;
 return 0;
}
