#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "ga.h"
#include "macdecls.h"
#include "cblas.h"

#define LOCAL_BUFLEN 100
#define TILE_DIM 2


void bench_orig(int g_a, int g_b, int g_c){

  int me, nproc, num_nodes, nodeid, ppn, tot_data_size;
  int g_cnt, count, next, index, i, j;
  int ilo, ihi, ld;
  double t1, t2, total_time;
  const int tile_dim = TILE_DIM;
  const int tile_size = tile_dim*tile_dim;

  double bufa[LOCAL_BUFLEN], bufb[LOCAL_BUFLEN], bufc[LOCAL_BUFLEN];
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
  printf("tot_data_size=%d\n", tot_data_size);
  printf("tile_size=%d\n", tile_size);

  for (i=0; i<tot_data_size/tile_size-1; i++) {
    if (next == count) {
      ilo = next*tile_size;
      ihi = ilo + tile_size - 1;
      ld = LOCAL_BUFLEN*nproc;

      NGA_Get(g_a, &ilo, &ihi, bufa, &ld);
      NGA_Get(g_b, &ilo, &ihi, bufb, &ld);

      ld = tile_dim;
      printf("here0\n");
      double *a = (double *) malloc( sizeof(double)*6 );
      double *b = (double *) malloc( sizeof(double)*6 );
      double *c = (double *) malloc( sizeof(double)*6 );

      for (j=0; j<6; j++) {
        a[j] = (double)j;
        b[j] = (double)j;
        c[j] = 0.0;
      }
      cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans, 2, 2, 2, 1.0, a, 2, b, 2, 2.0, c, 2);

      printf("here1\n");
//          call dgemm( 'T', 'N', tile_dim, tile_dim, tile_dim, 1.0d0, 
//     &                 bufa, tile_dim, bufb, tile_dim, 1.0, bufc, 
//     &                 tile_dim)
//
      NGA_Put(g_c, &ilo, &ihi, bufc, &ld);

      next = NGA_Read_inc(g_cnt, &index, 1);
    }
    count++;
  }

  t2 = GA_Wtime();
  total_time = t2 - t1;

  GA_Print(g_cnt);

  printf("num_nodes=%d\n", num_nodes);
  printf("nodeid=%d\n", nodeid);
  printf("ppn=%d\n", ppn);
  printf("me=%d\n", me);
  printf("nproc=%d\n", nproc);

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
    printf("Time taken = \%lf seconds\n", t2-t1);

  GA_Destroy(g_a);  GA_Destroy(g_b);  GA_Destroy(g_c);

  GA_Terminate();
  MPI_Finalize();

  return 0;
}

//      program psp
//      implicit none
//#include "mpif.h"
//#include "mafdecls.fh"
//#include "global.fh"
//#include "errquit.fh"
//#define LOCAL_BUFLEN 100
//      integer ierr, me, nproc, heap, stack, tot_data_size, ga_cnt
//      integer g_a, g_b, g_c, chunk, i, ihi, ilo, jhi, jlo, ld
//      integer msqids(7)
//      double precision buf(LOCAL_BUFLEN), t2, t1
//      logical status
//
//      heap =  96000000
//      stack = 80000000
//
//      call mpi_init(ierr)
//      call ga_initialize()
//
//      me = ga_nodeid()
//      nproc = ga_nnodes()
//      tot_data_size = nproc * LOCAL_BUFLEN
//
//      if (.not.ma_init(MT_DBL, stack, heap))
//     +   call ga_error("ma_init failed",-1)
//      call flush(6)
//
//c   This mimics the creation of T2/V2 in tce_energy.F
//      status = ga_create(MT_DBL, LOCAL_BUFLEN*nproc, 1, 
//     &                   "ga:A", -1, 1, g_a)
//      if (.not.status) call pexit('ga_create(A) fail')
//      status = ga_create(MT_DBL, LOCAL_BUFLEN*nproc, 1, 
//     &                   "ga:B", -1, 1, g_b)
//      if (.not.status) call pexit('ga_create(B) fail')
//      status = ga_create(MT_DBL, LOCAL_BUFLEN*nproc, 1, 
//     &                   "ga:C", -1, 1, g_c)
//      if (.not.status) call pexit('ga_create(C) fail')
//
//      ilo = me*LOCAL_BUFLEN + 1
//      ihi = ilo + LOCAL_BUFLEN - 1
//      ld = LOCAL_BUFLEN*nproc
//
//c   Populate GA with synthetic data (GA[i] = i)
//      do i=1,LOCAL_BUFLEN
//        buf(i) = dble(LOCAL_BUFLEN*me) + i
//      end do
//
//      call ga_put(g_a, ilo, ihi, 1, 1, buf, ld)
//      call ga_put(g_b, ilo, ihi, 1, 1, buf, ld)
//      call ga_zero(g_c)
//
//c     t1 = ga_wtime()
//c     call bench_orig(g_a, g_b, g_c)
//c     call ga_sync()
//c     t2 = ga_wtime()
//c     if(me.eq.0) write(*, 10), "bench_orig:", t2 - t1
// 10   format (a,2x,f10.6)
//
//c     t1 = ga_wtime()
//c     call bench_workq(g_a, g_b, g_c,.true.,ga_cnt,msqids)
//c     call ga_sync()
//c     t2 = ga_wtime()
//c     if(me.eq.0) write(*, 10), "bench_workq:", t2 - t1
//
//      t1 = ga_wtime()
//      call bench_nb(g_a, g_b, g_c)
//      call ga_sync()
//      t2 = ga_wtime()
//      if(me.eq.0) write(*, 10), "bench_nb:", t2 - t1
//
//c     call bench_workq_mm(g_a, g_b, g_c)
//
//c     call ga_print(g_a)
//c     call ga_print(g_c)
//
//      status = ga_destroy(g_a)
//      if (.not.status) call pexit('ga_destroy(A) fail')
//      status = ga_destroy(g_b)
//      if (.not.status) call pexit('ga_destroy(B) fail')
//      status = ga_destroy(g_c)
//      if (.not.status) call pexit('ga_destroy(C) fail')
//      call ga_terminate()
//      call mpi_finalize(ierr)
//
//      end program psp

