      program psp
      implicit none
#include "mpif.h"
#include "mafdecls.fh"
#include "global.fh"
#include "errquit.fh"
#define LOCAL_BUFLEN 100
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

c     call bench_orig(g_a, g_b, g_c)
      call bench_workq(g_a, g_b, g_c,.true.,ga_cnt,msqids)
c     call bench_workq_mm(g_a, g_b, g_c)

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

      end program psp


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

      call ga_cluster_info(num_nodes, nodeid, ch, ppn)
      me = ga_nodeid()
      nproc = ga_nnodes()
      tot_data_size = nproc * LOCAL_BUFLEN

c   Create a global counter for dynamic load balancing
      status = nga_create(MT_INT, 1, 1, 'counter', 1, ga_cnt)
      if (.not.status) call pexit('get_task() fail')
      CALL ga_zero(ga_cnt)

      count = 0
      next = nga_read_inc(ga_cnt, 1, 1)
      do i=0,tot_data_size/tile_size-1
        if (next.eq.count) then
          ilo = next*tile_size + 1
          ihi = ilo + tile_size - 1
          ld = LOCAL_BUFLEN*nproc
  
          call ga_get(g_a, ilo, ihi, 1, 1, bufa, ld)
          call ga_get(g_b, ilo, ihi, 1, 1, bufb, ld)

          call dgemm( 'T', 'N', tile_dim, tile_dim, tile_dim, 1.0d0, 
     &                 bufa, tile_dim, bufb, tile_dim, 1.0, bufc, 
     &                 tile_dim)

          call ga_put(g_c, ilo, ihi, 1, 1, bufc, tile_dim)

          next = nga_read_inc(ga_cnt, 1, 1)
        end if
        count = count + 1
      end do

      time2 = ga_wtime()
      total_time = time2 - time1

      if(me.eq.0) write(*, 10), "bench_orig:", total_time
 10   format (a,2x,f10.6)

      return
      end


      subroutine bench_workq(g_a, g_b, g_c, init, ga_cnt, msqids)
      implicit none
#include "global.fh"
#include "mafdecls.fh"
#include "errquit.fh"
      integer g_a, g_b, g_c, me, nproc, num_nodes, nodeid, ch, ppn
      integer ga_cnt, count, next, i, j, ilo, ihi, ld, tot_data_size
      double precision bufa(LOCAL_BUFLEN)
      double precision bufb(LOCAL_BUFLEN)
      double precision time1, time2, total_time
      logical status, a_collector, am_collector, init
      integer num_collectors, msqids(7), sem, collector
      integer tile_dim, tile_size
      parameter (tile_dim = 3)
      parameter (tile_size = tile_dim*tile_dim)

      time1 = ga_wtime()
      sem = 0

      call ga_cluster_info(num_nodes, nodeid, ch, ppn)
      me = ga_nodeid()
      nproc = ga_nnodes()
      tot_data_size = nproc * LOCAL_BUFLEN

c     The zeroeth proc on each node is that node's collector:
      num_collectors = 1
      collector = me - mod(me,ppn)
      a_collector = am_collector(me, collector, num_collectors) 

      if (init) then 
        if (me.eq.collector) then
          CALL work_queue_create(msqids, me, ppn)
          CALL multicast_msqids(msqids, ppn)
          CALL work_queue_sem_init(ppn)
        else
          CALL recv_msqids(msqids, nodeid, ppn)
          CALL recv_dataqids(nodeid, ppn)
        end if
      end if


c   Create a global counter for dynamic load balancing
      if (init) then
        status = nga_create(MT_INT, 1, 1, 'counter', 1, ga_cnt)
        if (.not.status) call pexit('get_task() fail')
        CALL ga_zero(ga_cnt)
      end if

      count = 0
      next = nga_read_inc(ga_cnt, 1, 1)

c     CALL work_queue_sem_getvalue(sem, nodeid)
      CALL work_queue_sem_post(nodeid)

      do i=0,tot_data_size/tile_size-1
        if (next.eq.count) then

