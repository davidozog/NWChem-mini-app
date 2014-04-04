GA = /home11/ozog/usr/local/ga-5-2
ARMCI = /home11/ozog/usr/local/ga-5-2
#GA = /home11/ozog/usr/local/ga-5-2-spawn
#GA = /home11/ozog/usr/local/ga-5-2-armci-mpi
#ARMCI = /home11/ozog/usr/local/armci-ompi
WORKQ = ./workq
BLAS = /home11/ozog/usr/local/openblas64


all:
	#mpicc -I ${GA}/include -L${GA}/lib bench.c -o bench -lga -larmci
	mpif77 -O3 -g -fdefault-integer-8 -I ${GA}/include -I ${ARMCI}/include -L${GA}/lib -L${ARMCI}/lib bench.F -L${WORKQ} -L${BLAS}/lib -o bench -lga -larmci -lworkq -lopenblas 
	mpicc -O3 -g -I ${GA}/include -I ${BLAS}/include -I ${ARMCI}/include -L${GA}/lib -L${ARMCI}/lib bench.c -L${WORKQ} -L${BLAS}/lib -o cbench -lga -larmci -lworkq -lopenblas 

tau:
	tau_f77.sh -O3 -fdefault-integer-8 -I ${GA}/include -I ${ARMCI}/include -L${GA}/lib -L${ARMCI}/lib bench.F -L${WORKQ} -L${BLAS}/lib -o bench.tau -lga -lTAU -lTauARMCIWrapper -larmci -lworkq -lopenblas
	tau_cc.sh -O3 -g -I ${GA}/include -I ${BLAS}/include -I ${ARMCI}/include -L${GA}/lib -L${ARMCI}/lib bench.c -L${WORKQ} -L${BLAS}/lib -o cbench.tau -lga -lTAU -lTauARMCIWrapper -larmci -lworkq -lopenblas
	# ARMCI-MPI
	#tau_f77.sh -O3 -fdefault-integer-8 -I ${GA}/include -I ${ARMCI}/include -L${GA}/lib -L${ARMCI}/lib bench.F -L${WORKQ} -L${BLAS}/lib -o bench.tau -lga -lTAU -larmci -lworkq -lopenblas
	#tau_cc.sh -O3 -g -I ${GA}/include -I ${BLAS}/include -I ${ARMCI}/include -L${GA}/lib -L${ARMCI}/lib bench.c -L${WORKQ} -L${BLAS}/lib -o cbench.tau -lga -lTAU -larmci -lworkq -lopenblas

	#module load ga/5.1-gcc
	#tau_cc.sh -o cbench bench.c -I/usr/common/usg/ga/5.1/gcc/include -L/usr/common/usg/ga/5.1/gcc/lib -lga -lTauARMCIWrapper -larmci -libverbs -libumad -lpthread -lm

clean:
	rm bench.o bench bench.tau cbench cbench.tau *.inst.* *.pdb *.o
