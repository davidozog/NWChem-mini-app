      program simple
      implicit none
#include "mpif.h"
#include "mafdecls.fh"
#include "global.fh"
#include "errquit.fh"
#define LOCAL_BUFLEN 3125000
      integer ierr, me, nproc, heap, stack, tot_data_size, ga_cnt
      integer g_a, g_b, g_c, chunk, i, ihi, ilo, jhi, jlo, ld
      integer msqids(7)
      double precision buf(LOCAL_BUFLEN)
      logical status

      heap =  96000000
      stack = 96000000

      call mpi_init(ierr)
      call ga_initialize()

      me = ga_nodeid()
      nproc = ga_nnodes()
      tot_data_size = nproc * LOCAL_BUFLEN

      if (.not.ma_init(MT_DBL, stack, heap))
     +   call ga_error("ma_init failed",-1)
      call flush(6)

c   This mimics the creation of T2/V2 in tce_energy.F
      status = ga_create(MT_DBL, LOCAL_BUFLEN*nproc, 1, 
     &                   "ga:A", -1, 1, g_a)
      if (.not.status) call pexit('ga_create(A) fail')
      status = ga_create(MT_DBL, LOCAL_BUFLEN*nproc, 1, 
     &                   "ga:B", -1, 1, g_b)
      if (.not.status) call pexit('ga_create(B) fail')
      status = ga_create(MT_DBL, LOCAL_BUFLEN*nproc, 1, 
     &                   "ga:C", -1, 1, g_c)
      if (.not.status) call pexit('ga_create(C) fail')

      ilo = me*LOCAL_BUFLEN + 1
      ihi = ilo + LOCAL_BUFLEN - 1
      ld = LOCAL_BUFLEN*nproc

c   Populate GA with synthetic data (GA[i] = i)
      do i=1,LOCAL_BUFLEN
        buf(i) = dble(LOCAL_BUFLEN*me) + i
      end do

      call ga_put(g_a, ilo, ihi, 1, 1, buf, ld)
      call ga_put(g_b, ilo, ihi, 1, 1, buf, ld)
      call ga_zero(g_c)

      call bench_orig(g_a, g_b, g_c)

c     call ga_print(g_a)
c     call ga_print(g_c)

      status = ga_destroy(g_a)
      if (.not.status) call pexit('ga_destroy(A) fail')
      status = ga_destroy(g_b)
      if (.not.status) call pexit('ga_destroy(B) fail')
      status = ga_destroy(g_c)
      if (.not.status) call pexit('ga_destroy(C) fail')
      call ga_terminate()
      call mpi_finalize(ierr)

      end program simple


      subroutine bench_orig(g_a, g_b, g_c)
      implicit none
#include "global.fh"
#include "mafdecls.fh"
#include "errquit.fh"
      integer g_a, g_b, g_c, me, nproc, num_nodes, nodeid, ch, ppn
      integer ga_cnt, count, next, i, j, ilo, ihi, ld, tot_data_size
      double precision bufa(LOCAL_BUFLEN)
      double precision bufb(LOCAL_BUFLEN)
      double precision bufc(LOCAL_BUFLEN)
      double precision time1, time2, total_time
      logical status
      integer tile_dim, tile_size
      parameter (tile_dim = 3)
      parameter (tile_size = tile_dim*tile_dim)
      integer k

      time1 = ga_wtime()

          call ga_get(g_a, 1, LOCAL_BUFLEN, 1, 1, bufa, ld)
          call ga_get(g_b, 1, LOCAL_BUFLEN, 1, 1, bufb, ld)

      time2 = ga_wtime()
      total_time = time2 - time1

      write(*, 10), "bench_orig:", total_time
 10   format (a,2x,f10.6)

      return
      end


      subroutine pexit(msg)
      implicit none
      integer nc
      character*(*) msg
      print *, msg
      call exit(-1)
      end
