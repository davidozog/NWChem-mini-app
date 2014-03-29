GA = /home/dave/usr/local/ga-5-2
BLAS = /home/dave/usr/local/openblas64
FTOKM = "/home/dave/sand/somefile"
FTOKD = "/home/dave/sand/datafile"

all:
	#mpicc -c work_queue_add.c
	#mpicc -c work_queue_create.c
	#mpicc -c work_queue_destroy.c
	#mpicc -c work_queue_get.c
	mpicc -g -Wall -fPIC -DFTOKM=\"${FTOKM}\" -DFTOKD=\"${FTOKD}\" -c *.c -I ${GA}/include -I ${BLAS}/include -L${GA}/lib -larmci -lopenblas
	gfortran -g -fPIC -c -fdefault-integer-8 -I ${GA}/include -L${GA}/lib ga_cluster_info.F -larmci
	mpicc -g -shared -Wl,-soname,libworkq.so.1 -o libworkq.so.1.0   *.o -L${GA}/lib -L${BLAS}/lib -lga -larmci -lopenblas
	ln -sf libworkq.so.1.0 libworkq.so.1
	ln -sf libworkq.so.1.0 libworkq.so
	
tau:
	#mpicc -c work_queue_add.c
	#mpicc -c work_queue_create.c
	#mpicc -c work_queue_destroy.c
	#mpicc -c work_queue_get.c
	tau_cc.sh -Wall -fPIC -DFTOKM=\"${FTOKM}\" -DFTOKD=\"${FTOKD}\" -c *.c -I ${GA}/include -I ${BLAS}/include -L${GA}/lib -L${BLAS}/lib -lga -lTauARMCIWrapper -larmci -lopenblas
	tau_f77.sh -fPIC -c -fdefault-integer-8 -I ${GA}/include -I ${BLAS}/include -L${GA}/lib -L${BLAS}/lib ga_cluster_info.F -lga -lTauARMCIWrapper -larmci -lopenblas
	tau_cc.sh -shared -Wl,-soname,libworkq.so.1 -o libworkq.so.1.0  *.o -L${GA}/lib -L${BLAS}/lib -lga -lTauARMCIWrapper -larmci -lopenblas
	ln -sf libworkq.so.1.0 libworkq.so.1
	ln -sf libworkq.so.1.0 libworkq.so
	

clean:
	rm *.o libworkq* *.inst.* *.pdb
