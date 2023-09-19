/* MODIFY THIS FILE
 * together with sure.c, this implements the SURE protocol
 * for reliable data transfer.
 */

#ifndef SURE_H
#define SURE_H

#include "udt.h"
#include <time.h>
#include <pthread.h>

// return codes to be used by SURE functions. DO NOT MODIFY
#define SURE_SUCCESS UDT_SUCCESS
#define SURE_FAILURE UDT_FAILURE

// SURE is a unidirectional protocol, data is sent from sender to receiver only
#define SURE_RECEIVER UDT_RECEIVER
#define SURE_SENDER UDT_SENDER

// this is the size of the payload of SURE packets.
// The whole packet must have at most UDT_PACKET_SIZE,
// but in this limit we must fit the SURE header
//(i.e. whatever else we want to add to sure_packet_t
// MODIFY THIS!!!
#define SURE_PACKET_SIZE UDT_PACKET_SIZE

// the size of the buffer, you can modify this if you want
#define SURE_BUFFER 64 // in number of packets, not bytes

// the size of the sender window, you can modify this if you want
#define SURE_WINDOW 10 // in number of packets, not bytes

// time out max number
#define SENDER_TMAX 10
// for how long must we wait before assuming the packet was lost?
// you can modify this if you want
#define SURE_TIMEOUT 2000000 // in microseconds

// how many times can we retransmit the fin packet before giving up
// you can modify this if you want
#define SURE_FIN_TIMEOUT 4 // in number of times

// taille headers en octet
#define HEADER 8
// taille du data
#define DATASIZE (SURE_PACKET_SIZE - (sizeof(char) * HEADER))
// this is the SURE segment
// MODIFY THIS!!!
typedef struct sure_packet
{
    char align;
    short ACK;
    char etat;
   
    short datasize;
    
    char data[SURE_PACKET_SIZE - (sizeof(char) * HEADER)];
    
    // the payload
} sure_packet_t;

// this is the struct the application will provide to all SURE calls,
// it MUST hold all the variables related to the connection (you may
// NOT declare them as global variables), for example the buffer
// MODIFY THIS!!!
typedef struct sure_socket
{
    // ADD OTHER VARIABLES HERE
    pthread_t thread;
    pthread_mutex_t mutex_read, mutex_write, mutex_timeout_send;
    pthread_cond_t cond_init, cond_connec;
    int cur;
    int pro;
    int fin;
    int cpt; // compteur de sure_read
    char etp;
    short ack_num;
    int cptpacket;
    int connect;
    sure_packet_t sure_buffer[1];
    sure_packet_t bigbuffer[SURE_BUFFER];
    bool timeout;
    udt_socket_t udt; // used by the lower-level protocol
} sure_socket_t;

// below are the interface the SURE protocol provides for applications
// DO NO MODIFY!

// initialize our SURE reliable transport using
// the underlying udt unreliable transport service.
// side will be SURE_RECEIVER or SURE_SENDER to
// determine what side we are initializing.
// If side is SURE_RECEIVER, the receiver argument
// will be ignored (it may be NULL)
// A connection will be established during this call (it may block
// for a while)
// This function returns SURE_SUCCESS or
// SURE_FAILURE
int sure_init(char *receiver, int port, int side, sure_socket_t *p);

// ends the SURE transport, doesn't return anything
// in the sender, wait until all the data has been sent before
// closing the connection (it may block for a while)
void sure_close(sure_socket_t *p);

// return SURE_FAILURE or the number of bytes read
int sure_read(sure_socket_t *s, char *msg, int msg_size);
// return SURE_FAILURE or the number of bytes written
// data may just be copied to a buffer and not really written when
// this function returns. If you want to measure the time to send
// data, stop de timer after calling sure_close
int sure_write(sure_socket_t *s, char *msg, int msg_size);

#endif
