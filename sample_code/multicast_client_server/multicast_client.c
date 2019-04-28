/* Receiver/client multicast Datagram example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <liquid/liquid.h>
#define IP "10.0.2.15"
#define port 4321
struct sockaddr_in localSock;
struct ip_mreq group;
int sd;
int datalen;
char databuf[1035];

int main(int argc, char *argv[])
{
	/* Create a datagram socket on which to receive. */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
	{
		perror("Opening datagram socket error");
		exit(1);
	}
	else
		printf("Opening datagram socket....OK.\n");

	/* Enable SO_REUSEADDR to allow multiple instances of this */
	/* application to receive copies of the multicast datagrams. */
	{
		int reuse = 1;
		if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
		{
			perror("Setting SO_REUSEADDR error");
			close(sd);
			exit(1);
		}
		else
			printf("Setting SO_REUSEADDR...OK.\n");
	}

	/* Bind to the proper port number with the IP address */
	/* specified as INADDR_ANY. */
	memset((char *)&localSock, 0, sizeof(localSock));
	localSock.sin_family = AF_INET;
	localSock.sin_port = htons(port);
	localSock.sin_addr.s_addr = INADDR_ANY;
	if (bind(sd, (struct sockaddr *)&localSock, sizeof(localSock)))
	{
		perror("Binding datagram socket error");
		close(sd);
		exit(1);
	}
	else
		printf("Binding datagram socket...OK.\n");

	/* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
	/* interface. Note that this IP_ADD_MEMBERSHIP option must be */
	/* called for each local interface over which the multicast */
	/* datagrams are to be received. */
	group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
	group.imr_interface.s_addr = inet_addr(IP);
	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
	{
		perror("Adding multicast group error");
		close(sd);
		exit(1);
	}
	else
		printf("Adding multicast group...OK.\n");

	FILE *fp = NULL;
	char filePath[1024] = "./Received/";
	/* Read from the socket. */
	datalen = sizeof(databuf);
	/* Read the file name */
	if (read(sd, databuf, datalen) < 0)
	{
		perror("Reading fileName message error");
		close(sd);
		exit(1);
	}
	else
	{
		printf("Reading fileName message...OK.\n");
		//printf("The message from multicast server is: \"%s\"\n", databuf);
		strncat(filePath, databuf, strlen(databuf) + 1);
		printf("The multimedia is saved in %s\n", filePath);
		memset(databuf, 0, sizeof(databuf));
	}

	unsigned int k = 0;
	if (read(sd, databuf, datalen) < 0)
	{
		perror("Reading fileName message error");
		close(sd);
		exit(1);
	}
	else
	{
		printf("Reading length message...OK.\n");
		//printf("The message from multicast server is: \"%s\"\n", databuf);
		k = atoi(databuf);
		//printf("The k = %u\n", k);
		memset(databuf, 0, sizeof(databuf));
	}

	unsigned char recvbuf[k];
	fp = fopen(filePath, "wb");
	if (fp == NULL)
	{
		printf("Error openning file.\n");
		return -1;
	}
	/* Receive file */
	long int number = 0;
	long int recv_num = 0;
	long int correct_num = 0;
	long int lost = 0;
	unsigned int n = 1040;				  // original data length (bytes)
	fec_scheme fs = LIQUID_FEC_HAMMING74; // error-correcting scheme

	// Create arrays
	unsigned char msg_dec[n];

	// Create fec object
	fec q = fec_create(fs, NULL);
	int bytes = 0;
	char num_str[11] = {0};
	char byte_str[5] = {0};
	unsigned char writebuf[1025] = {0};
	while (read(sd, recvbuf, sizeof(recvbuf)))
	{
		if (strcmp(recvbuf, "EOF") == 0)
		{
			printf("EOF\n");
			break;
		}
		else
		{
			number++;
			//printf("number = %ld\n",number);
			correct_num++;
			// Decode message
			fec_decode(q, n, recvbuf, msg_dec);
			memcpy(num_str,msg_dec,sizeof(num_str));
			recv_num = strtol(num_str,NULL,10);
			if(recv_num != correct_num)
			{
				lost += recv_num-correct_num;
				correct_num = recv_num;
			}
			//printf("%s\n",num_str);
			memcpy(byte_str,msg_dec+11,sizeof(byte_str));
			//printf("%s\n",byte_str);
			bytes = atoi(byte_str);
			fwrite(msg_dec+16, 1,bytes,fp);
			//printf("bytes = %d\n",bytes);
			memset(recvbuf, 0, sizeof(recvbuf));
			memset(msg_dec, 0, sizeof(msg_dec));
		}
	}
	printf("lost = %ld\n",lost);
	// Destroy the fec object
	fec_destroy(q);

	return 0;
}