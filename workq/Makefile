GA = /home/dave/usr/local/ga-5-2

all:
	#mpicc -c work_queue_add.c
	#mpicc -c work_queue_create.c
	#mpicc -c work_queue_destroy.c
	#mpicc -c work_queue_get.c
	mpicc -Wall -fPIC -c *.c
	gfortran -fPIC -c -fdefault-integer-8 -I ${GA}/include -L${GA}/lib ga_cluster_info.F
	mpicc -shared -Wl,-soname,libworkq.so.1 -o libworkq.so.1.0   *.o
	ln -sf libworkq.so.1.0 libworkq.so.1
	ln -sf libworkq.so.1.0 libworkq.so
	

clean:
	rm *.o
	rm libworkq*
