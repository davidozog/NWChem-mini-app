GA = /global/homes/o/ozog/usr/local/ga-5-2-gcc
WORKQ = ./workq
BLAS = /global/homes/o/ozog/usr/local/openblas64


all:
	#mpicc -I ${GA}/include -L${GA}/lib bench.c -o bench -lga -larmci
	mpif77 -g -fdefault-integer-8 -I ${GA}/include -L${GA}/lib bench.F -L${WORKQ} -L${BLAS}/lib -o bench -lga -larmci -lworkq -lopenblas
	mpicc -g -I ${GA}/include -I ${BLAS}/include -L${GA}/lib bench.c -L${WORKQ} -L${BLAS}/lib -o cbench -lga -larmci -lworkq -lopenblas

tau:
	tau_f77.sh -fdefault-integer-8 -I ${GA}/include -L${GA}/lib bench.F -L${WORKQ} -L${BLAS}/lib -o bench.tau -lga -larmci -lworkq -lopenblas
	tau_cc.sh -g -I ${GA}/include -I ${BLAS}/include -L${GA}/lib bench.c -L${WORKQ} -L${BLAS}/lib -o cbench.tau -lga -lTauARMCIWrapper -larmci -lworkq -lopenblas

clean:
	rm bench.o bench bench.tau cbench cbench.tau
