NWChem-mini-app
===============

These Global Arrays programs emulate the Coupled Cluster/Tensor Contraction Engine's behavior in NWChem for the purpose of
understanding performance and experimenting with alternate execution models.

There are two versions, one written in Fortran 77 and the other in C.  Both contain a function called bench\_orig()
which emulates NWChem's CC/TCE execution model (Get(A), Get(B), DGEMM, Put(C), repeat).  Currently, there is an on-node 
message queue version working in bench.F (bench\_workq), which can be compared to the original NWChem execution model.
Only the Fortran code can currently use the "workq/" library.

The C code (bench.c) contains a version that uses non-blocking calls to GA\_Get and GA\_Put.  
