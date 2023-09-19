/* this in an unreliable data transfer medium
 * data is sent by calling udt_send, and obtained
 * at receiver side by calling udt_recv
 * It is implemented by using an UDP socket and
 * randomly dropping packets (with a configurable
 * probability PROB_LOSS)
 * Author: Francieli Boito, February 2022
 * This file was prepared for a teaching activity,
 * you CANNOT share it online (for instance, in
 * a forum or in a git repo) unless you receive
 * explicit authorization from the author.
 */
/* DO NOT MODIFY THIS FILE!!! (except for changing
 * parameters in order to perform experiments, such
 * as the PROB_LOSS)
 * If you find a bug, please notify the author
 */
#ifndef UDT_H
#define UDT_H

#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* return codes for the functions provided by this module */
#define UDT_SUCCESS 0
#define UDT_FAILURE -1
#define UDT_TIMEOUT -2

/* we implement unidirectional communication: there's a sender and a receiver */
#define UDT_RECEIVER 0
#define UDT_SENDER 1

/* maximum packet size */
#define UDT_PACKET_SIZE 512

/*the probability (%) of a packet being lost in the way to the other host.
 *this is implemented by randomly (with this probability) NOT sending
 *packets in the udt_send function*/
#define PROB_LOSS 10 //must be between 0 and 99

/*a socket, which will be initialized by calling udt_init and then given
 * in each call to recvfrom or send */
typedef struct udt_socket {
    int s; //the UDP socket
    int port;
    int side; //receiver or sender
    struct sockaddr_in recv_addr; //the address of the other side. In the case of the receiver, it will only be filled when we call udt_recv for the first time
} udt_socket_t;

//initialize an unreliable transfer medium (it creates
//a UDP socket and fills the udt_socket_t structure
//with the socket file descriptor and a filled struct
//sockaddr_in for the sender (zeroes for the receiver))
//side determines what communication side we are
//initializing.
//If side is UDT_RECEIVER, the receiver
//argument will be ignored (it can be NULL)
//If side is UDT_SENDER, the receiver IP address must
//be provided.
//This function returns UDT_SUCCESS or UDT_FAILURE
int udt_init(char *receiver, int port, int side, udt_socket_t *p);

//close an unreliable data transport medium (in
//implementation, simply close the socket in the
//udt_socket_t structure)
void udt_close(udt_socket_t *p);

// sends a packet through our unreliable channel
// (in implementation, uses sendto to send through
// UDP). This packet may in fact be lost in the way
// (the probability of that happening is given by
// PROB_LOSS). When that happens, this function will
// NOT return any error, as there is no way for the
// sender to know that the message was lost in the
// way.
// returns the number of bytes sent or UDT_FAILURE
int udt_send(udt_socket_t *p, char *buf, int size);

// receive a packet from the unreliable channel
// (in reality, using recvfrom with UDP).
// If called by the UDT_RECEIVER, the address of
// the sender will be written to the sockaddr_in
// struct in the udt_socket_t struct.
// returns the number of bytes read, UDT_TIMEOUT or
// UDT_FAILURE
int udt_recv(udt_socket_t *p, char *buf, int size);

//configures a timeout (given in microseconds) that
//will be used in all the next calls to udt_recv
//this function does not block and does not return
//anything (it's udt_recv that will return UDT_TIMEOUT
//when it gives up on reading after a timeout)
void udt_set_timeout(udt_socket_t *s, long usec);
#endif
