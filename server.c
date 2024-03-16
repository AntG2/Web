#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "utils.h"

void send_ack(int acknum, int send_sockfd);

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_to;
    int expected_seq_num = 0;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);
    
    if (connect(send_sockfd, (struct sockaddr *) &client_addr_to, sizeof(client_addr_to)) < 0)
      {
	fprintf(stderr, "error connecting to client");
      }
    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb+");
    // TODO: Receive file from the client and save it as output.txt
    
    // use socket receive input into a buffer (use size from sender)
    char input[PAYLOAD_SIZE + 1];
    struct packet** pkt_list = malloc(NUM_SEQ * sizeof(struct packet*));
    bool received[NUM_SEQ];
    for (int i = 0; i < NUM_SEQ; i++)
      {
	received[i] = false;
      }
    while(1)
      {
	int bytes_read = recv(listen_sockfd, input, PAYLOAD_SIZE, 0);
	if (bytes_read < 0)
	  {
	    perror("hey");
	    fprintf(stderr, "error reading from socket");
	    exit(errno);
	  }
	//get seq num, length, payload
	struct packet* data = malloc(sizeof(struct packet));
	get_data(data, input, bytes_read);
	
	//	printRecv(&data);
        int checksum = compute_checksum(input, bytes_read);
        //printf("%d\n", confirm_checksum(input, bytes_read));
	if (checksum == 0)
	  {
	    // last packet signal ending transaction
	    if (data->last == 1)
	      {
		send_last(send_sockfd);
		break;
	      }
	    
	    if (received[data->seqnum] == false && (bufferable(data->seqnum, expected_seq_num) || data->seqnum == expected_seq_num))
	      {
		pkt_list[data->seqnum] = data;
		received[data->seqnum] = true;
	      }
	    if (data->seqnum == expected_seq_num)
	      {
		bool run = true;
		int index = expected_seq_num;
		int count = 0;
	        do
		  {
		    struct packet* pkt = pkt_list[index];
		    fwrite(pkt->payload, 1, pkt->length - HEADER_LEN, fp);
		    fseek(fp, 0, SEEK_CUR);
		    index = increment(index, 1);
		    count++;
		    if (!received[index])
		      run = false;
		  }
		while(run);
	        while(count > 0)
		  {
		    received[expected_seq_num] = false;
		    free(pkt_list[expected_seq_num]->payload);
		    count--;
		    expected_seq_num = increment(expected_seq_num, 1);
		  }
		send_ack(expected_seq_num, send_sockfd);
		
	      }
	    else
	      {
		send_ack(expected_seq_num, send_sockfd);
		fprintf(stderr, "unexpected seq number, %d, expecting %d\n", data->seqnum, expected_seq_num);
	      }
	      
	  }
	else
	  {
	    fprintf(stderr, "wrong checksum: corrupted packet\n");
	  }
	
      }
    free(pkt_list);
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}


void send_ack(int acknum, int send_sockfd)
{
  char* header = makeHeader(SERVER_PORT, SERVER_PORT_TO, HEADER_LEN, 0, acknum, 1, 0);
  append(header, 6, compute_checksum(header, HEADER_LEN));
  int n = send(send_sockfd, header, HEADER_LEN, 0);
  if (n<0)
    {
      perror("error sending ack\n");
      exit(errno);
    }
  free(header);
}
