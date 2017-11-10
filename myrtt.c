#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mympi.h"


//Function to calculate and return average RTT and standard deviation
void calcRTT(int size, int source, int destination, double *average, double *standardDeviation)
{
	void *buf = malloc(sizeof(char) * size);
	double rtts[9];
	double sum = 0.0;
	double variance = 0.0;
	double iterations = 9.0;
	int i, j, k;
	for(i = 0; i<10; i++)
	{
		double starttime = MPI_Wtime();
		MPI_Send(buf, size, MPI_BYTE, destination, 0, MPI_COMM_WORLD);
		MPI_Recv(buf, size, MPI_BYTE, destination, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		double endtime = MPI_Wtime();
		if(i != 0)
		{
			rtts[i-1] = (endtime - starttime);
		}
	}
	for(j = 0; j<9; j++)
	{
		sum += rtts[j];
	}
	*average = sum/iterations;
	for(k = 0; k<9; k++)
	{
		variance = variance + (*average - rtts[k])*(*average - rtts[k]);
	}
	variance = variance/iterations;
	*standardDeviation = sqrt(variance);
}

int main (int argc, char *argv[])
{
	int   numproc, rank, len;
	char  hostname[MPI_MAX_PROCESSOR_NAME];
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numproc);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(hostname, &len);

	int i, j, k, l, m, n;
	if (rank == 0)
	{
		//2-D array saves the size and latency values for each pair
		double **pairRTTTable = (double **)malloc(sizeof(double *) * (numproc/2));
		for(i = 0; i < (numproc/2); i++)
		{
			pairRTTTable[i] = (double *)malloc(sizeof(double) * 14);
		}
		int initialSize = 16;

		//Calculates latency values for 1st pair
		for(j = 0; j < 7; j++)
		{
			calcRTT(initialSize<<j, 0, 1, &(pairRTTTable[0][2*j]), &(pairRTTTable[0][2*j+1]));
		}

		//Sends token to next pair to start their execution
		for(k = 2; k<numproc; k = k + 2)
		{
			int token = 1;
			MPI_Send(&token, 1, MPI_INT, k, 0, MPI_COMM_WORLD);
			for(l = 0; l < 7; l++)
			{
				double *values = (double *)malloc(sizeof(double) * 2);
				//Receives latency values and stores in the array
				MPI_Recv(values, 2, MPI_DOUBLE, k, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				pairRTTTable[k/2][l * 2] = values[0];
				pairRTTTable[k/2][l * 2 + 1] = values[1];
			}
			MPI_Recv(&token, 1, MPI_INT, k, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
		FILE *fp = fopen("output.txt", "w");
		//Printing output
		for(m = 0; m < 7; m++)
		{
			fprintf(fp, "%d ", 16<<m);
			for(n = 0; n < numproc/2; n++)
			{
				fprintf(fp, "%e %e ", pairRTTTable[n][2 * m], pairRTTTable[n][2 * m + 1]);
			}
			fprintf(fp, "\n");
		}
	}
	else if(rank%2 == 1)					//Odd rank pairs wait to receive and return data packets
	{
		void *buf = malloc(sizeof(char) * 2048 * 1024);
		for(i = 0; i < 7; i++)
		{
			for(j = 0; j < 10; j++)
			{
				MPI_Recv(buf, 16<<i, MPI_BYTE, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				MPI_Send(buf, 16<<i, MPI_BYTE, rank - 1, 0, MPI_COMM_WORLD);
			}
		}
	}
	else
	{
		int token = 0;

		//Waits for token
		MPI_Recv(&token, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		int initialSize = 16;
		for(j = 0; j < 7; j++)
		{
			double *values = (double *)malloc(sizeof(double) * 2);
			calcRTT(initialSize<<j, rank, rank + 1, &(values[0]), &(values[1]));
			MPI_Send(values, 2, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
		}

		//Returns token after done execution
		MPI_Send(&token, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
	}
	MPI_Finalize();
}
