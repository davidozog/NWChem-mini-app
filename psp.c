#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "ga.h"
#include "macdecls.h"
#include "cblas.h"

#define LOCAL_BUFLEN 100
#define TILE_DIM 2
#define MAX_GETS 2


void print(double matrix[MAX_GETS][TILE_DIM*TILE_DIM])
{
    int i, j;
    for (i = 0; i < MAX_GETS; ++i)
    {
        for (j = 0; j < TILE_DIM*TILE_DIM; ++j)
            printf("%f ", matrix[i][j]);
        printf("\n");
    }
}

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

  for (i=0; i<tot_data_size/tile_size; i++) {
    if (next == count) {
      ilo = next*tile_size;
      ihi = ilo + tile_size - 1;
      ld = LOCAL_BUFLEN*nproc;

      NGA_Get(g_a, &ilo, &ihi, bufa, &ld);
      NGA_Get(g_b, &ilo, &ihi, bufb, &ld);

      memset(bufc,0,sizeof(bufc));
      cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans, tile_dim, \
                  tile_dim, tile_dim, 1.0, bufa, tile_dim, bufb,     \
                  tile_dim, 2.0, bufc, tile_dim);

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

  double bufa[MAX_GETS][tile_size]; 
  double bufb[MAX_GETS][tile_size]; 
  double bufc[MAX_GETS][tile_size];

  memset(bufa,0,sizeof(bufa[0][0])*MAX_GETS*tile_size);
  memset(bufb,0,sizeof(bufb[0][0])*MAX_GETS*tile_size);

  t1 = GA_Wtime();

/* Create a global counter for dynamic load balancing */
  int n=1;
  g_cnt = NGA_Create(C_INT, 1, &n, "ga:COUNTER", NULL);
  GA_Zero(g_cnt);

  count = 0;
  index = 0;
  next = NGA_Read_inc(g_cnt, &index, 1);

  ga_nbhdl_t handle_A[MAX_GETS], handle_B[MAX_GETS];

  unsigned int iter = 1;
  unsigned int prevID = 0;
  ld = LOCAL_BUFLEN*nproc;
  for (i=0; i<tot_data_size/tile_size-1; i++) {

    if (next == count) {

      memset(bufc,0,sizeof(bufc[0][0])*MAX_GETS*tile_size);

      if (iter==1) {
        for (j=0; j<MAX_GETS; j++) {
          ilo = (next+j)*tile_size;
          ihi = ilo + tile_size - 1;
          NGA_NbGet(g_a, &ilo, &ihi, &bufa[j][0], &ld,  &handle_A[j]);
          NGA_NbGet(g_b, &ilo, &ihi, &bufb[j][0], &ld,  &handle_B[j]);
        }
        ilo = (next)*tile_size;
        ihi = ilo + tile_size - 1;
        NGA_NbWait(&handle_A[0]);
        NGA_NbWait(&handle_B[0]);
      }
      else if (iter==2) {
        NGA_NbWait(&handle_A[1]);
        NGA_NbWait(&handle_B[1]);
        memcpy(&bufa[0][0], &bufa[1][0], sizeof(double)*tile_size);
        memcpy(&bufb[0][0], &bufb[1][0], sizeof(double)*tile_size);

        ilo = (next+1)*tile_size;
        ihi = ilo + tile_size - 1;
        NGA_NbGet(g_a, &ilo, &ihi, &bufa[1][0], &ld,  &handle_A[1]);
        NGA_NbGet(g_b, &ilo, &ihi, &bufb[1][0], &ld,  &handle_B[1]);
        ilo = (prevID)*tile_size;
        ihi = ilo + tile_size - 1;
      }
      else {
        NGA_NbWait(&handle_A[1]);
        NGA_NbWait(&handle_B[1]);
        memcpy(&bufa[0][0], &bufa[1][0], sizeof(double)*tile_size);
        memcpy(&bufb[0][0], &bufb[1][0], sizeof(double)*tile_size);

        ilo = (next+1)*tile_size;
        ihi = ilo + tile_size - 1;
        NGA_NbGet(g_a, &ilo, &ihi, &bufa[1][0], &ld,  &handle_A[1]);
        NGA_NbGet(g_b, &ilo, &ihi, &bufb[1][0], &ld,  &handle_B[1]);
        ilo = (prevID)*tile_size;
        ihi = ilo + tile_size - 1;
      }

      cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans, tile_dim, \
                  tile_dim, tile_dim, 1.0, &bufa[0][0], tile_dim,    \
                  &bufb[0][0], tile_dim, 2.0, &bufc[0][0], tile_dim);

      ld = tile_dim;
      NGA_Put(g_c, &ilo, &ihi, &bufc[0][0], &ld);

      prevID = next+1;
      next = NGA_Read_inc(g_cnt, &index, 1);
      iter++;
    }
    count++;
  }
        NGA_NbWait(&handle_A[1]);
        NGA_NbWait(&handle_B[1]);
        memcpy(&bufa[0][0], &bufa[1][0], sizeof(double)*tile_size);
        memcpy(&bufb[0][0], &bufb[1][0], sizeof(double)*tile_size);
        memset(bufc,0,sizeof(bufc[0][0])*MAX_GETS*tile_size);
      cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans, tile_dim, \
                  tile_dim, tile_dim, 1.0, &bufa[0][0], tile_dim,    \
                  &bufb[0][0], tile_dim, 2.0, &bufc[0][0], tile_dim);
        ilo = (prevID)*tile_size;
        ihi = ilo + tile_size - 1;

      ld = tile_dim;
      NGA_Put(g_c, &ilo, &ihi, &bufc[0][0], &ld);


  t2 = GA_Wtime();
  total_time = t2 - t1;

  GA_Destroy(g_cnt);

  return;
}


int main(int argc, char *argv[]) {

  int heap, stack;
  heap =  96000000;
  stack = 80000000;

  int me, nproc, i, tot_data_size;
  int n, g_a, g_b, g_c;
  int ilo, ihi, ld;
  double buf[LOCAL_BUFLEN], t1, t2;

  MPI_Init(&argc, &argv);
  GA_Initialize();                           /* initialize GA */

  me = GA_Nodeid(); 
  nproc = GA_Nnodes();
  tot_data_size = nproc * LOCAL_BUFLEN;

  if(! MA_init(C_DBL, stack, heap)) 
       GA_Error("MA_init failed",stack+heap);  /* initialize memory allocator*/ 

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

  t1 = GA_Wtime();
  bench_orig(g_a, g_b, g_c);
  t2 = GA_Wtime();
  GA_Sync();
  if (me == 0)
    printf("Bench (Original) time taken = \%lf seconds\n", t2-t1);

//  t1 = GA_Wtime();
//  bench_nb(g_a, g_b, g_c);
//  t2 = GA_Wtime();
//  GA_Sync();
//  if (me == 0)
//    printf("Bench (Non-Blocking) time taken = \%lf seconds\n", t2-t1);

  GA_Print(g_c);

  GA_Destroy(g_a);  GA_Destroy(g_b);  GA_Destroy(g_c);

  GA_Terminate();
  MPI_Finalize();

  return 0;
}

