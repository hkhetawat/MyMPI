#include "mympi.h"

int serverSocket;
int rank;
int total;
char *ipAddress;
struct sockaddr_in serverAddr;
pthread_t *serverThread;
pthread_mutex_t messageListLock;
pthread_cond_t *foundCondition;
int *sockets;

struct peerNode *nodes;
struct message *messageList;
struct message *requestList;

//Add the receive request to the list in case the MPI_Recv is called before the actual message comes.
void addRequestToList(struct message *request)
{
	if(requestList == NULL)
		requestList = request;
	else
	{
		struct message *temp = requestList;
		while(temp->next != NULL)
		{
			temp = temp->next;
		}
		temp->next = request;
	}
}

//Retrieve the request characterized by the given parameters.
struct message *getRequestFromList(int size, int tag, int type)
{
	if(requestList == NULL)
		return NULL;
	struct message *temp = requestList;
	if(temp->size == size && temp->tag == tag && temp->type == type)
	{
		requestList = requestList->next;
		return temp;
	}
	while(temp->next != NULL)
	{
		if(temp->next->size == size && temp->next->tag == tag && temp->next->type == type)
		{
			break;
		}
	}
	struct message *returnMessage = temp->next;
	if(temp->next != NULL)
		temp->next = temp->next->next;
	return returnMessage;
}

//Add the message to the list in case the MPI_Recv is called after the actual message comes.
void addMessageToList(struct message *recvdMessage)
{
	if(messageList == NULL)
		messageList = recvdMessage;
	else
	{
		struct message *temp = messageList;
		while(temp->next != NULL)
		{
			temp = temp->next;
		}
		temp->next = recvdMessage;
	}
}

//Retrieve the message characterized by the given parameters.
struct message *getMessageFromList(int source, int size, int type, int tag)
{
	if(messageList == NULL)
		return NULL;
	struct message *temp = messageList;
	if(temp->size == size && temp->tag == tag && temp->type == type && temp->source == source)
	{
		messageList = messageList->next;
		return temp;
	}
	while(temp->next != NULL)
	{
		if(temp->next->size == size && temp->next->tag == tag && temp->next->type == type && temp->next->source == source)
		{
			break;
		}
	}
	struct message *returnMessage = temp->next;
	if(temp->next != NULL)
		temp->next = temp->next->next;
	return returnMessage;
}

//Thread function to listen for messages from each peer.
void *listenForMessages(void* arg)
{
	int notDone = 1;
	int listenRank = *((int *)arg);
	do
	{
		int buffer[3] = {0};
		read(sockets[listenRank], buffer, sizeof(int) * 3);
		if(buffer[2] == 0)
		{
			notDone = 0;
			pthread_exit(NULL);
			break;
		}
		struct message *request = getRequestFromList(buffer[0], buffer[1], buffer[2]);
		if(request == NULL)			//Message comes before MPI_Recv called.
		{
			struct message *recvdMessage = (struct message *)malloc(sizeof(struct message));
			recvdMessage->size = buffer[0];
			recvdMessage->tag = buffer[1];
			recvdMessage->type = buffer[2];
			recvdMessage->source = listenRank;
			recvdMessage->next = NULL;
			recvdMessage->data = malloc(recvdMessage->size * recvdMessage->type);
			read(sockets[listenRank], recvdMessage->data, recvdMessage->size * recvdMessage->type);
			pthread_mutex_lock(&messageListLock);
			addMessageToList(recvdMessage);
			pthread_mutex_unlock(&messageListLock);
		}
		else						//MPI_Recv called before message comes.
		{
			request->data = malloc(request->size * request->type);
			read(sockets[listenRank], request->data, request->size * request->type);
			pthread_mutex_lock(&messageListLock);
			pthread_cond_signal(&(foundCondition[listenRank]));
			pthread_mutex_unlock(&messageListLock);
		}
	}
	while(1);
	return NULL;
}

int MPI_Send(void *buf, int count, int datatype, int dest, int tag, int MPI_comm)
{
	int buffer[3];
	buffer[0] = count;
	buffer[1] = tag;
	buffer[2] = datatype;
	write(sockets[dest], buffer, sizeof(int) * 3);
	write(sockets[dest], buf, datatype * count);
	return 0;
}

