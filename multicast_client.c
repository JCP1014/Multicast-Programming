/* Receiver/client multicast Datagram example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <liquid/liquid.h>
#include <sys/stat.h>
#include <unistd.h>
#define port 6666

struct sockaddr_in localSock;
struct ip_mreq group;
int sd;
int datalen;
char databuf[1035];

int main(int argc, char *argv[])
{
	char mode = 'f';
	char IP[16] = "10.0.2.15";
	if (argc >= 2)
		(strcmp(argv[1],"n")==0)  ? (mode = 'n') : (mode = 'f');
	if (argc >= 3)
		strncpy(IP, argv[2], strlen(argv[2]) + 1);

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
	char filePath[1024] = "./Received/";	// Save received file in "Received" directory
	datalen = sizeof(databuf);
	/* Read the file name from socket*/
	if (read(sd, databuf, datalen) < 0)
	{
		perror("Reading fileName message error");
		close(sd);
		exit(1);
	}
	else
	{
		//printf("Reading fileName message...OK.\n");
		strncat(filePath, databuf, strlen(databuf) + 1);
		printf("The multimedia is saved in %s\n", filePath);
		memset(databuf, 0, sizeof(databuf));
	}

	// Read how large is the encoded message
	unsigned int k = 0;
	if (read(sd, databuf, datalen) < 0)
	{
		perror("Reading length message error");
		close(sd);
		exit(1);
	}
	else
	{
		//printf("Reading length message...OK.\n");
		k = atoi(databuf);	// Convert the string received to integer
		memset(databuf, 0, sizeof(databuf));	// Clear buffer
	}

	unsigned char recvbuf[k];	// Create the receive buffer
	fp = fopen(filePath, "wb");
	if (fp == NULL)
	{
		printf("Error openning file.\n");
		return -1;
	}

	/* Receive file */
	long int number = 0;
	long int recv_num = 0;	// serial number of the packet received
	long int correct_num = 0;	// packet number that expected to receive
	long int lost_packet = 0;	// number of lost packet
	long int total_packet = 0;	// total number of packet should be received when no packet lost
	long int recv_data = 0;	// size of data received
	long int total_data = 0;	// total size of data should be received when no data lost
	unsigned int n = 1040;				  // original data length (bytes)
	fec_scheme fs = LIQUID_FEC_HAMMING74; // error-correcting scheme
	unsigned char msg_dec[n];	// decoded message
	fec q = fec_create(fs, NULL); // Create fec object
	int bytes = 0;	// valid data length to write into file
	char num_str[11] = {0};	// packet number in string type
	char byte_str[5] = {0};	// valid data length in string type

	// Read message
	while (read(sd, recvbuf, sizeof(recvbuf)))
	{
		if (strcmp(recvbuf, "EOF") == 0)	// If server has completed sending file
		{
			break;
		}
		else
		{
			number++;
			//printf("number = %ld\n",number);
			correct_num++;
			if (mode == 'f')
			{
				fec_decode(q, n, recvbuf, msg_dec);	// Decode message
				memcpy(num_str, msg_dec, sizeof(num_str));	// Read the serial number
				recv_num = strtol(num_str, NULL, 10);	// Convert string into integer
				memcpy(byte_str, msg_dec + 11, sizeof(byte_str));	// Read valid data length
				bytes = atoi(byte_str);	// Convert string to integer
				fwrite(msg_dec + 16, 1, bytes, fp);	// Write data into file
				memset(msg_dec, 0, sizeof(msg_dec));	// Clear buffer
			}
			else
			{
				//printf("No FEC\n");
				memcpy(num_str, recvbuf, sizeof(num_str));
				recv_num = strtol(num_str, NULL, 10);
				memcpy(byte_str, recvbuf + 11, sizeof(byte_str));
				bytes = atoi(byte_str);
				fwrite(recvbuf + 16, 1, bytes, fp);
			}
			// Compute how many packet has lost
			if (recv_num != correct_num)	// If the received packet number is not as same as the number we expexted
			{
				lost_packet += recv_num - correct_num;	// How many lost
				correct_num = recv_num;	// Update 
			}
			memset(recvbuf, 0, sizeof(recvbuf));	// Clear buffer
		}
	}
	fclose(fp);	 // Close file
	fec_destroy(q);	// Destroy the fec object

	// Read the total number of packet should be received originally
	if (read(sd, databuf, datalen) < 0)
	{
		perror("Reading final packet number error");
		close(sd);
		exit(1);
	}
	else
	{
		//printf("Reading final packet number...OK.\n");
		total_packet = strtol(databuf, NULL, 10);
		printf("Lost packet = %ld\n", lost_packet);	// number of lost packet 
		printf("Packet lost rate= %f\n", (double)lost_packet / total_packet); // Compute packet lost rate
		memset(databuf, 0, sizeof(databuf));
	}
	// Read the total size of original file 
	if (read(sd, databuf, datalen) < 0)
	{
		perror("Reading file size error");
		close(sd);
		exit(1);
	}
	else
	{
		//printf("Reading file size number...OK.\n");
		total_data = strtol(databuf, NULL, 10);
		// Get the size of file that client write
		struct stat st;
		stat(filePath, &st);
		recv_data = st.st_size;
		printf("Received data size = %ld\n",recv_data);	// size of file that client write
		printf("Lost data size = %ld\n", total_data-recv_data);	// Compute how many bytes of data are lost 
		printf("Data lost rate = %f\n", (double)(total_data-recv_data) / total_data);	// Compute data lost rate
		memset(databuf, 0, sizeof(databuf));
	}
	close(sd);	// Close socket
	return 0;
}