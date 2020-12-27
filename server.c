/* server.c
 * Ischanov, Mansur
 *
 * Description
 *  client - server IPC using sockets 
 * Specifications
 *  The client server code makes a request and the server responds.
 * Examples
 *  gcc -Wall server.c -o server -lpthread 
 *  gcc -Wall client.c -o client
 *  ./server server _port
 *  ./client server_ip_add:_port _command 
 */

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BACKLOG 10 // how many pending connections queue will hold

typedef struct file_info {
    char client_ip_addr[100];
    unsigned int bufsize;
} file_info;

void *tokenize(char*);
void *parse_input(char**, char*, file_info*);
void log_append(char**, char*);

pthread_mutex_t lock;
file_info finfo;

void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while ( waitpid(-1, NULL, WNOHANG) > 0 );

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if ( sa->sa_family == AF_INET ) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv)
{
    char **token;
    char *out_buf; 
    char s[INET6_ADDRSTRLEN], in_buf[BUFSIZ];
    int sockfd, new_fd, rv, yes = 1;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    struct sigaction sa;
    socklen_t sin_size;
    
    if (argc != 2)
    {
        fprintf(stderr, "Usage: server port # example port 4443\n");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        fprintf(stderr, "mutex() failed in line %d\n", __LINE__);
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return (1);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, 
                p->ai_protocol)) == -1) 
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) 
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // listen allows queue of up to BACKLOG number
    if (listen(sockfd, BACKLOG) == -1) 
    {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if ( sigaction(SIGCHLD, &sa, NULL) == -1) 
    {
        perror("sigaction");
        exit(1);
    }

    while (1) 
    {  
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) 
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, 
            get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        strcat(finfo.client_ip_addr, s);

        if (!fork()) 
        { 
            close(sockfd); // child doesn't need the listener
            
            if (recv(new_fd, in_buf, sizeof(in_buf) -1, 0) == -1)
            {
                perror("recv");
            }

            if ((out_buf = malloc(1024 * sizeof(char))) == NULL)
            {
                fprintf(stderr, "malloc() failed in line %d\n", __LINE__);
                exit(EXIT_FAILURE);
            } 
            finfo.bufsize = 1024;

            token = tokenize(in_buf);
            out_buf = parse_input(token, out_buf, &finfo);
            
            if (send(new_fd, out_buf, strlen(out_buf), 0) == -1)
            {
                perror("send"); 
            }
            
            close(new_fd);
            free(out_buf);
            free(token);

            exit(EXIT_SUCCESS);
        }
        close(new_fd);  
    }
    pthread_mutex_destroy(&lock);

    return (0);
}

