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
/* DO NOT MODIFY THIS FILE!!!
 * If you find a bug, please notify the author
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include "udt.h"


//initialize an unreliable transfer medium (it creates
//a UDP socket and fills the udt_socket_t structure
//with the socket file descriptor and a filled struct
//sockaddr_in)
//side determines what communication side we are
//initializing.
//If side is UDT_RECEIVER, the receiver
//argument will be ignored (it can be NULL
//If side is UDT_SENDER, the receiver IP adress must
//be provided.
//This function returns UDT_SUCCESS or UDT_FAILURE
int udt_init(char *receiver, int port, int side, udt_socket_t *p) {
    int ret;
    struct sockaddr_in sender;

    if (!p) return UDT_FAILURE;
    //create the socket
    p->s = socket(AF_INET, SOCK_DGRAM, 0);
    if (p->s == -1) {
        perror("udt - creating UDP socket");
        return UDT_FAILURE;
    }
    p->port = port;
    if (side == UDT_SENDER) {
        if(!receiver) return UDT_FAILURE;
        //fill a structure with the receiver's address
        memset(&(p->recv_addr), 0, sizeof(p->recv_addr));
        p->recv_addr.sin_family = AF_INET;
        p->recv_addr.sin_port = htons(port);
        p->recv_addr.sin_addr.s_addr = inet_addr(receiver);
    } else {
        memset(&sender, 0, sizeof(sender));
        sender.sin_family = AF_INET;
        sender.sin_port = htons(port);
        sender.sin_addr.s_addr = INADDR_ANY;
        //bind socket to the sender's address (in this case, any)
        ret = bind(p->s, (struct sockaddr *)&sender, sizeof(sender));
        if(ret == -1) {
            perror("UDT - bind");
            return UDT_FAILURE;
        }
    }
    p->side = side;
    srand((unsigned int) time(NULL)); //for random number generation (used in udt_send)
    return UDT_SUCCESS;
}

//close an unreliable data transport medium (in
//implementation, simply close the socket in the
//udt_socket_t structure)
void udt_close(udt_socket_t *p) {
    close(p->s);
}

//randomly decide (with probability PROB_LOSS) if
//a packet must be "lost"
bool should_i_drop() {
    return (rand() % 100 < PROB_LOSS);
}

// sends a packet through our unreliable channel
// (in implementation, uses sendto to send through
// UDP). This packet may in fact be lost in the way
// (the probability of that happening is given by
// PROB_LOSS). When that happens, this function will
// NOT return any error, as there is no way for the
// sender to know that the message was lost in the
// way.
// returns the number of bytes sent or UDT_FAILURE
int udt_send(udt_socket_t *p, char *buf, int size) {
    int ret;

    if (size > UDT_PACKET_SIZE) return UDT_FAILURE;
    if (!p || !buf) return UDT_FAILURE;
    //we may lose this packet
    if(should_i_drop()) {
        return UDT_SUCCESS;
    }
    //use UDP to send this data to the receiver
    ret = sendto(p->s, buf, size, 0, (struct sockaddr *) &(p->recv_addr), sizeof(p->recv_addr));
    if(ret != size) {
        perror("UDT - sendto");
    }
    return ret;
}

// receive a packet from the unreliable channel
// (in reality, using recvfrom with UDP).
// If called by the UDT_RECEIVER, the address of
// the sender will be written to the sockaddr_in
// struct in the udt_socket_t struct.
// returns the number of bytes read, UDT_TIMEOUT or
// UDT_FAILURE
int udt_recv(udt_socket_t *p, char *buf, int size) {
    unsigned int len;
    int ret;

    if(!p || !buf) return UDT_FAILURE;
    if(p->side == UDT_RECEIVER) {
        memset(&(p->recv_addr), 0, sizeof(p->recv_addr));
        len = sizeof(p->recv_addr);
        ret = recvfrom(p->s, buf, size, 0, (struct sockaddr *)&(p->recv_addr), &len);
    } else {
        ret = recvfrom(p->s, buf, size, 0, NULL, NULL);
    }
    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return UDT_TIMEOUT;
    else if (ret < 0) {
        perror("UDT - recvfrom");
        return UDT_FAILURE;
    } else return ret;
}

//configures a timeout (given in microseconds) for udt_rect
void udt_set_timeout(udt_socket_t *s, long usec) {
    int ret;
    struct timeval timeout;

    //fill the timeout
    timeout.tv_sec = usec/1000000L;
    timeout.tv_usec = usec % 1000000L;
    //set it up
    ret = setsockopt(s->s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if(ret == -1) {
        perror("UDT - setsockopt");
    }
}
