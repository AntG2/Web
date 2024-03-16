#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

#include "utils.h"



int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to;

    unsigned short seq_num = 0;
    
    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    if (connect(send_sockfd, (struct sockaddr *) &server_addr_to, sizeof(server_addr_to)) < 0)
      {
	fprintf(stderr, "error connecting to server");
      }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    const int READ_SEGMENT = PAYLOAD_SIZE - HEADER_LEN;
    char buffer[READ_SEGMENT + 1];
    //UDP header: src, dest, length, checksum
    // UDP fixed header size 4 fields 16 bits/2 bytes each
    char* header;

    // initial window size
    int cwnd = 1;
    
    
    while(1)
      {

	int cwnd_cp = cwnd;
	int segments_read = 0;
	int bytes_read;

	struct segment* seg_list = malloc(cwnd * sizeof(struct segment));

	int start_seq = seq_num;
	while(!feof(fp) && cwnd_cp > 0)
	  {
	    bytes_read = fread(buffer, 1, READ_SEGMENT, fp);
	    //printf("bytes_read: %d\n", bytes_read);
	    // content+header
	    int length = bytes_read + HEADER_LEN;
	    buffer[bytes_read] = '\0';
	    header = makeHeader(CLIENT_PORT, CLIENT_PORT_TO, length, seq_num, 0, 0, 0);
	   
	    char* output = malloc((PAYLOAD_SIZE) * sizeof(char));
	    
	    //header append to output
	    concatenate(output, header, 0, HEADER_LEN);
	    //content append to output
	    concatenate(output, buffer, HEADER_LEN, bytes_read);
	    
	    int checksum = compute_checksum(output, bytes_read + HEADER_LEN);
	    append(output, 6, checksum);

	    int index = cwnd - cwnd_cp;
	    seg_list[index].output = output;
	    seg_list[index].len = bytes_read + HEADER_LEN;
	    seg_list[index].seqnum = seq_num;
	    cwnd_cp--;
	    segments_read++;
	    seq_num++;
	    if (seq_num == NUM_SEQ)
	      {
		seq_num = 0;
	      }
	  }

	struct timeval begin, now;


	for (int i = 0; i < segments_read; i++)
	  {
	    int n = send(send_sockfd, seg_list[i].output, seg_list[i].len, 0);
	    if (i == 0)
	      {
		// record time first packet is sent
		gettimeofday(&begin, 0);
	      }
	    if (n < 0)
	      {
		perror("error sending packet\n");
		exit(errno);
	      }
	    usleep(50);
	  }

	
	char input[HEADER_LEN];
	bool timeout = false;
	struct packet data;
	int last_acknum = start_seq;
	bool packet_lost = false;
	int dup_count = 0;
	int cur_seq = start_seq;
	// wait for receiver ack messages
	
        while(1)
	  {
	    int bound = start_seq + cwnd;
	    if (bound >= NUM_SEQ)
	      bound -= NUM_SEQ;
	    int num_dup = 3;
	    if (timeout)
	      {
		if (cur_seq < start_seq)
		  cur_seq += NUM_SEQ;
		int start = cur_seq - start_seq;
		int n = send(send_sockfd, seg_list[start].output, seg_list[start].len, 0);
		gettimeofday(&begin, 0);
		if (n < 0)
		  {
		    perror("error sending packet\n");
		    exit(errno);
		  }
		  
	      }
	    if (recv(listen_sockfd, input, HEADER_LEN, MSG_DONTWAIT) < 0)
	      {
		gettimeofday(&now, 0);
		int compare = compare_time(now.tv_sec, now.tv_usec, begin.tv_sec, begin.tv_usec);
		
		timeout = (compare >= 500000);
		continue;
	      }
	    else
	      {
		get_data(&data, input, HEADER_LEN);
		printArr(input, HEADER_LEN);
		if (!is_greater(data.acknum, cur_seq, start_seq))
		  {
		    if (data.acknum == last_acknum)
		      dup_count++;
		    else
		      {
			dup_count = 0;
			last_acknum = data.acknum;
		      }
		    if (dup_count == num_dup)
		      {
			dup_count = 0;
			packet_lost = true;
			timeout = true;
		      }
		    fprintf(stderr, "ack does not match %d, dup count = %d\n", cur_seq, dup_count);
		  }
		else
		  {
		    last_acknum = data.acknum;
		    if (is_bounded(last_acknum, cur_seq, cwnd) && is_greater(last_acknum, cur_seq, start_seq))
		      cur_seq = last_acknum;
		    
		    int x = start_seq;
		    x = increment(start_seq, segments_read);
		    printf("cur: %d, bound: %d, x: %d\n", cur_seq, bound, x);
		    
		    if (cur_seq == bound || cur_seq == x)
			break;
		    
		  }
		  
	      }
	  }
	for (int i = 0; i < segments_read; i++)
	  {
	    free(seg_list[i].output);
	  }
	
	if (packet_lost)
	  cwnd /= 2;
	else
	  cwnd++;
	if (cwnd > MAX_WINDOW)
	  cwnd = MAX_WINDOW;
	if (cwnd == 0)
	  cwnd = 1;
	printf("new cwnd: %d\n", cwnd);
	
	
	free(header);
	free(seg_list);
	if (feof(fp))
	  break;
	
      }
    //send ending transaction packet
    struct timeval begin, now;
    struct packet data;
    char input[HEADER_LEN];
    bool timeout = true;
    int beat_count = 0;
    do
      {
	if (timeout)
	  {
	    beat_count++;
	    send_last(send_sockfd);
	    gettimeofday(&begin, 0);
	    if (beat_count > 5)
	      break;
	  }
	if (recv(listen_sockfd, input, HEADER_LEN, MSG_DONTWAIT) < 0)
	  {
	    gettimeofday(&now, 0);
	    int compare = compare_time(now.tv_sec, now.tv_usec, begin.tv_sec, begin.tv_usec);
		
	    timeout = (compare >= 500000);
	    continue;
	  }
	else
	  {
	    get_data(&data, input, HEADER_LEN);
	    if (data.last != 1)
	      {
		fprintf(stderr, "not yet finished\n");
		timeout = true;
	      }
	    else
	      break;
	  }
      } while (1);
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
