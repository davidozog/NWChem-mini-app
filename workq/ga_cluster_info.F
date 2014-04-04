      subroutine ga_cluster_info(num_nodes, nodeid, counter_host, ppn)
      implicit none
#include "global.fh"
#include "mafdecls.fh"
      integer num_nodes, nodeid, counter_host, ppn, me
      CHARACTER(len=255) :: local_size
      double precision t2, t1

c     PPN hack (ompi only):
      CALL getenv("OMPI_COMM_WORLD_LOCAL_SIZE", local_size)
      read (local_size,*) ppn 

c     NODEID hack:
c     t1 = ga_wtime()
c     CALL node_info(nodeid)
c     t2 = ga_wtime()
c10   format (a,2x,f10.6)
c     if(me.eq.0) write(*, 10), "node_info:", t2 - t1

c     FASTER NODEID hack:
      me = ga_nodeid()
      num_nodes = ga_cluster_nnodes()
      num_nodes = num_nodes / ppn
      nodeid = me / ppn

c     nodeid = ga_cluster_nodeid()
c     ppn = ga_cluster_nprocs(nodeid)


c     print *, "num_nodes:", num_nodes
c     print *, "nodeid:", nodeid
c     print *, "ppn:", ppn 
c     print *, "me:", me 

      counter_host = me/ppn + 1

      return
      end

      FUNCTION am_collector(me, collector, num_collectors)
      IMPLICIT NONE
      LOGICAL am_collector
      INTEGER me, collector, num_collectors, i

      am_collector = .false.

      DO i=0,num_collectors-1
        IF (me.eq.collector+i) am_collector = .true.
      END DO
      RETURN 
      END FUNCTION am_collector