c         CALL work_queue_sem_getvalue(sem, nodeid)
c         print *, "sem value = ", sem

          CALL work_queue_alloc_task(next, tile_size*2)

          ilo = next*tile_size + 1
          ihi = ilo + tile_size - 1
          ld = LOCAL_BUFLEN*nproc
  
c         call ga_get(g_a, ilo, ihi, 1, 1, bufa, ld)
c         call ga_get(g_b, ilo, ihi, 1, 1, bufb, ld)
          call work_queue_append_task_single(next, tile_dim, g_a, g_b, 
     &                                       ld, bufa, bufb)
          call work_queue_add_single(msqids, next, tile_dim, me, nodeid,
     &                         ppn, a_collector)

          CALL work_queue_sem_release(nodeid)

          if (.not.a_collector) then
              CALL worker(g_a, g_b, g_c, ga_cnt, msqids, num_collectors)
              return
          end if 

          next = nga_read_inc(ga_cnt, 1, 1)
          CALL work_queue_sem_post(nodeid)
        end if
        count = count + 1
      end do

      if(me.eq.0) write(*,11), "total tasks: ", count
 11   format (a,2x,i5)

      CALL work_queue_sem_release(nodeid)
      CALL work_queue_sem_wait()
      time2 = ga_wtime()
      total_time = time2 - time1

      if (me.eq.collector) then
        CALL work_queue_end_taskgroup(msqids, ppn, count)

      end if

      CALL worker(g_a, g_b, g_c, ga_cnt, msqids, num_collectors)

      if(me.eq.0) write(*, 10), "bench_workq:", total_time
 10   format (a,2x,f10.6)

      return
      end


      subroutine bench_workq_mm(g_a, g_b, g_c)
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
      integer num_collectors, msqids(7), sem, collector
      integer tile_dim, tile_size
      parameter (tile_dim = 3)
      parameter (tile_size = tile_dim*tile_dim)

      time1 = ga_wtime()
      sem = 0

      call ga_cluster_info(num_nodes, nodeid, ch, ppn)
      me = ga_nodeid()
      nproc = ga_nnodes()
      tot_data_size = nproc * LOCAL_BUFLEN

c     The zeroeth proc on each node is that node's collector:
      num_collectors = 1
      collector = me - mod(me,ppn)
      if (me.eq.collector) then
        CALL work_queue_create(msqids, me, ppn)
        CALL multicast_msqids(msqids, ppn)
        CALL work_queue_sem_init(ppn)
      else
        CALL recv_msqids(msqids, nodeid, ppn)
        CALL recv_dataqids(nodeid, ppn)
      end if


c   Create a global counter for dynamic load balancing
      status = nga_create(MT_INT, 1, 1, 'counter', 1, ga_cnt)
      if (.not.status) call pexit('get_task() fail')
      CALL ga_zero(ga_cnt)

      count = 0
      next = nga_read_inc(ga_cnt, 1, 1)

      CALL work_queue_sem_getvalue(sem, nodeid)
      CALL work_queue_sem_post(nodeid)

      do i=0,tot_data_size/tile_size-1
        if (next.eq.count) then

          CALL work_queue_sem_getvalue(sem, nodeid)
c         print *, "sem value = ", sem

          CALL work_queue_alloc_task(next, tile_size)

          ilo = next*tile_size + 1
          ihi = ilo + tile_size - 1
          ld = LOCAL_BUFLEN*nproc
  
          call ga_get(g_a, ilo, ihi, 1, 1, bufa, ld)
          call ga_get(g_b, ilo, ihi, 1, 1, bufb, ld)

          call dgemm( 'T', 'N', tile_dim, tile_dim, tile_dim, 1.0d0, 
     &                 bufa, tile_dim, bufb, tile_dim, 1.0, bufc, 
     &                 tile_dim)

          call ga_put(g_c, ilo, ihi, 1, 1, bufc, tile_dim)

          CALL work_queue_sem_release(nodeid)
          next = nga_read_inc(ga_cnt, 1, 1)
          CALL work_queue_sem_post(nodeid)
        end if
        count = count + 1
      end do

      CALL work_queue_sem_release(nodeid)
      CALL work_queue_sem_wait()
      time2 = ga_wtime()
      total_time = time2 - time1

      if (me.eq.collector) then
        CALL work_queue_end_taskgroup(msqids, ppn, count)

