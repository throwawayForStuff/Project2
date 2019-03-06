#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>

#define MAX 255

struct segmentPacket
{
  int type;
  int sequenceNum;
  int length;
  char data[512];
};

void catchAlarm(int x)
{
  exit(0);
}

int main(int argc, char *argv[])
{
  int sockfd;
  int port;
  int clientLength; //TODO
  int messageSize; //TODO
  int chunkSize;
  int packetReceived = -1;
  double lossRate = 0.0;

  struct sockaddr_in serverAddr;
  struct sockaddr_in clientAddr;
  struct sigaction myAction;

  char buffer[21]; //Handling only input from other. TODO
  buffer[20] = '\0';

  bzero(buffer, 20);

  if (argc < 3 || argc > 4)
  {
    printf("\nError in getting arguments.\n");
    exit(1);
  }

  port = atoi(argv[1]);
  chunkSize = atoi(argv[2]);
  if (argc == 4)
    lossRate = atof(argv[3]);

  srand48(123456789); //TODO

  if ((sockfd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
  {
    printf("\nCannot create socket.\n");
    exit(1);
  }
  printf("\nSocket created.\n");

  memset(&serverAddr, '\0', sizeof(&serverAddr));
  serverAddr.sin_port = htons(port);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind (sockfd, (struct sockaddr *)&serverAddr, sizeof (serverAddr)) < 0)
  {
    printf("\nCannot bind.\n");
    exit(1);
  }
  printf("\nBind successfully.\n");

  myAction.sa_handler = catchAlarm;
  if (sigfillset (&myAction.sa_mask) < 0)
  {
    printf("\nsigfillset failed\n");
    exit(1);
  }

  while (1)
  {
    clientLength = sizeof (clientAddr);
    
    struct segmentPacket currentPacket;
    memset(&currentPacket, '\0', sizeof(currentPacket));

    messageSize = recvfrom (sockfd, &currentPacket, sizeof(currentPacket), 0, (struct sockaddr *)&clientAddr, &clientLength);
    currentPacket.type = ntohl (currentPacket.type);
    currentPacket.sequenceNum = ntohl (currentPacket.sequenceNum);
    currentPacket.length = ntohl (currentPacket.length);

    if (currentPacket.type == 4)
    {
      printf("%s\n", buffer);

      struct segmentPacket ackPacket;
      ackPacket.type = htonl(8);
      ackPacket.sequenceNum = htonl(0);
      ackPacket.length = htonl(0);
      if (sendto (sockfd, &ackPacket, sizeof (ackPacket), 0, (struct sockaddr *)&clientAddr, clientLength) != sizeof (ackPacket))
      {
        printf("\nCannot send shutdown packet.\n");
	exit(0); //Data still received
      }
      alarm(7);
      
      while (1)
      {
        while ((recvfrom (sockfd, &currentPacket, sizeof (int)*3 + chunkSize, 0, (struct sockaddr *)&clientAddr, &clientLength)) < 0)
	{
	  if (errno == EINTR)
	    exit(0);
	}

	if (ntohl (currentPacket.type) == 4)
	{
	  ackPacket.type = htonl(8);
	  ackPacket.sequenceNum = htonl(0);
	  ackPacket.length = htonl(0);
	  if (sendto (sockfd, &ackPacket, sizeof (ackPacket), 0, (struct sockaddr *)&clientAddr, clientLength) != sizeof (ackPacket))
	  {
	    printf("\nCannot send shut down ack\n");
	    exit(0);
	  }
	}
      }
      printf("\nCannot receive packet.\n");
      exit(1);
    }

    else
    {
      //if (lossRate > drand(48))
        //continue;
      printf("\nReceived packet %d of length %d\n", currentPacket.sequenceNum, currentPacket.length);

      if (currentPacket.sequenceNum == packetReceived + 1)
      {
        packetReceived++;
	int bufferOffset = chunkSize * currentPacket.sequenceNum;
	memcpy(&buffer[bufferOffset], currentPacket.data, currentPacket.length);
      }

      printf("\nSending ACK %d\n", packetReceived);
      struct segmentPacket currentACK;
      currentACK.type = htonl(2);
      currentACK.sequenceNum = htonl(packetReceived);
      currentACK.length = htonl(0);

      if (sendto (sockfd, &currentACK, sizeof (currentACK), 0, (struct sockaddr *)&clientAddr, clientLength) != sizeof (currentACK))
      {
        printf("\nSent a different number of bytes than expected.\n");
	exit(1);
      }
    }
  }
  exit(0);
}
