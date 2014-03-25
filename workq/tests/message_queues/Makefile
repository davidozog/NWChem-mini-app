MPICC=cc
FTOK_PATH = "/global/homes/o/ozog/somefile"

all:
	${MPICC} -DFTOK_PATH=\"${FTOK_PATH}\" -o kirk kirk.c
	${MPICC} -DFTOK_PATH=\"${FTOK_PATH}\" -o spock spock.c
	${MPICC} -DFTOK_PATH=\"${FTOK_PATH}\" -o main main.c

clean:	
	rm kirk spock main