! TODO: put this in the finished worker's finalize subroutine
        CALL work_queue_destroy(msqids)
        CALL work_queue_destroy_sem()
      else
c     TODO: Destroy all data when application is finished...
c       CALL work_queue_destroy_data(shmid)
      end if

      if(me.eq.0) write(*, 10), "bench_workq_mm:", total_time
 10   format (a,2x,f10.6)

      return
      end
      

      subroutine worker(g_a, g_b, g_c, ga_cnt, msqids, num_collectors)
      implicit none
#include "global.fh"
      integer g_a, g_b, g_c, ga_cnt, msqids(7), num_collectors, nproc
      integer me, num_nodes, nodeid, collector, collector_host, ppn
      integer more_tasks,data_size,task_id,qlen,qid,nc,data_id,key,shmid
      integer a,b,c,d,tile_dim,tile_size,e,f,i
      double precision bufc(LOCAL_BUFLEN)
      integer ilo, ihi, ld
      character*(256) hostname
      integer j

      me = ga_nodeid()
      nproc = ga_nnodes()
      CALL ga_cluster_info(num_nodes, nodeid, collector_host, ppn)
      collector = me - mod(me,ppn)

      more_tasks = 1
      data_size = 0
      tile_size = 0
      tile_dim = 0
      task_id = 0
      key = 0
      shmid = 0
      data_id = 0
      qlen = 0
      qid = 0


      do while (more_tasks.gt.0)

        call work_queue_get_max_qlen(nodeid, msqids, qlen, qid)
        if (qlen.eq.0) then
c         print *, "R:", me, "QLEN IS ZERO"
          call bench_workq(g_a, g_b, g_c,.false.,ga_cnt,msqids)
          return
        end if

        call work_queue_get_info(msqids, qid, data_id, shmid,  
     &    more_tasks, data_size, tile_dim, a, b, c, d, key, task_id, 
     &    me, ppn)

        IF (more_tasks.gt.0) THEN
          do i = 1, more_tasks
            call work_queue_get_next_single(me, data_id, task_id, 
     &                                    tile_dim, i)

            
            do j=1,LOCAL_BUFLEN
              bufc(j) = 0.0d0
            end do
            call work_queue_execute_task_single(bufc,
     &                tile_dim, 1.0d0, 2.0d0,'T','N',me)

            tile_size = tile_dim*tile_dim
            ilo = task_id*tile_size + 1
            ihi = ilo + tile_size - 1
            ld = LOCAL_BUFLEN*nproc

            call ga_put(g_c, ilo, ihi, 1, 1, bufc, tile_dim)

          end do

        call work_queue_free_shm(shmid)
        END IF

      end do

      CALL bench_finalize(me, collector, nc, msqids)

c     call dgemm( 'T', 'N', tile_dim, tile_dim, tile_dim, 1.0d0, 
c    &             bufa, tile_dim, bufb, tile_dim, 1.0, bufc, 
c    &             tile_dim)

c     call ga_put(g_c, ilo, ihi, 1, 1, bufc, tile_dim)


      return
      end


      subroutine bench_finalize(me, collector, n_collectors, msqids)
      implicit none
#include "global.fh"
      integer me, collector, dummy, n_collectors
      integer msqids(7)

      call ga_sync()

      if (me.eq.collector) then
        call work_queue_destroy(msqids)
        call work_queue_destroy_sem()
      else
c     todo: destroy all data when application is finished...
c       call work_queue_destroy_data(shmid)
      end if

      return
      end
      

      subroutine pexit(msg)
      implicit none
      integer nc
      character*(*) msg
      print *, msg
      call exit(-1)
      end
