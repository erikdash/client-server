#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define BLOCK_SIZE 1024
pthread_mutex_t lock; //declare mutex lock

int totalPorts;
int numThreads;
FILE * outputFile; //declare global variables

typedef struct portArgs //define the port arguments struct (as a pthread can only take one argument)
{
	char *portString;
	char *ipAddress;
}portArgs;


void * downloadChunk(void *arguments)
{
	portArgs *args = (portArgs*)(arguments); //cast the void argument to a portArgs type

	int port = atoi(args->portString); //turn the port number string into an int

	char receivedBuffer[1024];
	char fullChunk[1024];
	char chunkString[128]; //declare a few char buffers

	pthread_mutex_lock(&lock);
	int threadNum = numThreads++; //increment threadNum (within a mutex lock)
	pthread_mutex_unlock(&lock);
	
	int chunkNum = threadNum;

	
	while(1)
	{
		int con_fd = 0;
		int ret = 0;
		struct sockaddr_in serv_addr;
		con_fd = socket(PF_INET, SOCK_STREAM, 0);

		if (con_fd == -1) 
		{
			perror("Socket Error\n");
			return  -1;
		}

	 
		memset(&serv_addr, 0, sizeof(struct sockaddr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
		serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");


		ret = connect(con_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));

		if (ret < 0) 
		{
			perror("Connect error\n");
			return -1;
		}
		else
		{
			printf("Successful connection!\n");
		} //the lines from the start of the while loop to here establish a connecction to the server
		

		snprintf(chunkString, 128, "%d", chunkNum);
		send(con_fd, chunkString, strlen(chunkString), 0); //format and send the chunk number to the server
		printf("chunkString: %s\n", chunkString);

		int length = recv(con_fd, receivedBuffer, 1024, 0); //recieve the chunk
		printf("Length: %d\n", length);
		if(length <= 0)
		{
			close(con_fd);
			break;
		}
		
		do
		{
			strncat(fullChunk, receivedBuffer, 1024); //add the recievedBuffer to the full chunk
			length = recv(con_fd, receivedBuffer, 1024, 0); //attempt to get more in case the initial download didnt get the whole chunk

			if(length <= 0)
			{
				close(con_fd);
				break;
			}

		}while((length > 0) && (strlen(fullChunk) <= 1024)); //continue to download until the whole chunk has been received
		close(con_fd);
		
		pthread_mutex_lock(&lock);
		fseek(outputFile, chunkNum*1024, SEEK_SET); //find the corresponding place in the output file
		fputs(fullChunk, outputFile); //place the downloaded chunk there
		pthread_mutex_unlock(&lock);
	
		chunkNum += totalPorts;
	
		memset(fullChunk, 0, 1024);
		memset(receivedBuffer, 0, 1024);
		memset(chunkString, 0, 128); //reset the char buffers so that this thread can download new chunks
	}

	
	
}


int main(int argc, char **argv)
{
	outputFile = fopen("output.txt", "w");
	totalPorts = (argc-1)/2;
	pthread_t threadList[totalPorts];
	void *readCnts[totalPorts];
	
	int i = 0;
	while(1)
	{
		struct portArgs *newArgs  = malloc(sizeof(portArgs));
		newArgs->portString = argv[2+i*2];
		newArgs->ipAddress = argv[1+i*2];
		pthread_create(&threadList[i], NULL, downloadChunk, (void*)newArgs);		
		i++;
		if(i >= totalPorts)
		{
			break;
		}
	} //create new threads until the thread count matches the total number of ports

	i = 0;

	while(1)
	{
		pthread_join(threadList[i], &readCnts[i]);
		i++;
		if(i >= totalPorts)
		{
			break;
		}
	} //join all of the threads
	fclose(outputFile);
	return 0;
}
