GA = /global/homes/o/ozog/usr/local/ga-5-2-gcc
#GA = /usr/common/usg/ga/5.1/gcc
WORKQ = ../workq
BLAS = /global/homes/o/ozog/usr/local/openblas64


all:
	mpif77 -g -fdefault-integer-8 -I ${GA}/include -I ${BLAS}/include -L${GA}/lib simple.F -L${WORKQ} -L${BLAS}/lib -o simple -lga -larmci -lworkq -lopenblas

tau:
	tau_f77.sh -fdefault-integer-8 -I ${GA}/include -I ${BLAS}/include -L${GA}/lib simple.F -L${WORKQ} -L${BLAS}/lib -o simple.tau -lga -lTauARMCIWrapper -larmci -lworkq -lopenblas

clean:
	rm simple.o simple simple.tau *.inst.* *.pdb
