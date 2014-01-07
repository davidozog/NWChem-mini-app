#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "ga.h"
#include "macdecls.h"
#include "cblas.h"

#define HEAP 200000000
#define STACK 15435450
#define LOCAL_BUFLEN 1093475
//#define LOCAL_BUFLEN 10
#define TILE_DIM 100
#define ITERATIONS 10
#define NUM_BUFS 2


void call_DGEMM(int tile_dim, double *a, double *b, double *c) {
  cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans, tile_dim, \
              tile_dim, tile_dim, 1.0, a, tile_dim, b, tile_dim, \
              2.0, c, tile_dim);
  return;
}


/* Original version */
void bench_orig(int g_a, int g_b, int g_c) {

  int me, nproc, num_nodes, nodeid, ppn, tot_data_size;
  int g_cnt, count, next, index, i, j;
  int ilo, ihi, ld;
  double t1, t2, total_time;
  const int tile_dim = TILE_DIM;
  const int tile_size = tile_dim*tile_dim;

  double bufa[tile_size], bufb[tile_size], bufc[tile_size];
  memset(bufa,0,sizeof(bufa));
  memset(bufb,0,sizeof(bufb));
  memset(bufc,0,sizeof(bufc));

  me = GA_Nodeid(); 
  nproc = GA_Nnodes();
  num_nodes = GA_Cluster_nnodes();
  nodeid = GA_Cluster_nodeid();
  ppn = GA_Cluster_nprocs(nodeid);
  tot_data_size = nproc * LOCAL_BUFLEN;

  t1 = GA_Wtime();

/* Create a global counter for dynamic load balancing */
  int n=1;
  g_cnt = NGA_Create(C_INT, 1, &n, "ga:COUNTER", NULL);
  GA_Zero(g_cnt);

  count = 0;
  index = 0;
  next = NGA_Read_inc(g_cnt, &index, 1);

//    {
//       int i = 0;
//       char hostname[256];
//       gethostname(hostname, sizeof(hostname));
//       printf("PID %d on %s ready for attach\n", getpid(), hostname);
//       fflush(stdout);
//       while (0 == i)
//       sleep(5);
//    }

  for (i=0; i<tot_data_size/tile_size; i++) {
    if (next == count) {
      ilo = next*tile_size;
      ihi = ilo + tile_size - 1;
      ld = LOCAL_BUFLEN*nproc;

      NGA_Get(g_a, &ilo, &ihi, bufa, &ld);
      NGA_Get(g_b, &ilo, &ihi, bufb, &ld);

      memset(bufc,0,sizeof(bufc));
      call_DGEMM(tile_dim, bufa, bufb, bufc);

      ld = tile_dim;
      NGA_Put(g_c, &ilo, &ihi, bufc, &ld);

      next = NGA_Read_inc(g_cnt, &index, 1);
    }
    count++;
  }

  t2 = GA_Wtime();
  total_time = t2 - t1;

  GA_Destroy(g_cnt);

  return;
}

/* Structs/methods for non-blocking version: */
enum bufstate { RED = 1, BLACK = 0 };

void swap_color(enum bufstate *color) {
  if (*color == RED)
    *color = BLACK;
  else
    *color = RED;
}

enum bufstate other_color(enum bufstate *color) {
  if (*color == RED)
    return BLACK;
  else
    return RED;
}

void print(double matrix[NUM_BUFS][TILE_DIM*TILE_DIM]) {
    int i, j;
    for (i = 0; i < NUM_BUFS; ++i)
    {
        for (j = 0; j < TILE_DIM*TILE_DIM; ++j)
            printf("%f ", matrix[i][j]);
        printf("\n");
    }
}


