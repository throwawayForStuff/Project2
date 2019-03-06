#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h> //TODO
#include <signal.h> //TODO

#define TIMEOUT 3
#define MAXTRY 10

struct segmentPacket
{
  int type;
  int sequenceNum;
  int length;
  char data[512];
};

int tries = 0;
int base = 0;
int windowSize = 0;
int sendFlag = 1;

void catchAlarm (int i)
{
  tries++;
  sendFlag = 1;
}

int min(int x, int y)
{
  if (x < y)
    return x;
  return y;
}

int max(int x, int y)
{
  if (x > y)
    return x;
  return y;
}

int main(int argc, char *argv[])
{
  int sockfd;
  int port;
  int clientSize;
  int respLength; //TODO
  int packetReceived = -1; //Highest ack received
  int packetSent = -1;	    //Highest packet sent
  int packets = 0;          //Number of packets to be sent
  int chunkSize = 0;        //Chunk size in bytes TODO change if getting from file?
  const int dataSize = 20; //TODO

  struct sockaddr_in serverAddr;
  struct sockaddr_in clientAddr;
  struct sigaction myAction; //TODO

  char * serverIP;
  char buffer[20] = "asdfghjklqwertyiua"; //TODO

  if (argc != 5)
  {
    printf("\nError in receiving arguments.\n");
    exit(1);
  }
  
  serverIP = argv[1];
  port = atoi(argv[2]);
  chunkSize = atoi(argv[3]);
  windowSize = atoi(argv[4]);

  if (chunkSize >= 512)
  {
    printf("\nChunk size must be less than 512\n");
    exit(1);
  }

  packets = dataSize / chunkSize;
  if (dataSize % chunkSize)
    packets++;

  if ((sockfd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
  {
    printf("\nCannot create socket.\n");
    exit(1);
  }
  printf("\nSocket created.\n");

  myAction.sa_handler = catchAlarm; //TODO
  if (sigfillset (&myAction.sa_mask) < 0)
  {
    printf("\nsigfillset failed.\n"); //TODO
    exit(1);
  }
  myAction.sa_flags = 0;

  if (sigaction(SIGALRM, &myAction, 0) < 0)
  {
    printf("\nsigaction failed.\n"); //TODO
    exit(1);
  }

  memset(&serverAddr, '\0', sizeof(serverAddr));
  serverAddr.sin_port = htons(port);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = inet_addr(serverIP);

  while ((packetReceived < packets - 1) && (tries < MAXTRY))
  {
    if (sendFlag > 0)
    {
      sendFlag = 0;
      
      for (int counter = 0; counter < windowSize; counter++)
      {
        packetSent = min(max(base + counter, packetSent), packets - 1);
	struct segmentPacket currentPacket;

	if ((base + counter) < packets)
	{
	  memset(&currentPacket, '\0', sizeof(currentPacket));
	  printf("\nSending packet %d. Packet %d sent. %d packets received", base + counter, packetSent, packetReceived);

	  currentPacket.type = htonl(1);
	  currentPacket.sequenceNum = htonl (base + counter);

	  int currentLength;
	  if ((dataSize - ((base + counter) * chunkSize)) > chunkSize)
	    currentLength = chunkSize;
	  else
	    currentLength = dataSize % chunkSize;

	  currentPacket.length = htonl(currentLength);

	  memcpy(currentPacket.data, buffer + ((base + counter) * chunkSize), currentLength);
	  if (sendto(sockfd, &currentPacket, (sizeof(int)*3) + currentLength, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != ((sizeof(int)*3) + currentLength))
	  {
	    printf("\nSent different number of bytes than expected.\n");
	    exit(1);
	  }
	}
      }
    }

    clientSize = sizeof(clientAddr);
    alarm(TIMEOUT);
    struct segmentPacket currentACK;

    while ((respLength = (recvfrom (sockfd, &currentACK, sizeof(int)*3, 0, (struct sockaddr *)&clientAddr, &clientSize))) < 0)
    {
      if (errno == EINTR)
      {
        if (tries < MAXTRY)
	{
	  printf("\nTimed out. %d more tries.\n", MAXTRY - tries);
	  break;
	}
	else
	{
	  printf("\nNo response from server.\n"); //TODO
	  exit(1);
	}
      }

      else
      {
        printf("\nCannot send data.\n"); //TODO client vs server
	exit(1);
      }
    }

    if (respLength)
    {
      int ackType = ntohl(currentACK.type);
      int ackNum = ntohl(currentACK.sequenceNum);

      if (ackNum > packetReceived && ackType == 2)
      {
        printf("\nACK received.\n");
	packetReceived++;
	base = packetReceived;
	if (packetReceived == packetSent)
	{
	  alarm(0);
	  tries = 0;
	  sendFlag = 1;
	}
	else
	{
	  tries = 0;
	  sendFlag = 0;
	  alarm(TIMEOUT);
	}
      }
    }
  }

  for (int counter = 0; counter < 10; counter++)
  {
    struct segmentPacket shutDown;
    shutDown.type = htonl(4);
    shutDown.sequenceNum = htonl(0);
    shutDown.length = htonl(0);
    sendto(sockfd, &shutDown, (sizeof(int)*3), 0, (struct sockaddr *)&serverAddr, sizeof (serverAddr));
  }

  close(sockfd);
  exit(0);
}
