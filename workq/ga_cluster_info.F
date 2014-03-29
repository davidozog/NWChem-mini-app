      subroutine ga_cluster_info(num_nodes, nodeid, counter_host, ppn)
      implicit none
#include "global.fh"
#include "mafdecls.fh"
      integer num_nodes, nodeid, counter_host, ppn, me
c     CHARACTER(len=32) :: ppn_str

      num_nodes = ga_cluster_nnodes()
      nodeid = ga_cluster_nodeid()
      ppn = ga_cluster_nprocs(nodeid)
      me = ga_nodeid()

c     CALL getenv("PROCS_PER_NODE", ppn_str)
c     read (ppn_str,*) ppn
c     FASTER NODEID hack:
      num_nodes = ga_cluster_nnodes()
      num_nodes = num_nodes / ppn
      nodeid = me / ppn

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