int MPI_Recv(void *buf, int count, int datatype, int source, int tag, int MPI_comm, void *status)
{
	struct message *foundMessage;
	pthread_mutex_lock(&messageListLock);
	struct message request;
	foundMessage = getMessageFromList(source, count, datatype, tag);
	while(foundMessage == NULL)			//MPI_Recv called before message comes.
	{
		request.data = NULL;
		request.size = count;
		request.tag = tag;
		request.type = datatype;
		request.source = source;
		request.next = NULL;
		addRequestToList(&request);
		pthread_cond_wait(&(foundCondition[source]), &messageListLock);				//Wait for corresponding message to come.
		if(request.data != NULL)
		{
			foundMessage = &request;
		}
	}
	pthread_mutex_unlock(&messageListLock);
	memcpy(buf, foundMessage->data, foundMessage->size * foundMessage->type);
	return 0;
}

void setupServerSocket()
{
    serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	int port = 10000;
	do
	{
	    serverAddr.sin_family = AF_INET;
	    serverAddr.sin_port = htons(port);
	    serverAddr.sin_addr.s_addr = inet_addr(ipAddress);
	    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  
		port++;
	}
    while(bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1);
}

//Populate list of peers from the endpoints file.
void populatePeerList()
{
	int i = 0, bufPos = 0, port = 0, rank = 0;
	FILE *fp = fopen("endpoints", "r");
	char c;
	c = fgetc(fp);
	while(c != EOF)
	{
		while(c != ':')
		{
			rank = rank * 10 + (c - '0');
			c = fgetc(fp); 
		}
		nodes[i].rank = rank;
		c = fgetc(fp);
		while(c != ':')
		{
			nodes[i].IP[bufPos++] = c;
			c = fgetc(fp);
		}
		nodes[i].IP[bufPos] = '\0';
		c = fgetc(fp);
		while(c != '\n' && c != EOF)
		{
			port = port * 10 + (c - '0');
			c = fgetc(fp);
		}
		nodes[i].port = port;
		if(c == EOF)
		{
			fclose(fp);
			break;
		}
		else
		{
			port = 0;
			rank = 0;
			bufPos = 0;
			i++;
			c = fgetc(fp);
			continue;
		}
	}
}

//Last node to write to the endpoints file sends a message to all other nodes informing them that all nodes have entered the system.
void sendOk()
{
	struct sockaddr_in serverAddr;
	int buffer;
	int i;
	for(i = 0; i<total; i++)
	{
		if(i == rank)
			continue;
		int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
		buffer = rank;
	    serverAddr.sin_family = AF_INET;
	    serverAddr.sin_port = htons(nodes[i].port);
	    serverAddr.sin_addr.s_addr = inet_addr(nodes[i].IP);
	    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
		connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
		write(clientSocket, &buffer, sizeof(int));
		sockets[i] = clientSocket;
	}
}

//Nodes wait for all the nodes to enter the system.
void waitForOk()
{
	int buffer;
	struct sockaddr_in clientAddr;
	socklen_t addr_size;
	addr_size = sizeof(clientAddr);
	listen(serverSocket, total);
	int confd = accept(serverSocket, (struct sockaddr *)&clientAddr, &addr_size);
	read(confd, &buffer, sizeof(int));
	sockets[buffer] = confd;
}

//Add information about itself to the endpoints file.
void doFileWork()
{
	int nodeNumber = 0;
	char buffer[20];
	FILE *fp = fopen("endpoints", "r");
	if(fp != NULL)
	{
		char c;
		while((c = fgetc(fp)) != EOF)
		{
			if(c == '\n')
			{
				nodeNumber++;
			}
		}
		fclose(fp);
	
		fp = fopen("endpoints", "a");
		fprintf(fp, "%d:%s:%d\n", rank, ipAddress, ntohs(serverAddr.sin_port));
		fclose(fp);
	}
	else
	{
		fp = fopen("endpoints", "w");
		fprintf(fp, "%d:%s:%d\n", rank, ipAddress, ntohs(serverAddr.sin_port));
		fclose(fp);
	}
	if(nodeNumber + 1 == total)
	{
		populatePeerList();
		sendOk();
	}
	else
	{
		waitForOk();
		populatePeerList();
	}
}

