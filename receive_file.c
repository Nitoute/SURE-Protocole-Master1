/* this is an application-layer code that will
 * receive files from other hosts using the
 * fictitious SURE protocol.
 * This side (the receiver side) must be running
 * before the other side (copy_file) is called.
 * Author: Francieli Boito, February 2022
 * This file was prepared for a teaching activity
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "sure.h"

#define APP_MSG_SIZE (SURE_PACKET_SIZE - HEADER) * 64
#define PORT 3001

int main(int argc, char *argv[])
{
    int ret, bytes, fd;
    sure_socket_t s;
    char filename[APP_MSG_SIZE];
    char buf[APP_MSG_SIZE];
    int file_count = 0;

    while (true)
    {
        printf("waiting for the next file\n");
        // create our "socket" using our SURE protocol (and establish the connection)
        ret = sure_init(NULL, PORT, SURE_RECEIVER, &s);
        if (ret != SURE_SUCCESS)
            exit(EXIT_FAILURE);
        // make the name of the file
        file_count++;
        sprintf(filename, "%d.received", file_count);
        printf("Will create file %s\n", filename);
        // create the file
        fd = open(filename, O_CREAT | O_WRONLY, 0600);
        if (fd < 0)
        {
            perror("open");
            return EXIT_FAILURE;
        }
        // receive and write the file using our socket
        do
        {
            bytes = sure_read(&s, buf, APP_MSG_SIZE);
            if (bytes > 0)
            {
                ret = write(fd, buf, bytes);
                if (ret != bytes)
                {
                    perror("write");
                    return EXIT_FAILURE;
                }
            }
        } while (bytes > 0);
        close(fd);
        sure_close(&s); // end the connection
        printf("Done with file %s\n", filename);
    } 
    return EXIT_SUCCESS;
}
