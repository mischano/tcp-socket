/* client.c
 * Ischanov, Mansur
 * 
 * Description
 *  client - server IPC using sockers
 * Specification
 *  The client server code makes a request and the server responds.
 * Example  
 *  gcc -Wall server.c -o server -lpthread
 *  gcc -Wall client.c -o client
 *  ./server server _port
 *  ./client server_ip_addr:_port _command
 */

#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define BUF_READ 1024


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if ( sa->sa_family == AF_INET ) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void usererr()
{
    fprintf(stderr, "Usage: client ipaddr:port\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    char usr_inpt_buf[BUFSIZ];
    char s[INET6_ADDRSTRLEN];
    char *ipaddr, *port, *recv_buf;
    int rv, sockfd, bytes_read, i = 0; 
    unsigned long total_bytes_read = 0, recv_buf_size;
    struct addrinfo hints, *servinfo, *p;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }
    while (argv[i] != NULL)
    {
        sprintf(usr_inpt_buf + strlen(usr_inpt_buf), argv[i++]);
        strcat(usr_inpt_buf, " ");
    }
    usr_inpt_buf[strlen(usr_inpt_buf) - 1] = '\0';
    
    ipaddr = strtok(argv[1], ":");
    port = strtok(NULL, " ");
    if (ipaddr == NULL || port == NULL)
    {
        usererr();
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ( (rv = getaddrinfo(ipaddr, port, &hints, &servinfo)) != 0 ) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ( (sockfd = socket(p->ai_family,
                          p->ai_socktype,
                          p->ai_protocol)) == -1 ) {
            perror("client: socket");
            continue;
        }

        if ( connect(sockfd, p->ai_addr, p->ai_addrlen) == -1 ) {
            perror("client: connect");
            close(sockfd);
            continue;
        }

        break;
    }

    if ( p == NULL ) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
        s, sizeof s);
    freeaddrinfo(servinfo); 
    send(sockfd, usr_inpt_buf, strlen(usr_inpt_buf), 0); 
    
    if ((recv_buf = malloc(1024 * sizeof(char))) == NULL)
    {
        fprintf(stderr, "malloc() failed in line %d\n", __LINE__);
        exit(EXIT_FAILURE);
    }
    recv_buf_size = 1024; total_bytes_read = 0;

    do {
        bytes_read = recv(sockfd, recv_buf + total_bytes_read, BUF_READ, 0);
        if (bytes_read <= 0)
        {
            break;
        }
        total_bytes_read += bytes_read;
        if (total_bytes_read >= recv_buf_size)
        {
            recv_buf_size *= recv_buf_size;
            if ((recv_buf = realloc(recv_buf, recv_buf_size)) == NULL)
            {
                fprintf(stderr, "realloc() failed in line %d\n", __LINE__);
                exit(EXIT_FAILURE);
            }
        }
    } while (bytes_read > 0);
    
    recv_buf[total_bytes_read - 1] = '\0';
    write(1, recv_buf, strlen(recv_buf));
    close(sockfd);
    free(recv_buf);

    return 0;
}
