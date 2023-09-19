/* this is an application-layer code that will
 * copy a file from source to destination machines
 * using our fictitious SURE protocol.
 * The receiver-side (receive_file.c) must be
 * running before calling the sender-side.
 * Author: Francieli Boito, February 2022
 * This file was prepared for a teaching activity.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "sure.h"

#define APP_MSG_SIZE (SURE_PACKET_SIZE- HEADER) * 64
// #define APP_MSG_SIZE 1024
#define PORT 3001

#define TIMESPEC_DIFF(t1, t2) ((t2.tv_nsec - t1.tv_nsec) + ((t2.tv_sec - t1.tv_sec) * 1000000000L))

void usage(char *cmd)
{
    fprintf(stderr, "Usage: %s [destination] [file name] \n", cmd);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int fd, ret, bytes;
    long int total = 0;
    double elapsed;
    struct timespec before, after;
    sure_socket_t s;
    char buf[APP_MSG_SIZE];

    // verify arguments
    if (argc != 3)
        usage(argv[0]);
    // try to open the file
    fd = open(argv[2], O_RDONLY);
    if (fd < 0)
        usage(argv[0]);
    // create our "socket" using our SURE protocol (and establish the connection)
    ret = sure_init(argv[1], PORT, SURE_SENDER, &s);
    printf("IN copy_file: udtside=%d ret=%d\n", s.udt.side,ret);
    if (ret != SURE_SUCCESS)
    {
        printf("IN copy_file: exit failure");
        exit(EXIT_FAILURE);
    }
    // read and send the file using our socket
    clock_gettime(CLOCK_MONOTONIC, &before);

    printf("IN copy_file: SURE INIT SUCces");

    do
    {
        bytes = read(fd, buf, APP_MSG_SIZE);
        if (bytes > 0)
        {
            printf("IN copy_file:read =%d\n",bytes);
            //printf("IN copy_file: avant sure_write avec side = %d et bytes = %d\n", s.udt.side, bytes);
            ret = sure_write(&s, buf, bytes);
            //printf("IN copy_file: apres sure_write side = %d et bytes = %d\n", s.udt.side, bytes);
            if (ret != bytes)
            {
                exit(EXIT_FAILURE);
                printf("IN copy_file: exitfailure rater sure_write ");
            }
            total += ret;
        }
    } while (bytes > 0);
    close(fd);
    sure_close(&s); // end the connection
    clock_gettime(CLOCK_MONOTONIC, &after);
    elapsed = (1.0 * TIMESPEC_DIFF(before, after)) / 1000000000L;
    printf("IN copy_file: Sent %ld bytes in %lf seconds, throughput = %lf\n", total, elapsed, (1.0 * total) / elapsed);
    printf("IN copy_file: Did the file arrive perfectly? You should check!\n");
    return EXIT_SUCCESS;
}
