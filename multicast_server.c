/* Send Multicast Datagram code example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <liquid/liquid.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#define port 6666

struct in_addr localInterface;
struct sockaddr_in groupSock;
int sd;

int main(int argc, char *argv[])
{
	char mode = 'f';	// 'f': using FEC / 'n': no FEC 
	char IP[16] = "10.0.2.15";
	char fileName[256] = "test_input.JPG";
	int trigger = 15;	// Send data after how many seconds
	time_t start;	// for recording the time at which server starts
 
	// Read arguments
	if (argc >= 2)
		(strcmp(argv[1],"n")==0) ? (mode = 'n') : (mode = 'f');
	if (argc >= 3)
		strncpy(IP, argv[2], strlen(argv[2]) + 1);
	if (argc >= 4)
		strncpy(fileName, argv[3], strlen(argv[3]) + 1);
	if (argc >= 5)
		trigger = atoi(argv[4]);
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

	start = time(NULL);	// Record when server starts
	while (1)
	{
		if (time(NULL) - start >= trigger)	// If it is time to trigger multicast event
		{
			// Tell client the file name
			if (sendto(sd, fileName, sizeof(fileName), 0, (struct sockaddr *)&groupSock, sizeof(groupSock)) < 0)
			{
				perror("Sending fileName message error");
			}
			//else
			//	printf("Sending fileName message...OK\n");

			long number = 0;	// packet number
			char num_str[11] = {0};	// packet number in string type 
			char byte_str[5] = {0};	// valid sata length in string type
			FILE *fp = NULL;	// file pointer
			unsigned char sendbuf[1040] = {0};	// for send to socket
			unsigned char readbuf[1024] = {0};	// for read file

			unsigned int n = sizeof(sendbuf);				// original data length (bytes)
			fec_scheme fs = LIQUID_FEC_HAMMING74;			// error-correcting scheme
			unsigned int k = fec_get_enc_msg_length(fs, n); // Compute sizeof encoded message
			unsigned char msg_enc[k];	// encoded message
			fec q = fec_create(fs, NULL);	// Create fec object
			char len_str[10];	// length of encoded message in string type
			snprintf(len_str, sizeof(len_str), "%u", k);	// Convert length of encoded message to string
			// Tell the length of encoded message to client, so it know how long should it receive
			if (sendto(sd, len_str, sizeof(len_str), 0, (struct sockaddr *)&groupSock, sizeof(groupSock)) < 0)
			{
				perror("Sending length message error");
			}
			//else
			//	printf("Sending length message...OK\n");

			fp = fopen(fileName, "rb"); // Open file
			if (fp == NULL)
			{
				printf("Error openning file.\n");
				return -1;
			}

			// Read file and send
			while (!feof(fp))
			{
				/* Set the serial number of this packet */
				number++;
				snprintf(num_str, sizeof(num_str), "%010ld", number);
				//printf("number = %ld\n", strtol(num_str,NULL,10));
				printf("number = %s\n", num_str);
				memcpy(sendbuf, num_str, sizeof(num_str));	// Put the serial number into send buffer
				int bytes = fread(readbuf, 1, sizeof(readbuf), fp);
				snprintf(byte_str, sizeof(byte_str), "%04d", bytes);
				memcpy(sendbuf + 11, byte_str, sizeof(num_str));	// Put the valid data length into send buffer
				memcpy(sendbuf + 16, readbuf, sizeof(readbuf));

				if (mode == 'f')	// Using FEC
				{
					// Encode meesage and send
					fec_encode(q, n, sendbuf, msg_enc);
					sendto(sd, msg_enc, sizeof(msg_enc), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
					memset(msg_enc, 0, sizeof(msg_enc));	// Clear buffer
				} 
				else	// No FEC
				{
					//printf("No FEC\n");
					sendto(sd,sendbuf,sizeof(sendbuf), 0, (struct sockaddr *)&groupSock, sizeof(groupSock))	;
				}
				// Clear buffer
				memset(readbuf, 0, sizeof(readbuf));
				memset(sendbuf, 0, sizeof(sendbuf));
			}
			fclose(fp);	// Close file
			fec_destroy(q);	// Destroy the fec object
			// Tell client the file is sended completely
			sendto(sd, "EOF", 3*sizeof(char), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
			// Send the serial number of the final packet for client to compute lost packet rate
			if (sendto(sd, num_str, sizeof(num_str), 0, (struct sockaddr *)&groupSock, sizeof(groupSock)) < 0)
			{
				perror("Sending final packet number error");
			}
			//else
			//	printf("Sending final packet number...OK\n");

			// Get the original file size and send to client for computing lost data rate
			struct stat st;
			stat(fileName, &st);
			long int file_size = st.st_size;
			char size_str[10] = {0};
			snprintf(size_str,sizeof(size_str),"%ld",file_size);
			printf("Total data size = %s\n",size_str);
			if (sendto(sd, size_str, sizeof(size_str), 0, (struct sockaddr *)&groupSock, sizeof(groupSock)) < 0)
			{
				perror("Sending file size error");
			}
			//else
			//	printf("Sending file size number...OK\n");
				
			break;
		}
	}

	close(sd);	// Close socket
	return 0;
}
