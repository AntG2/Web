#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// MACROS
#define SERVER_IP "127.0.0.1"
#define LOCAL_HOST "127.0.0.1"
#define SERVER_PORT_TO 5002
#define CLIENT_PORT 6001
#define SERVER_PORT 6002
#define CLIENT_PORT_TO 5001
#define PAYLOAD_SIZE 1024
#define WINDOW_SIZE 5
#define TIMEOUT 2
#define MAX_SEQUENCE 1024
#define HEADER_LEN 14
#define NUM_SEQ 20
#define MAX_WINDOW NUM_SEQ / 2




// Packet Layout
// You may change this if you want to
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char ack;
  char last;
    unsigned int length;
    char* payload;
};

struct segment {
  int seqnum;
  int len;
  char* output;
};

void append(char* arr, int ind, unsigned int val)
{
    arr[ind] = (val >> 8) & 0xff;
    arr[ind + 1] = val & 0xff;
}

char* makeHeader(int port, int port_to, int length, int seqnum, int acknum, char ack, char last)
{
  char* header = malloc(HEADER_LEN * sizeof(char));
  append(header, 0, port);
  append(header, 2, port_to);
  append(header, 4, length);
  append(header, 6, 0);
  append(header, 8, seqnum);
  append(header, 10, acknum);
  header[12] = ack;
  header[13] = last;
  return header;
}


void printArr(char* c, int len)
{
  for (int i = 0; i < len; i++)
    {
      printf("%i ", c[i]);
    }
  printf("\n");
}

void concatenate(char* dest, char* src, int ind, int len)
{
  for (int i = ind; i < ind + len; i++)
    {
      dest[i] = src[i - ind];
    }
}

int compute_checksum(char* buffer, int len)
{
  //printf("begin computation: \n");
  unsigned int total = 0;
   for (int i = 0; i < len; i += 2)
    {
      //printf("num: 0x%x, ", buffer[i]);
      total += (unsigned int) ((buffer[i]& 0xff) << 8);
      if (i+1 < len)
	{
	  //printf("0x%x, ", buffer[i+1]);
	  total += (unsigned int) (buffer[i+1] & 0xff);
	}
      if (total > 0xffff)
	total -= 0xffff;
      //printf("current: 0x%x\n", total);
    }
    return ~total & 0xffff;
}

int increment(int seq, int x)
{
  seq += x; 
  if (seq >= NUM_SEQ)
    seq -= NUM_SEQ;
  return seq;
}


void get_data(struct packet* data, char* buffer, int buf_len)
{
  if (buf_len < HEADER_LEN)
    fprintf(stderr, "packet error\n");
  data->length = ((buffer[4] & 0xff) << 8)+(buffer[5] & 0xff);
  data->seqnum = ((buffer[8] & 0xff) << 8)+(buffer[9] & 0xff);
  data->acknum = ((buffer[8] & 0xff) << 10)+(buffer[11] & 0xff);
  data->ack = buffer[12] & 0xff;
  data->last = buffer[13] & 0xff;
  data->payload = malloc(PAYLOAD_SIZE * sizeof(char));
  strcpy(data->payload, buffer + HEADER_LEN);
}

void send_last(int send_sockfd)
{
  char* header = makeHeader(CLIENT_PORT, CLIENT_PORT_TO, HEADER_LEN, 0, 0, 0, 1);
  append(header, 6, compute_checksum(header, HEADER_LEN));
  int n = send(send_sockfd, header, HEADER_LEN, 0);
  if (n<0)
    {
      perror("error sending ack\n");
      exit(errno);
    }
  free(header);
}

bool is_greater(int seq_num, int expected_seq, int start_seq)
{
  //printf("seq: %d, expect: %d, start: %d\n", seq_num, expected_seq, start_seq);
  if (seq_num < start_seq)
    seq_num += NUM_SEQ;
  if (expected_seq < start_seq)
    expected_seq += NUM_SEQ;
  return seq_num > expected_seq;
  
}

bool bufferable(int seq, int expected)
{
  if (seq < expected)
    seq += NUM_SEQ;
  return (seq - expected) < MAX_WINDOW;
}

bool is_bounded(int seq, int cur, int cwnd)
{
  if (seq < cur)
    seq += NUM_SEQ;
  return (seq - cur) <= cwnd;
}

bool is_valid_ack(int seq, int start, int cwnd)
{
  
  int end = start + cwnd;
  if (end >= NUM_SEQ)
    end -= NUM_SEQ;
  if ((start < end && (seq >= end || seq < start))
      || (start > end && (seq >= end && seq < start)))
    return false;
  
  return true;
}

int compare_time(long sec1, long usec1, long sec2, long usec2)
{
  if (sec1 == sec2)
    return usec1 - usec2;
  else // sec1 > sec2
    return usec1 + 1000000 - usec2;
}



// Utility function to build a packet
void build_packet(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char last, char ack,unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->ack = ack;
    pkt->last = last;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// Utility function to print a packet
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s\n", pkt->seqnum, pkt->length, pkt->last ? " LAST": "", (pkt->ack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
}

#endif