//Connect to nodes ranked lower than itself.
void connectToLower()
{
	struct sockaddr_in serverAddr;
	int buffer;
	int i;
	for(i = rank - 1; i>=0; i--)
	{
		int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
		buffer = rank;
	    serverAddr.sin_family = AF_INET;
	    serverAddr.sin_port = htons(nodes[i].port);
	    serverAddr.sin_addr.s_addr = inet_addr(nodes[i].IP);
	    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
		connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
		write(clientSocket, &buffer, sizeof(int));
		sockets[i] = clientSocket;
	}
}

//Accept connections from nodes ranked higher than itself.
void acceptFromUpper()
{
	int i;
	for(i = total - 2; i>rank; i--)
	{
		int buffer;
		struct sockaddr_in clientAddr;
		socklen_t addr_size;
		addr_size = sizeof(clientAddr);
		listen(serverSocket, total);
		int confd = accept(serverSocket, (struct sockaddr *)&clientAddr, &addr_size);
		read(confd, &buffer, sizeof(int));
		sockets[buffer] = confd;
	}	
}

int MPI_Comm_size(int MPI_Comm, int *size)
{
	*size = total;
	return 0;
}

int MPI_Comm_rank(int MPI_Comm, int *worldRank)
{
	*worldRank = rank;
	return 0;
}

int MPI_Get_processor_name(char *name, int *resultlen)
{
	*resultlen = strlen(ipAddress);
	name = (char *)malloc(sizeof(char) * (*resultlen) + 1);
	strcpy(name, ipAddress);
	return 0;
}

int MPI_Init(int *argc, char **argv[])
{
	struct hostent *host;
	struct in_addr **addList;
	int i;
	char *ipAddressCompute, *hostName;
	rank = atoi((*argv)[1]);
	total = atoi((*argv)[2]);
	ipAddressCompute = (*argv)[3];
        ipAddress = (char *)malloc(sizeof(char)*40);
	hostName = (char *)malloc(sizeof(char)*40);
        sprintf(hostName, "compute-storage-%s", ipAddressCompute+8);
	host = gethostbyname(hostName);
	addList = (struct in_addr **)host->h_addr_list;
	strcpy(ipAddress, inet_ntoa(*addList[0]));
	nodes = (struct peerNode *)malloc(sizeof(struct peerNode) * total);
	foundCondition = (pthread_cond_t *)malloc(sizeof(pthread_cond_t) * total);
	serverThread = (pthread_t *)malloc(sizeof(pthread_t) * total);
	messageList = NULL;
	requestList = NULL;
	sockets = (int *)malloc(sizeof(int) * total);
	pthread_mutex_init(&messageListLock, NULL);
	for(i = 0; i<total; i++)
	{
		sockets[i] = 0;
		pthread_cond_init(&(foundCondition[i]), NULL);
	}
	setupServerSocket();
	doFileWork();
	acceptFromUpper();
	if(rank != total -1)
		connectToLower();
	for(i = 0; i<total; i++)
	{
		int *listenRank = (int *)malloc(sizeof(int));
		*listenRank = i;
		if(i != rank)
			pthread_create(&(serverThread[i]), NULL, listenForMessages, listenRank);
	}
	return 0;
}

double MPI_Wtime()
{
	struct timeval tv;
	uint64_t timeInMicroseconds;
	double time;
    gettimeofday(&tv,NULL);
    timeInMicroseconds = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
    time = (double)timeInMicroseconds/(double)1000000;
    return time;
}

int MPI_Finalize()
{
	int i;
	int buf[3] = {0};
	for(i = 0; i<rank; i++)
	{
		if(rank != i)
		{
			write(sockets[i], buf, sizeof(int) * 3);
		}
	}
	return 0;
}
