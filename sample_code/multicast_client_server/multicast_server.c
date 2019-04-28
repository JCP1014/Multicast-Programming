/* Send Multicast Datagram code example. */
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

struct in_addr localInterface;
struct sockaddr_in groupSock;
int sd;
//char databuf[1024] = "Multicast test message.";
char databuf[1035] = {0};
int datalen = sizeof(databuf);
char fileName[256] = "video.mp4";

char *server_fec(char *data, unsigned int length);

int main(int argc, char *argv[])
{
	if (argv[1] != NULL)
		strncpy(fileName, argv[1], strlen(argv[1]) + 1);
	printf("%s\n", fileName);
	/* Create a datagram socket on which to send. */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
	{
		perror("Opening datagram socket error");
		exit(1);
	}
	else
		printf("Opening the datagram socket...OK.\n");

	/* Initialize the group sockaddr structure with a */
	/* group address of 226.1.1.1 and port 4321. */
	memset((char *)&groupSock, 0, sizeof(groupSock));
	groupSock.sin_family = AF_INET;
	groupSock.sin_addr.s_addr = inet_addr("226.1.1.1");
	groupSock.sin_port = htons(port);

	/* Disable loopback so you do not receive your own datagrams.
	{
	char loopch = 0;
	if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
	{
	perror("Setting IP_MULTICAST_LOOP error");
	close(sd);
	exit(1);
	}
	else
	printf("Disabling the loopback...OK.\n");
	}
	*/

	/* Set local interface for outbound multicast datagrams. */
	/* The IP address specified must be associated with a local, */
	/* multicast capable interface. */
	localInterface.s_addr = inet_addr(IP);
	if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0)
	{
		perror("Setting local interface error");
		exit(1);
	}
	else
		printf("Setting the local interface...OK\n");
	/* Send a message to the multicast group specified by the*/
	/* groupSock sockaddr structure. */
	/*int datalen = 1024;*/
	/* Send file name */
	if (sendto(sd, fileName, sizeof(fileName), 0, (struct sockaddr *)&groupSock, sizeof(groupSock)) < 0)
	{
		perror("Sending fileName message error");
	}
	else
		printf("Sending fileName message...OK\n");

	long number = 0;
	char num_str[11] = {0};
	FILE *fp = NULL;
	char readbuf[1024] = {0};
	char *saveptr = NULL;

	unsigned int n = sizeof(readbuf);				// original data length (bytes)
	fec_scheme fs = LIQUID_FEC_HAMMING74;			// error-correcting scheme
	unsigned int k = fec_get_enc_msg_length(fs, n); // Compute sizeof encoded message
	unsigned char msg_enc[k];
	fec q = fec_create(fs, NULL);

	char len_str[10];
	snprintf(len_str,sizeof(len_str),"%u",k);
	printf("k = %u\n",k);
	if (sendto(sd, len_str, sizeof(len_str), 0, (struct sockaddr *)&groupSock, sizeof(groupSock)) < 0)
	{
		perror("Sending length message error");
	}
	else
		printf("Sending length message...OK\n");

	/*sprintf(num_str, "%ld", number);
	strncat(num_str, "/", sizeof(char));
	memset(databuf, 0, datalen);
	strncat(databuf, num_str, strlen(num_str));*/
	/*printf("data = %s\n",databuf);
	strtok_r(databuf,"/",&saveptr);
	printf("after = %s\n",databuf);
	printf("save = %s\n",saveptr);*/

	/* Send file */
	fp = fopen(fileName, "rb");
	if (fp == NULL)
	{
		printf("Error openning file.\n");
		return -1;
	}
	while (!feof(fp))
	{
		/* Set the serial number of this packet */
		/*number++;
		sprintf(num_str, "%ld", number);
		printf("number = %s\n", num_str);
		strncat(num_str, "/", sizeof(char));
		strncat(databuf, num_str, strlen(num_str));*/

		fread(readbuf, sizeof(readbuf), 1, fp);

		// Encode meesage
		//fec_encode(q, n, readbuf, msg_enc);

		//strncat(databuf, readbuf, sizeof(readbuf));
		sendto(sd, readbuf, sizeof(readbuf), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
		number++;
		printf("number = %ld\n",number);
		memset(readbuf, 0, sizeof(readbuf));
		memset(msg_enc, 0, sizeof(msg_enc));
	}
	fclose(fp);
	// Destroy the fec object
	fec_destroy(q);
	sendto(sd, "EOF", 3, 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
	memset(databuf, 0, datalen);
	/* Try the re-read from the socket if the loopback is not disable
	if(read(sd, databuf, datalen) < 0)
	{
	perror("Reading datagram message error\n");
	close(sd);
	exit(1);
	}
	else
	{
	printf("Reading datagram message from client...OK\n");
	printf("The message is: %s\n", databuf);
	}*/
	
	close(sd);
	return 0;
}

char *server_fec(char *data, unsigned int length)
{
	unsigned int n = length;						// original data length (bytes)
	fec_scheme fs = LIQUID_FEC_HAMMING74;			// error-correcting scheme
	unsigned int k = fec_get_enc_msg_length(fs, n); // Compute sizepf encoded message

	// Create arrays
	unsigned char msg_enc[k];

	// Create fec object
	fec q = fec_create(fs, NULL);
	//fec_print(q);

	// Encode meesage
	fec_encode(q, n, data, msg_enc);

	// Destroy the fec object
	fec_destroy(q);

	return msg_enc;
}