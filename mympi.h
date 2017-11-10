#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <pthread.h>
#define MPI_DOUBLE sizeof(double)
#define MPI_INTEGER sizeof(int)
#define MPI_INT sizeof(int)
#define MPI_BYTE 1
#define MPI_COMM_WORLD 1
#define MPI_MAX_PROCESSOR_NAME 40
#define MPI_STATUS_IGNORE NULL

//Structure for messages and requests.
struct message
{
	void *data;
	int size;
	int tag;
	int source;
	int type;
	struct message *next;
};

//Structure to represent each peer.
struct peerNode
{
	int rank;
	char IP[40];
	int port;
};

int MPI_Init(int *argc, char **argv[]);
int MPI_Comm_size(int MPI_Comm, int *size);
int MPI_Comm_rank(int MPI_Comm, int *worldRank);
int MPI_Get_processor_name(char *name, int *resultlen);
int MPI_Send(void *buf, int count, int datatype, int dest, int tag, int MPI_comm);
int MPI_Recv(void *buf, int count, int datatype, int source, int tag, int MPI_comm, void *status);
int MPI_Finalize();
double MPI_Wtime();