/* Non-blocking version */
void bench_nb(int g_a, int g_b, int g_c) {

  int me, nproc, num_nodes, nodeid, ppn, tot_data_size;
  int g_cnt, count, next, index, i, j, total_tasks;
  int ilo, ihi, ld;
  double t1, t2, total_time;
  const int tile_dim = TILE_DIM;
  const int tile_size = tile_dim*tile_dim;

  me = GA_Nodeid(); 
  nproc = GA_Nnodes();
  num_nodes = GA_Cluster_nnodes();
  nodeid = GA_Cluster_nodeid();
  ppn = GA_Cluster_nprocs(nodeid);
  tot_data_size = nproc * LOCAL_BUFLEN;

  double bufa[NUM_BUFS][tile_size]; 
  double bufb[NUM_BUFS][tile_size]; 
  double bufc[NUM_BUFS][tile_size];

  memset(bufa,0,sizeof(bufa[0][0])*NUM_BUFS*tile_size);
  memset(bufb,0,sizeof(bufb[0][0])*NUM_BUFS*tile_size);

  t1 = GA_Wtime();

/* Create a global counter for dynamic load balancing */
  int n=1;
  g_cnt = NGA_Create(C_INT, 1, &n, "ga:COUNTER", NULL);
  GA_Zero(g_cnt);

  count = 0;
  index = 0;
  next = NGA_Read_inc(g_cnt, &index, 1);

  ga_nbhdl_t handle_A[NUM_BUFS], handle_B[NUM_BUFS], handle_C;

  unsigned int iter = 1;
  unsigned int prevID = 0;
  enum bufstate color, prev_color;
  color = RED;
  prev_color = BLACK;

  ld = LOCAL_BUFLEN*nproc;

  for (i=0; i<tot_data_size/tile_size; i++) {

    if (next == count) {

      memset(bufc,0,sizeof(bufc[0][0])*NUM_BUFS*tile_size);

      if (iter==1) {
        ilo = next * tile_size;
        ihi = ilo + tile_size - 1;
        NGA_NbGet(g_a, &ilo, &ihi, &bufa[BLACK][0], &ld,  &handle_A[BLACK]);
        NGA_NbGet(g_b, &ilo, &ihi, &bufb[BLACK][0], &ld,  &handle_B[BLACK]);

        prevID = next;
        next = NGA_Read_inc(g_cnt, &index, 1);
        ilo = next * tile_size;
        ihi = ilo + tile_size - 1;
        if (ihi<=tot_data_size) {
          NGA_NbGet(g_a, &ilo, &ihi, &bufa[RED][0], &ld,  &handle_A[RED]);
          NGA_NbGet(g_b, &ilo, &ihi, &bufb[RED][0], &ld,  &handle_B[RED]);
        }

        ilo = prevID * tile_size;
        ihi = ilo + tile_size - 1;
        NGA_NbWait(&handle_A[BLACK]);
        NGA_NbWait(&handle_B[BLACK]);
      }
      else {
        NGA_NbWait(&handle_A[color]);
        NGA_NbWait(&handle_B[color]);
        prev_color = color;
        swap_color(&color);

        prevID = next;
        next = NGA_Read_inc(g_cnt, &index, 1);

        ilo = next * tile_size;
        ihi = ilo + tile_size - 1;
        if (ihi<=tot_data_size) {
          NGA_NbGet(g_a, &ilo, &ihi, &bufa[color][0], &ld,  &handle_A[color]);
          NGA_NbGet(g_b, &ilo, &ihi, &bufb[color][0], &ld,  &handle_B[color]);
        }
        ilo = prevID * tile_size;
        ihi = ilo + tile_size - 1;
      }

      call_DGEMM(tile_dim, &bufa[prev_color][0], &bufb[prev_color][0], &bufc[prev_color][0]);

      ld = tile_dim;
      if (iter > 1) 
        NGA_NbWait(&handle_C);
      NGA_NbPut(g_c, &ilo, &ihi, &bufc[prev_color][0], &ld, &handle_C);

      prevID = next;
      //next = NGA_Read_inc(g_cnt, &index, 1);
      iter++;
    }
    count++;
  }

  if (prevID > 0 && prevID < count) {
        NGA_NbWait(&handle_A[color]);
        NGA_NbWait(&handle_B[color]);
        prev_color = color;
        swap_color(&color);
        memset(bufc,0,sizeof(bufc[0][0])*NUM_BUFS*tile_size);
        call_DGEMM(tile_dim, &bufa[prev_color][0], &bufb[prev_color][0], &bufc[prev_color][0]);
        ilo = (prevID)*tile_size;
        ihi = ilo + tile_size - 1;

      ld = tile_dim;
      NGA_Put(g_c, &ilo, &ihi, &bufc[prev_color][0], &ld);
  }

      /* This is the last task from the loop */
      NGA_NbWait(&handle_C);

  t2 = GA_Wtime();
  total_time = t2 - t1;

  GA_Destroy(g_cnt);

  return;
}


int main(int argc, char *argv[]) {

  int heap, stack;
  heap =  HEAP;
  stack = STACK;

  int me, nproc, i, tot_data_size;
  int n, g_a, g_b, g_c;
  int ilo, ihi, ld;
  double buf[LOCAL_BUFLEN], t1, t2;

  MPI_Init(&argc, &argv);
  GA_Initialize();

  me = GA_Nodeid(); 
  nproc = GA_Nnodes();
  tot_data_size = nproc * LOCAL_BUFLEN;

  if(! MA_init(C_DBL, stack, heap)) 
       GA_Error("MA_init failed",stack+heap);  

/* This mimics the creation of T2/V2 in tce_energy.F */
  n = LOCAL_BUFLEN*nproc;
  g_a = NGA_Create(C_DBL, 1, &n, "ga:A", NULL);
  g_b = NGA_Create(C_DBL, 1, &n, "ga:B", NULL);
  g_c = NGA_Create(C_DBL, 1, &n, "ga:C", NULL);


  ilo = me*LOCAL_BUFLEN;
  ihi = ilo + LOCAL_BUFLEN - 1;
  ld = LOCAL_BUFLEN*nproc;

/* Populate GA with synthetic data (GA[i] = i) */
  for (i=1; i<=LOCAL_BUFLEN; i++) 
    buf[i-1] = (double)(LOCAL_BUFLEN*me) + i;

  NGA_Put (g_a, &ilo, &ihi, buf, &ld);
  NGA_Put (g_b, &ilo, &ihi, buf, &ld);
  GA_Zero(g_c);

//  t1 = GA_Wtime();
//  for (i=0; i<ITERATIONS; i++) {
//    bench_orig(g_a, g_b, g_c);
//  }
//  GA_Sync();
//  t2 = GA_Wtime();
//  if (me == 0)
//    printf("Bench (Original) time taken = \%lf seconds\n", t2-t1);

  t1 = GA_Wtime();
  for (i=0; i<ITERATIONS; i++) {
    bench_nb(g_a, g_b, g_c);
  }
  GA_Sync();
  t2 = GA_Wtime();
  if (me == 0)
    printf("Bench (Non-Blocking) time taken = \%lf seconds\n", t2-t1);

//  GA_Print(g_c);

  GA_Destroy(g_a);  GA_Destroy(g_b);  GA_Destroy(g_c);

  GA_Terminate();
  MPI_Finalize();

  return 0;
}

