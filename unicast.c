#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <liquid/liquid.h>
#include <sys/stat.h>

#define ERR_EXIT(m)         \
    do                      \
    {                       \
        perror(m);          \
        exit(EXIT_FAILURE); \
    } while (0)

void udp_server(char *ip, int port, char *fileName);
void udp_client(char *ip, int port);
void error(const char *msg);
char mode = 'f';
char databuf[1024];
long int datalen = sizeof(databuf);

int main(int argc, char *argv[])
{
    char *ip;
    int port;
    char *fileName = NULL;

    if (argc >= 5) // Ensure there are enough input arguments
    {

        (strcmp(argv[1], "n") == 0) ? (mode = 'n') : (mode = 'f');
        ip = argv[3];
        port = atoi(argv[4]);
        if (strcmp("send", argv[2]) == 0 && argc >= 6) // UDP & send
        {
            fileName = argv[5];
            udp_server(ip, port, fileName);
        }
        else if (strcmp("recv", argv[2]) == 0) // UDP & recv
        {
            udp_client(ip, port);
        }
        else
        {
            printf("input error\n");
        }
    }
    else
    {
        printf("input error\n");
    }
}

void udp_server(char *ip, int port, char fileName[256])
{
    int sock;
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        ERR_EXIT("socket error");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    /* Avoid the bind error which says the address already in use */
    int on = 1;
    if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)
        error("setsockopt failed");

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        ERR_EXIT("bind error");

    unsigned char recv_buf[1024] = {0};
    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);
    int n;

    // Wait for client sending "connected"
    n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                 (struct sockaddr *)&peeraddr, &peerlen);
    if (n == -1) // Fail
    {
        ERR_EXIT("recvfrom error");
    }
    else if (n > 0) // Success
    {
        strcat(fileName, "\0"); // Concatenate the null character to avoid sending wrong file name
        // Tell the client the file name
        if (sendto(sock, fileName, strlen(fileName), 0,
                   (struct sockaddr *)&peeraddr, peerlen) < 0)
            error("ERROR sending to client");

        /* Read the file and send to the client */
        long number = 0;    // packet number
        char num_str[11] = {0}; // packet number in string type 
        char byte_str[5] = {0}; // valid sata length in string type
        FILE *fp = NULL;    // file pointer
        unsigned char sendbuf[1040] = {0};  // for send to socket
        unsigned char readbuf[1024] = {0};  // for read file

        unsigned int n = sizeof(sendbuf);               // original data length (bytes)
        fec_scheme fs = LIQUID_FEC_HAMMING74;           // error-correcting scheme
        unsigned int k = fec_get_enc_msg_length(fs, n); // Compute sizeof encoded message
        unsigned char msg_enc[k];   // encoded message
        fec q = fec_create(fs, NULL);   // Create fec object
        char len_str[10];   // length of encoded message in string type
        snprintf(len_str, sizeof(len_str), "%u", k);    // Convert length of encoded message to string
        // Tell the length of encoded message to client, so it know how long should it receive
        if (sendto(sock, len_str, sizeof(len_str), 0,
                   (struct sockaddr *)&peeraddr, peerlen) < 0)
        {
            perror("Sending length message error");
        }
        //else
        //   printf("Sending length message...OK\n");

        fp = fopen(fileName, "rb"); // Open file
        if (fp == NULL)
        {
            printf("Error openning file.\n");
            return;
        }

        // Read file and send
        while (!feof(fp))
        {
            /* Set the serial number of this packet */
            number++;
            snprintf(num_str, sizeof(num_str), "%010ld", number);
            //printf("number = %ld\n", strtol(num_str,NULL,10));
            printf("number = %s\n", num_str);
            memcpy(sendbuf, num_str, sizeof(num_str));  // Put the serial number into send buffer
            int bytes = fread(readbuf, 1, sizeof(readbuf), fp);
            snprintf(byte_str, sizeof(byte_str), "%04d", bytes);
            memcpy(sendbuf + 11, byte_str, sizeof(num_str));    // Put the valid data length into send buffer
            memcpy(sendbuf + 16, readbuf, sizeof(readbuf));

            if (mode == 'f')    // Using FEC
            {
                // Encode meesage and send
                fec_encode(q, n, sendbuf, msg_enc);
                sendto(sock, msg_enc, sizeof(msg_enc), 0,
                       (struct sockaddr *)&peeraddr, peerlen);
                memset(msg_enc, 0, sizeof(msg_enc));
            }
            else    // No FEC
            {
                //printf("No FEC\n");
                sendto(sock, sendbuf, sizeof(sendbuf), 0,
                       (struct sockaddr *)&peeraddr, peerlen);
            }

            // Delay for reducing packet lost rate
            int i = 0;
            for (i = 0; i < 100000; i++)
            {
                ;
            }
            memset(readbuf, 0, sizeof(readbuf));
            memset(sendbuf, 0, sizeof(sendbuf));
        }
        fclose(fp); // Close file
        // Complete transfering the file
        printf("Completed\n");
        fec_destroy(q); // Destroy the fec object
        // Tell client the file is sended completely
        sendto(sock, "EOF", 3 * sizeof(char), 0, (struct sockaddr *)&peeraddr, peerlen);
        // Send the serial number of the final packet for client to compute lost packet rate
        if (sendto(sock, num_str, sizeof(num_str), 0, (struct sockaddr *)&peeraddr, peerlen) < 0)
        {
            perror("Sending final packet number error");
        }
        //else
        //    printf("Sending final packet number...OK\n");

        // Get the original file size and send to client for computing lost data rate
        struct stat st;
        stat(fileName, &st);
        long int file_size = st.st_size;
        char size_str[10] = {0};
        snprintf(size_str, sizeof(size_str), "%ld", file_size);
        printf("Total data size = %s\n", size_str);
        if (sendto(sock, size_str, sizeof(size_str), 0, (struct sockaddr *)&peeraddr, peerlen) < 0)
        {
            perror("Sending file size error");
        }
        //else
        //    printf("Sending file size number...OK\n");
    }
    close(sock); // Close the socket
}

