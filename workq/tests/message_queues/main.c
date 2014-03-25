#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <mpi.h>

#ifdef FTOK_PATH
  #define FTOK_FILEPATH FTOK_PATH
#endif


struct my_msgbuf {
	long mtype;
	char mtext[200];
};

int spock()
{
	struct my_msgbuf buf;
	int msqid;
	key_t key;

	if ((key = ftok(FTOK_FILEPATH, 'B')) == -1) {  /* same key as kirk.c */
		perror("ftok");
		exit(1);
	}

	printf("spock: permission to come aboard, captain.\n");
	sleep(1);

	if ((msqid = msgget(key, 0644)) == -1) { /* connect to the queue */
		perror("msgget");
		exit(1);
	}
	
	printf("spock: ready to receive messages, captain.\n");

	for(;;) { /* Spock never quits! */
		if (msgrcv(msqid, &buf, sizeof(buf.mtext), 0, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}
		printf("spock: \"%s\"\n", buf.mtext);
		
		buf.mtype = 1;
		buf.mtext[0] = 'S';
		buf.mtext[1] = '\0';
		int len = strlen(buf.mtext);
		
		if (msgsnd(msqid, &buf, len+1, 0) == -1) /* +1 for '\0' */
			perror("msgsnd");
		break;

	}

	return 0;
}

int kirk()
{
	struct my_msgbuf buf;
	int msqid;
	key_t key;

	if ((key = ftok(FTOK_FILEPATH, 'B')) == -1) {
		perror("ftok");
		exit(1);
	}

	if ((msqid = msgget(key, 0644 | IPC_CREAT)) == -1) {
		perror("msgget");
		exit(1);
	}
	sleep(2);
	
	printf("Enter lines of text, ^D to quit:\n");

	buf.mtype = 1; /* we don't really care in this case */

	//while(fgets(buf.mtext, sizeof buf.mtext, stdin) != NULL) {
	buf.mtext[0] = 'K';
	buf.mtext[1] = '\0';
		int len = strlen(buf.mtext);

		/* ditch newline at end, if it exists */
		if (buf.mtext[len-1] == '\n') buf.mtext[len-1] = '\0';

		if (msgsnd(msqid, &buf, len+1, 0) == -1) /* +1 for '\0' */
			perror("msgsnd");
	//}
//	sleep(4);
	for(;;) {
		if (msgrcv(msqid, &buf, sizeof(buf.mtext), 0, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}
		printf("kirk: \"%s\"\n", buf.mtext);
		if (buf.mtext[0] != 'K') break;
	}

	if (msgctl(msqid, IPC_RMID, NULL) == -1) {
		perror("msgctl");
		exit(1);
	}

	return 0;
}


int main(int argc, char *argv[]) {
	
	int rank, size;
	MPI_Init(&argc, &argv);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank);  
  	MPI_Comm_size (MPI_COMM_WORLD, &size);  

	printf("starting...\n");
	if (rank == 0) kirk();
	if (rank == 1) spock();
	printf("finished...\n");

	MPI_Finalize();
	return 0;
}
