      program psp
      implicit none
#include "mpif.h"
#include "mafdecls.fh"
#include "global.fh"
#include "errquit.fh"
#define LOCAL_BUFLEN 10000000
      integer ierr, me, nproc, heap, stack, total_size
      integer g_a, g_b, g_c, chunk, i, ihi, ilo, jhi, jlo, ld
      double precision buf(LOCAL_BUFLEN)
      logical status

      heap =  96000000
      stack = 96000000

      call mpi_init(ierr)
      call ga_initialize()

      me = ga_nodeid()
      nproc = ga_nnodes()
      total_size = nproc * LOCAL_BUFLEN

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

      call bench(g_a, g_b, g_c)

      status = ga_destroy(g_a)
      if (.not.status) call pexit('ga_destroy(A) fail')
      status = ga_destroy(g_b)
      if (.not.status) call pexit('ga_destroy(B) fail')
      status = ga_destroy(g_c)
      if (.not.status) call pexit('ga_destroy(C) fail')
      call ga_terminate()
      call mpi_finalize(ierr)

      end program psp


      subroutine bench(g_a, g_b, g_c)
      implicit none
#include "global.fh"
#include "mafdecls.fh"
#include "errquit.fh"
      integer g_a, g_b, g_c, me, nproc, num_nodes, nodeid, ch, ppn
      integer ga_cnt, count, next, i, j, ilo, ihi, ld, total_size
      double precision bufa(LOCAL_BUFLEN)
      double precision bufb(LOCAL_BUFLEN)
      double precision bufc(LOCAL_BUFLEN)
      logical status

      call ga_cluster_info(num_nodes, nodeid, ch, ppn)
      nproc = ga_nnodes()
      total_size = nproc * LOCAL_BUFLEN

c   Create a global counter for dynamic load balancing
      status = nga_create(MT_INT, 1, 1, 'counter', 1, ga_cnt)
      if (.not.status) call pexit('get_task() fail')
      CALL ga_zero(ga_cnt)

      count = 0
      next = nga_read_inc(ga_cnt, 1, 1)
      do i=0,total_size/4-1
        if (next.eq.count) then
          ilo = next*4 + 1
          ihi = ilo + 4 - 1
          ld = LOCAL_BUFLEN*nproc
  
          call ga_get(g_a, ilo, ihi, 1, 1, bufa, ld)
          call ga_get(g_b, ilo, ihi, 1, 1, bufb, ld)
          call ga_get(g_c, ilo, ihi, 1, 1, bufc, ld)

          call dgemm( 'T', 'N', 2, 2, 2, 1.0d0, bufa, 2,
     &                 bufb, 2, 1.0, bufc, 2)

          call ga_put(g_c, ilo, ihi, 1, 1, bufc, 2)

          next = nga_read_inc(ga_cnt, 1, 1)
        end if
        count = count + 1
      end do

c     call ga_print(g_a)
c     call ga_print(g_c)

      return
      end

      
      subroutine pexit(msg)
      implicit none
      integer nc
      character*(*) msg
      print *, msg
      call exit(-1)
      end