void udp_client(char *ip, int port)
{
    int sock;
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        ERR_EXIT("socket");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    int ret;

    int addr_len = sizeof(servaddr);
    // Tell the server that client is ready to receive packets
    ret = sendto(sock, "Connected", 9, 0, (struct sockaddr *)&servaddr, addr_len);
    if (ret < 0)
        error("sendto error");

    FILE *fp = NULL;
    char filePath[1024] = "./Received/";    // Save received file in "Received" directory
    char fileName[256];
    bzero(fileName, 256);
    memset(databuf, 0, sizeof(databuf)); // Clear the buffer
    // Get the file name from the server
    if (recvfrom(sock, databuf, datalen, 0, (struct sockaddr *)&servaddr, &addr_len) < 0)
    {
        perror("Reading fileName message error");
        close(sock);
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
    if (recvfrom(sock, databuf, datalen, 0, (struct sockaddr *)&servaddr, &addr_len) < 0)
    {
        perror("Reading length message error");
        close(sock);
        exit(1);
    }
    else
    {
        //printf("Reading length message...OK.\n");
        k = atoi(databuf);  // Convert the string received to integer
        memset(databuf, 0, sizeof(databuf));
    }

    unsigned char recvbuf[k];   // Create the receive buffer
    fp = fopen(filePath, "wb");
    if (fp == NULL)
    {
        printf("Error openning file.\n");
        return;
    }

    /* Receive file */
    long int number = 0;
    long int recv_num = 0;  // serial number of the packet received
    long int correct_num = 0;   // packet number that expected to receive
    long int lost_packet = 0;   // number of lost packet
    long int total_packet = 0;  // total number of packet should be received when no packet lost
    long int recv_data = 0; // size of data received
    long int total_data = 0;    // total size of data should be received when no data lost
    unsigned int n = 1040;                // original data length (bytes)
    fec_scheme fs = LIQUID_FEC_HAMMING74; // error-correcting scheme
    unsigned char msg_dec[n];
    fec q = fec_create(fs, NULL); // Create fec object
    int bytes = 0;  // valid data length to write into file
    char num_str[11] = {0}; // packet number in string type
    char byte_str[5] = {0}; // valid data length in string type
    // Read message
    while (recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&servaddr, &addr_len))
    {
        if (strcmp(recvbuf, "EOF") == 0)    // If server has completed sending file
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
                fec_decode(q, n, recvbuf, msg_dec); // Decode message
                memcpy(num_str, msg_dec, sizeof(num_str));  	// Read the serial number
                recv_num = strtol(num_str, NULL, 10);   // Convert string into integer
                memcpy(byte_str, msg_dec + 11, sizeof(byte_str));   // Read valid data length
                bytes = atoi(byte_str); // Convert string to integer
                fwrite(msg_dec + 16, 1, bytes, fp); // Write data into file
                memset(msg_dec, 0, sizeof(msg_dec));    // Clear buffer
            }
            else
            {
                memcpy(num_str, recvbuf, sizeof(num_str));
                recv_num = strtol(num_str, NULL, 10);
                memcpy(byte_str, recvbuf + 11, sizeof(byte_str));
                bytes = atoi(byte_str);
                fwrite(recvbuf + 16, 1, bytes, fp);
            }
            // Compute how many packet has lost
            if (recv_num != correct_num)
            {
                lost_packet += (recv_num - correct_num);    // How many lost
                correct_num = recv_num; // Update 
            }   
            memset(recvbuf, 0, sizeof(recvbuf));    // Clear buffer
        }
    }
    fclose(fp); // Close file
    fec_destroy(q); // Destroy the fec object

    // Read the total number of packet should be received originally
    if (recvfrom(sock, databuf, datalen, 0, (struct sockaddr *)&servaddr, &addr_len) < 0)
    {
        perror("Reading final packet number error");
        close(sock);
        exit(1);
    }
    else
    {
        //printf("Reading final packet number...OK.\n");
        total_packet = strtol(databuf, NULL, 10);
        printf("Lost packet = %ld\n", lost_packet); // number of lost packet 
        printf("Packet lost rate = %f\n", (double)lost_packet / total_packet);  // Compute packet lost rate
        memset(databuf, 0, sizeof(databuf));
    }
    // Read the total size of original file 
    if (recvfrom(sock, databuf, datalen, 0, (struct sockaddr *)&servaddr, &addr_len) < 0)
    {
        perror("Reading file size error");
        close(sock);
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
        printf("Received data size = %ld\n", recv_data);    // size of file that client write
        printf("Lost data size = %ld\n", total_data - recv_data);   // Compute how many bytes of data are lost 
        printf("Data lost rate = %f\n", (double)(total_data - recv_data) / total_data); // Compute data lost rate
        memset(databuf, 0, sizeof(databuf));
    }
    // Task is completed, close the socket
    close(sock);
}

void error(const char *msg)
{
    perror(msg); // Print the reason of error
    exit(1);     // Abnormal termination
}