void *parse_input(char **input, char *output, file_info *finfo)
{
    int fd, file_found; 
    unsigned long file_size;
    off_t curr_pos, total_read = 0;
    size_t read_bytes; 
    char message[100];
    char reject[] = ")(*&^%$#@?!`~-+0123456789";
    struct dirent *de;
    DIR *dr;
    
    *output = '\0'; *message = '\0';
    if (*(input + 1) == NULL)
    {
        return (output);
    }
    
    if ((dr = opendir(".")) == NULL)
    {
        fprintf(stderr, "opendir() failed in line %d\n", __LINE__);
        exit(EXIT_FAILURE);
    }
    // message[0] = '\0';
    
    if ((strcmp(*(input + 1), "index")) == 0)
    {
        while ((de = readdir(dr)) != NULL)
        {
            if ((strcmp(de->d_name, "..") != 0) && 
                (strcmp(de->d_name, ".") != 0))
            {
                strcat(output, de->d_name);
                strcat(output, "\n");
                total_read += strlen(de->d_name) + 1;
            }
        }
        strcat(output, "\n");
        total_read++;
        strcat(message, "index ");
        sprintf(message + strlen(message), "%lu\n", total_read);
        log_append(input, message);
    }
    else if ((strcmp(*(input + 1), "server")) == 0 || 
        (strcmp(*(input + 1), "client")) == 0)
    {
        strcat(message, *(input + 1));
        strcat(message, " NOT_ALLOWED\n");
        log_append(input, message);
    }
    else if (strcmp(*(input + 1), "log") == 0)
    {
        if ((fd = open("log.log", O_RDONLY)) == -1)
        {
            file_found = 1;
            log_append(input, "log 0\n");
            return (output);
        }
        file_found = 1;
        curr_pos = lseek(fd, (size_t)0, SEEK_CUR);
        file_size = lseek(fd, 0L, SEEK_END);
        lseek(fd, curr_pos, SEEK_SET); 
        if ((output = realloc(output, file_size + 10)) == NULL)
        {
            fprintf(stderr, "realloc() failed in line"
            "%d\n", __LINE__);
            exit(EXIT_FAILURE);
        }
        finfo->bufsize = file_size + 10;
        while (total_read < file_size)
        {
            if ((read_bytes = read(fd, ((char *)output) + total_read, 1024)) 
                < 0)
            {
                log_append(input, "NOT_READABLE\n");
            }
            else 
            {
                total_read = total_read + read_bytes;
            }
        }
        strcat(output, " ");
        sprintf(message, "log %lu\n", total_read);
        log_append(input, message);
    } 
    else 
    {
        file_found = 0;
        while ((de = readdir(dr)) != NULL)
        {
            if (strcmp(de->d_name, *(input + 1)) == 0)
            {
                if ((strcspn(de->d_name, reject) != strlen(de->d_name)) ||
                    (de->d_name[0] == '.'))
                {
                    log_append(input, "BAD_FILENAME\n");
                    return (output);
                }

                if ((fd = open(de->d_name, O_RDONLY)) == -1)
                {
                    file_found = 1;
                    log_append(input, "NOT_READABLE\n");
                }
                else
                {
                    file_found = 1;
                    curr_pos = lseek(fd, (size_t)0, SEEK_CUR);
                    file_size = lseek(fd, 0L, SEEK_END);
                    lseek(fd, curr_pos, SEEK_SET); 
                    
                    if (finfo->bufsize <= file_size)
                    {
                        if ((output = 
                                realloc(output, file_size + 10)) == NULL)
                        {
                            fprintf(stderr, "realloc() failed in line"
                            "%d\n", __LINE__);
                            exit(EXIT_FAILURE);
                        }
                        finfo->bufsize = file_size + 10;
                    }
                    while (total_read < file_size)
                    {
                        if ((read_bytes = 
                            read(fd, ((char *)output) + total_read, 1024)) 
                            < 0)
                        {
                            log_append(input, "NOT_READABLE\n");
                        }
                        else 
                        {
                            total_read = total_read + read_bytes;
                        }
                    }
                    strcat(output, " ");
                    
                    if (total_read < 1000)
                    {
                        sprintf(message, "%lu\n", total_read);
                    }
                    else 
                    {
                        strcat(message, "bigfile ");
                        sprintf(message + strlen(message), "%lu\n", 
                            total_read);
                        log_append(input, message);
                    }
                    log_append(input, message);
                }
            }
        }
        if (!file_found)
        {
            log_append(input, "NOT_FOUND\n");
        }
        close(fd);
    } 
    return (output);
}

void *tokenize(char *buffer)
{
    char **token;
    char *tmp;
    int i = 0;

    if ((token = malloc(1010101010 * sizeof(char))) == NULL)
    {
        fprintf(stderr, "malloc() failed in line %d\n", __LINE__);
        exit(EXIT_FAILURE);
    }
    tmp = strtok(buffer, " ");
    while (tmp != NULL)
    {
        tmp = strtok(NULL, " ");
        *(token + i) = tmp;
        i++;
    }
    tmp = strchr(*token, ':');
    strcat(finfo.client_ip_addr, tmp);

    return (token);
}

void log_append(char **request, char* message)
{
    char date[200];
    int fd, hr, min, sec, dd, mm, yy; 
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    
    hr = local->tm_hour;
    min = local->tm_min;
    sec = local->tm_sec;
    dd = local->tm_mday;
    mm = local->tm_mon + 1;
    yy = local->tm_year + 1900;
    
    pthread_mutex_lock(&lock); 
    if ((fd = open("log.log", O_WRONLY | O_APPEND | O_CREAT)) < 0)
    {
        chmod("log.log", 0777);
        if ((fd = open("log.log", O_WRONLY | O_APPEND | O_CREAT)) < 0)
        {
            fprintf(stderr, "open() failed in line %d\n", __LINE__);
            exit(EXIT_FAILURE);
        }
    }

    if (sprintf(date, "[%d-%d-%d %d:%d:%d] ", yy, mm, dd, hr, min, sec) < 0)
    {
        fprintf(stderr, "sprintf() failed in line %d\n", __LINE__);
        exit(EXIT_FAILURE);
    }

    if (write(fd, date, strlen(date)) < 0)
    {
        fprintf(stderr, "write() failed in line %d\n", __LINE__);
        exit(EXIT_FAILURE);
    }
    if ((write(fd, finfo.client_ip_addr, strlen(*request))) < 0)
    {
        fprintf(stderr, "write() failed in line %d\n", __LINE__);
        exit(EXIT_FAILURE);
    }
    write(fd, " ", 1);
    write(fd, message, strlen(message));
    date[0] = '\0';
    pthread_mutex_unlock(&lock);

    close(fd);
}

