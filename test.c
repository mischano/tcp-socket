#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>

#define IP_PORT "129.65.128.80:4440"
#define BUFSIZE 1024

/* content vector -- containts the content and the size of provided vector */
struct contv {
    char *buf;
    unsigned int buf_size, total_read;
};

int create_pipe_fork(int*, int);
void* my_realloc(char*, unsigned int, int);
void read_from_stdout(struct contv*, int*);
void read_from_curr_dir(struct contv*);
void err_display(char*, int);
void dup_execvp(int*, char**);

/***********************************
 * test01 -- test an empty command.*
 ***********************************/
void test01_empty_input()
{
    char *argv[] = {"./client", IP_PORT, NULL};
    char buf[BUFSIZE];
    unsigned int total_read;
    int fd[2];
    pid_t pid;

    *buf = '\0';
    /* child */
    if ((pid = create_pipe_fork(fd, __LINE__)) == 0)
    {
        dup_execvp(fd, argv);
    }
    wait(NULL); /* end child */

    close(*(fd + 1));
    
    /* read client.c output from stdout */
    if ((total_read = read(*fd, buf, BUFSIZE)) == -1)
    {
        err_display("read()", __LINE__);
    }
    close(*fd);
    
    /* test01 test cases */
    assert(total_read == 0);
    assert(strlen(buf) == 0);
}

/*******************************
 * test02-- test index command.*
 *******************************/
void test02_index()
{
    char *argv[] = {"./client", IP_PORT, "index", NULL}; // client argv
    int fd[2];
    struct contv sos, ds; /* stdout struct, dir struct */
    pid_t pid;
    
    /* if child */
    if ((pid = create_pipe_fork(fd, __LINE__)) == 0)
    {
        dup_execvp(fd, argv);
    }
    wait(NULL); /* end child */

    close(*(fd + 1));
    if ((sos.buf = malloc(BUFSIZE * sizeof(char))) == NULL)
    {
        err_display("malloc()", __LINE__);
    }
    sos.buf_size = BUFSIZE;
    *(sos.buf) = '\0';
    
    /* read client.c output from stdout */
    read_from_stdout(&sos, fd);

    if ((ds.buf = malloc(BUFSIZE * sizeof(char))) == NULL)
    {
        err_display("malloc()", __LINE__);
    }
    ds.buf_size = BUFSIZE;
    ds.total_read = 0;
    *(ds.buf) = '\0';
    
    read_from_curr_dir(&ds);

    /* test02 test cases */
    assert(sos.total_read == ds.total_read);
    assert(strcmp(sos.buf, ds.buf)); 
    
    free(sos.buf);
    free(ds.buf);
}

/* void test03_small_file()
{

} */

int main()
{
    test01_empty_input();
    printf("test01 passed successfully\n"); 
    test02_index();
    printf("test02 passed successfully\n");  

    return (0);
}
void read_from_curr_dir(struct contv *ds)
{
    DIR *dr;
    struct dirent *de;

    if ((dr = opendir(".")) == NULL)
    {
        err_display("opendir()", __LINE__);
    }
    while ((de = readdir(dr)) != NULL)
    {
        if ((strcmp(de->d_name, ".") == 0) 
            || (strcmp(de->d_name, "..") == 0))
        {
            continue;
        }

        ds->total_read = ds->total_read + strlen(de->d_name) + 1;
        if (ds->total_read > ds->buf_size)
        {
            ds->buf = my_realloc(ds->buf, ds->buf_size, __LINE__);
        }
        strcat(ds->buf, de->d_name);
        *(ds->buf + ds->total_read - 1) = '\n';
    }
    strcat(ds->buf, "\n");
    ds->total_read++; /* add newline byte */
}

void read_from_stdout(struct contv *c, int *fd)
{
    int read_chunk;

    if ((read_chunk = read(*fd, c->buf, BUFSIZE)) == BUFSIZE)
    {
        c->total_read = 0;
        while (read_chunk != 0)
        {
            c->total_read += read_chunk;
            if (c->total_read == c->buf_size)
            {
                my_realloc(c->buf, c->buf_size, __LINE__);
            }
            if ((read_chunk = read(*fd, c->buf + c->total_read, BUFSIZE))
                == -1)
            {
                err_display("read()", __LINE__);
            }
        }
    } 
    else 
    {
        if (read_chunk == -1)
        {
            err_display("read()", __LINE__);
        }
        c->total_read = read_chunk;
    } 
    c->total_read++; /* add null terminating byte */
}

void *my_realloc(char *ptr, unsigned int size, int err_line)
{
    char *tmp;
    
    size *= 2;
    if ((tmp = realloc(ptr, size)) == NULL)
    {
        fprintf(stderr, "this is all I could read...\n%s\n", ptr);
        fprintf(stderr, "failed in lien %d\n", err_line);
        exit(EXIT_FAILURE);
    }
    ptr = tmp;

    return (ptr);
}

int create_pipe_fork(int *fd, int err_line)
{
    pid_t pid;

    if (pipe(fd) == -1)
    {
        err_display("pipe()", err_line);
    }
    
    if ((pid = fork()) == -1)
    {
        err_display("fork()", err_line);
    }

    return (pid);
}


void dup_execvp(int *fd, char **argv)
{
    dup2(fd[1], STDOUT_FILENO);
    close(fd[0]);
    close(fd[1]);

    execvp("./client", argv);
    err_display("execl()", __LINE__);
}

void err_display(char *str, int err_line)
{
    fprintf(stderr, "%s failed in err_line %d\n", str, err_line);
    exit(EXIT_FAILURE);
}

