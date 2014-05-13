#ifndef _WORKQ_H
#define _WORKQ_H

#define MSG_NUMB 2
#define MSG_TYPE 1
#define MAXMICROTASKS 4096
#define NUM_MSGQS 1
#define NUM_QUEUES 1
#define USE_POSIX_SHM 1
//#define USE_MPI_SHM 1
#ifdef FTOKM
  #define FTOK_FILEPATH FTOKM
#endif
#ifdef FTOKD
  #define FTOK_DATAPATH FTOKD
#endif

/* Specific to TCE-CC macro-task: */
struct num_tasks {
    long mtype;
    int ntasks;
    int dimc;
    int shm_key; 
    int data_id;
    int data_size;
    int a;
    int b;
    int c;
    int d;
    int tce_key;
};

/* Specific to TCE-CC micro-task: */
struct my_msgbuf {
    long mtype;
    int task_id;
    int dima_sort;
    int dimb_sort;
    int dim_common;
    int nsuper1;
    int nsuper2;
    int e;
    int f;
};

/* For Mini-App benchmark: */
struct bench_buf {
    long mtype;
    int task_id;
    int tile_dim;
};

int dataqids[NUM_QUEUES];

/* Courier side */
int workq_alloc_task_(int *task_id, int *size);
int workq_append_task_single_(int *task_id, int *tile_dim, int *g_a, int *g_b, \
                              int *ld,  double *bufa, double *bufb);
int workq_append_task_(int *task_id, int *dima_sort, int *dimb_sort,           \
                       int *dim_common, int *nsuper1, int *nsuper2, int *e,    \
                       int *f, int *d_a, int *d_b, int *hasha, int *hashb,     \
                       int *keya, int *keyb,  int *da, int *db, int *intorb,   \
                       int *g2b, int *g1b, int *g4b, int *g3b);
int workq_get_min_qlen_(int *nodeid, int *msqids, int *qlen, int *qid);
int workq_enqueue_single_(int *msqids, int *task_id, int *tile_dim, int *rank, \
                     int *nodeid, int *ppn, int *collector);
int workq_enqueue_(int *msqids, int *task_id, int *dimc, int *a, int *b,       \
                   int *c, int *d, int *tce_key, int *rank, int *nodeid,       \
                   int *ppn, int *collector); 
int workq_end_taskgroup_(int *msqids, int *ppn, int *ntasks);
int workq_get_max_qlen_(int *nodeid, int *msqids, int *qlen, int *qid);

/* Worker side */
int workq_rcv_info(int *msqids, int *qid, int rank, int ppn);
int workq_get_shm_segment(int *shm_key, int *shmid, int *data_size);
int workq_get_info_(int *msqids, int *qid, int *data_id, int *shmid,           \
                    int *more_tasks, int *data_size, int *dimc, int *a,        \
                    int *b, int *c, int *d, int *tce_key,  int *shm_key,       \
                    int *rank, int *ppn);
int workq_dequeue_single_(int *rank, int *data_id, int *task_id,               \
                          int *tile_dim, int *i);
int workq_dequeue_next_(int *rank, int *data_id, int *task_id, int *dima_sort, \
                        int *dimb_sort, int *dim_common, int *nsuper1,         \
                        int *nsuper2, int *e, int *f, int *i);

int workq_free_shm_(int *task_id, int *data_size, int *shmid);
int workq_sem_init_(int *ppn);
int workq_dgemm_(char *c1, char *c2, double *alpha, double *factor,            \
                      int *dima_sort, int *dimb_sort, int *dim_common,         \
                      double *a_sorted, double *b_sorted, double *k_c);
int workq_tce_sort_4_(double *unsorted, double *sorted, int *a, int *b,        \
                      int *c, int *d, int *i, int *j, int *k, int *l,          \
                      double *factor);   
int workq_execute_task_(double *k_cs, int *dima_sort, int *dimb_sort,          \
                        int *dim_common, int *a, int *b, int *c, int *d,       \
                        int *e, int *f, int *i, int *j, int *k, int *l,        \
                        double *factor, double *alpha, char *c1, char *c2,     \
                        int *rank);
int workq_execute_task_single_(double *k_cs, int *tile_dim, double *alpha,     \
                               double *factor, char *c1, char *c2, int *rank);
int workq_get_data_(int *dima, double *k_a, int *dimb, double *k_b, int *rank );

int workq_free_shm_(int *task_id, int *data_size, int *shmid);

int multicast_msqids_(int *msqids, int *ppn);
int recv_msqids_(int *msqids, int *nodeid, int *ppn);
int recv_dataqids_(int *nodeid, int *ppn);


#endif
