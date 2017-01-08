/**
 * Multithreaded, libevent 2.x-based socket server.
 * Copyright (c) 2012 Qi Huang
 * This software is licensed under the BSD license.
 * See the accompanying LICENSE.txt for details.
 *
 * To compile: ./make
 * To run: ./echoserver_threaded
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <signal.h>

#include "commands.h"
#include "workqueue.h"

/* Port to listen on. */
#define SERVER_PORT 5555
/* Connection backlog (# of backlogged connections to accept). */
#define CONNECTION_BACKLOG 8
/* Number of worker threads.  Should match number of CPU cores reported in 
 * /proc/cpuinfo. */
#define NUM_THREADS 8

/* Behaves similarly to fprintf(stderr, ...), but adds file, line, and function
 information. */
#define errorOut(...) {\
    fprintf(stderr, "%s:%d: %s():\t", __FILE__, __LINE__, __FUNCTION__);\
    fprintf(stderr, __VA_ARGS__);\
}

// Behaves similarly to printf(...), but adds file, line, and function
// information.  I omit do ... while(0) because I always use curly braces in my
// if statements.
#define INFO_OUT(...) {\
	printf("%s:%d: %s():\t", __FILE__, __LINE__, __FUNCTION__);\
	printf(__VA_ARGS__);\
}

// Behaves similarly to fprintf(stderr, ...), but adds file, line, and function
// information.
#define ERROR_OUT(...) {\
	fprintf(stderr, "\e[0;1m%s:%d: %s():\t", __FILE__, __LINE__, __FUNCTION__);\
	fprintf(stderr, __VA_ARGS__);\
	fprintf(stderr, "\e[0m");\
}

// Behaves similarly to perror(...), but supports printf formatting and prints
// file, line, and function information.
#define ERRNO_OUT(...) {\
	fprintf(stderr, "\e[0;1m%s:%d: %s():\t", __FILE__, __LINE__, __FUNCTION__);\
	fprintf(stderr, __VA_ARGS__);\
	fprintf(stderr, ": %d (%s)\e[0m\n", errno, strerror(errno));\
}

/**
 * Struct to carry around connection (client)-specific data.
 */
typedef struct client {
    /* The client's socket. */
    int fd;

    /* The event_base for this client. */
    struct event_base *evbase;

    /* The bufferedevent for this client. */
    struct bufferevent *buf_ev;

    /* The output buffer for this client. */
    struct evbuffer *output_buffer;

    /* Here you can add your own application-specific attributes which
     * are connection-specific. */
    long valsize;
} client_t;

static struct event_base *evbase_accept;
static workqueue_t workqueue;

/* Signal handler function (defined below). */
static void sighandler(int signal);

static void closeClient(client_t *client) {
    if (client != NULL) {
        if (client->fd >= 0) {
            close(client->fd);
            client->fd = -1;
        }
    }
}

static void closeAndFreeClient(client_t *client) {
    if (client != NULL) {
        closeClient(client);
        if (client->buf_ev != NULL) {
            bufferevent_free(client->buf_ev);
            client->buf_ev = NULL;
        }
        if (client->evbase != NULL) {
            event_base_free(client->evbase);
            client->evbase = NULL;
        }
        if (client->output_buffer != NULL) {
            evbuffer_free(client->output_buffer);
            client->output_buffer = NULL;
        }
        free(client);
    }
}


/* Count the total occurrences of 'str' in 'buf'. */
int count_instances(struct evbuffer *buf, const char *str)
{
    size_t len = strlen(str);
    int total = 0;
    struct evbuffer_ptr p;

    if (!len)
        /* Don't try to count the occurrences of a 0-length string. */
        return -1;

    evbuffer_ptr_set(buf, &p, 0, EVBUFFER_PTR_SET);

    while (1) {
         p = evbuffer_search(buf, str, len, &p);
         if (p.pos < 0)
             break;
         total++;
         evbuffer_ptr_set(buf, &p, 1, EVBUFFER_PTR_ADD);
    }

    return total;
}

size_t str_firstpos(struct evbuffer *buf, const char *str)
{
    size_t len = strlen(str);
    struct evbuffer_ptr p;

    if (!len)
        /* Don't try to count the occurrences of a 0-length string. */
        return -1;

    p = evbuffer_search(buf, str, len, NULL);
    return p.pos;
}

void write_msg(struct bufferevent *buf_event, struct client *client, const char *msg) {
    evbuffer_add(client->output_buffer, msg, sizeof(msg));
    //flush buffer
    if (bufferevent_write_buffer(buf_event, client->output_buffer)) {
        errorOut("Error sending data to client on fd %d\n", client->fd);
        closeClient(client);
    }
}
/**
 * Called by libevent when there is data to read.
 */
void buffered_on_read(struct bufferevent *buf_event, void *arg)
{
    char *cmdline;
    char *temp;
    char *cmd;
    char *key;
    char *val;
    int processset = 0;
    struct evbuffer *buf;

    const char stored[8] = {'S','T','O','R','E','D',13,10};
    const char not_stored[12] = {'N','O','T','_','S','T','O','R','E','D',13,10}; 
    const char error[7] = {'E','R','R','O','R',13,10};
    
    buf = bufferevent_get_input(buf_event);
    client_t *client = (client_t *)arg;

    //check new line
    int cntnl = count_instances(buf,"\n");
    if (cntnl < 1) { 
        //no new line, wait next portion
        return;
    }
    //check set
    processset = 0;
    if (str_firstpos(buf,"set") == 0) {
        if (cntnl <=1) {
            //set without val - wait next portion
            return;
        }
        else{
            //set and more then 2 line
            processset = 1;
        }
    }

    cmdline = evbuffer_readln(buf, NULL, EVBUFFER_EOL_CRLF_STRICT);
    if(!cmdline) {
        return;
    }
    else {
        INFO_OUT("Read a line of length %zd from client on fd %d: %s\n", strlen(cmdline), client->fd, cmdline);
    }
    temp = strtok(cmdline, " ");
    if (!temp) {
        errorOut("Error: Empty command\n");
        return;
    }
    else{
        cmd = temp;
        INFO_OUT("command:%s\n",cmd);
    }
   
    INFO_OUT("processset:%d\n",processset);
    if (processset) {
        if ( (temp = strtok(NULL," ")) ) {
            key = temp;
            INFO_OUT("key:%s\n",key);
        }       
        int j = 0;
        size_t num = 0;
        while( (temp = strtok(NULL," ")) ) {
            j++;
            if (j == 2) {
                temp = strtok(NULL," ");
                num = atoi(temp);
                break;
            }
        }
        if (num<=0) {
            write_msg(buf_event,client,not_stored);
            return;
        }
        val = evbuffer_readln(buf, &num, EVBUFFER_EOL_CRLF_STRICT);
        if(!val) {
            // No data, or data has arrived, but no end-of-line was found
            write_msg(buf_event,client,not_stored);
            return;
        }
        INFO_OUT("Read a val line of length %zd from client on fd %d: %s\n", strlen(val), client->fd, val);
        write_msg(buf_event,client,stored);
        free(val);
        free(cmdline);
        return;
    }
    //process other one line command
    if (strcmp(cmd,"quit") == 0) {
        INFO_OUT("Close client on fd: %d\n", client->fd);
        closeClient(client);//?auto free cmdline on close?
        return;
    }
    if (strcmp(cmd,"get") == 0) {

    }
    else {
        write_msg(buf_event,client,error);
    }
    free(cmdline);
}


void buffered_on_read_new(struct bufferevent *bev, void *arg) {
    client_t *client = (client_t *)arg;
    char resp[8] = {'S','T','O','R','E','D',13,10};
    char *response;
    struct evbuffer *input;

    input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);  
    char data[len];
    evbuffer_remove(input, data, len);
    
    //response = handle_read(data,len);
    
    evbuffer_add(client->output_buffer, resp, sizeof(resp));
    //evbuffer_add(client->output_buffer, response, strlen(response));

    /* Send the results to the client.  This actually only queues the results
     * for sending. Sending will occur asynchronously, handled by libevent. */
    if (bufferevent_write_buffer(bev, client->output_buffer)) {
        errorOut("Error sending data to client on fd %d\n", client->fd);
        closeClient(client);
    }
}

/**
 * Called by libevent when there is data to read.
 */
void buffered_on_read_old(struct bufferevent *bev, void *arg) {
    client_t *client = (client_t *)arg;
    char data[4096];
    char resp[8] = {'S','T','O','R','E','D',13,10};    
    int nbytes;

    /* If we have input data, the number of bytes we have is contained in
     * bev->input->off. Copy the data from the input buffer to the output
     * buffer in 4096-byte chunks. There is a one-liner to do the whole thing
     * in one shot, but the purpose of this server is to show actual real-world
     * reading and writing of the input and output buffers, so we won't take
     * that shortcut here. */
    struct evbuffer *input;
    input = bufferevent_get_input(bev);
    while (evbuffer_get_length(input) > 0) {
        /* Remove a chunk of data from the input buffer, copying it into our
         * local array (data). */
        nbytes = evbuffer_remove(input, data, 4096); 
        //printf("data:%.*s\n",nbytes,data);
        //printf("resp:%.*s\n",8,resp);
        /* Add the chunk of data from our local array (data) to the client's
         * output buffer. */
        //evbuffer_add(client->output_buffer, data, nbytes);
        evbuffer_add(client->output_buffer, resp, 8);
    }

    /* Send the results to the client.  This actually only queues the results
     * for sending. Sending will occur asynchronously, handled by libevent. */
    if (bufferevent_write_buffer(bev, client->output_buffer)) {
        errorOut("Error sending data to client on fd %d\n", client->fd);
        closeClient(client);
    }
}

/**
 * Called by libevent when the write buffer reaches 0.  We only
 * provide this because libevent expects it, but we don't use it.
 */
void buffered_on_write(struct bufferevent *bev, void *arg) {
}

/**
 * Called by libevent when there is an error on the underlying socket
 * descriptor.
 */
void buffered_on_error(struct bufferevent *bev, short what, void *arg) {
    closeClient((client_t *)arg);
}

static void server_job_function(struct job *job) {
    client_t *client = (client_t *)job->user_data;

    event_base_dispatch(client->evbase);
    closeAndFreeClient(client);
    free(job);
}

/**
 * This function will be called by libevent when there is a connection
 * ready to be accepted.
 */
void on_accept(evutil_socket_t fd, short ev, void *arg) {
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    workqueue_t *workqueue = (workqueue_t *)arg;
    client_t *client;
    job_t *job;

    client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        warn("accept failed");
        return;
    }

    /* Set the client socket to non-blocking mode. */
    if (evutil_make_socket_nonblocking(client_fd) < 0) {
        warn("failed to set client socket to non-blocking");
        close(client_fd);
        return;
    }

    /* Create a client object. */
    if ((client = malloc(sizeof(*client))) == NULL) {
        warn("failed to allocate memory for client state");
        close(client_fd);
        return;
    }
    memset(client, 0, sizeof(*client));
    client->fd = client_fd;

    /* Add any custom code anywhere from here to the end of this function
     * to initialize your application-specific attributes in the client struct.
     */

    if ((client->output_buffer = evbuffer_new()) == NULL) {
        warn("client output buffer allocation failed");
        closeAndFreeClient(client);
        return;
    }

    if ((client->evbase = event_base_new()) == NULL) {
        warn("client event_base creation failed");
        closeAndFreeClient(client);
        return;
    }

    /* Create the buffered event.
     *
     * The first argument is the file descriptor that will trigger
     * the events, in this case the clients socket.
     *
     * The second argument is the callback that will be called
     * when data has been read from the socket and is available to
     * the application.
     *
     * The third argument is a callback to a function that will be
     * called when the write buffer has reached a low watermark.
     * That usually means that when the write buffer is 0 length,
     * this callback will be called.  It must be defined, but you
     * don't actually have to do anything in this callback.
     *
     * The fourth argument is a callback that will be called when
     * there is a socket error.  This is where you will detect
     * that the client disconnected or other socket errors.
     *
     * The fifth and final argument is to store an argument in
     * that will be passed to the callbacks.  We store the client
     * object here.
     */
    client->buf_ev = bufferevent_socket_new(client->evbase, client_fd,
                                            BEV_OPT_CLOSE_ON_FREE);
    if ((client->buf_ev) == NULL) {
        warn("client bufferevent creation failed");
        closeAndFreeClient(client);
        return;
    }
    bufferevent_setcb(client->buf_ev, buffered_on_read_new, buffered_on_write,
                      buffered_on_error, client);

    /* We have to enable it before our callbacks will be
     * called. */
    bufferevent_enable(client->buf_ev, EV_READ);

    /* Create a job object and add it to the work queue. */
    if ((job = malloc(sizeof(*job))) == NULL) {
        warn("failed to allocate memory for job state");
        closeAndFreeClient(client);
        return;
    }
    job->job_function = server_job_function;
    job->user_data = client;

    workqueue_add_job(workqueue, job);
}

/**
 * Run the server.  This function blocks, only returning when the server has 
 * terminated.
 */
int runServer(void) {
    evutil_socket_t listenfd;
    struct sockaddr_in listen_addr;
    struct event *ev_accept;
    int reuseaddr_on;

    /* Set signal handlers */
    sigset_t sigset;
    sigemptyset(&sigset);
    struct sigaction siginfo = {
        .sa_handler = sighandler,
        .sa_mask = sigset,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGINT, &siginfo, NULL);
    sigaction(SIGTERM, &siginfo, NULL);

    /* Create our listening socket. */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(1, "listen failed");
    }

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(SERVER_PORT);
    if (bind(listenfd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) 
        < 0) {
        err(1, "bind failed");
    }
    if (listen(listenfd, CONNECTION_BACKLOG) < 0) {
        err(1, "listen failed");
    }
    reuseaddr_on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on,
               sizeof(reuseaddr_on));

    /* Set the socket to non-blocking, this is essential in event
     * based programming with libevent. */
    if (evutil_make_socket_nonblocking(listenfd) < 0) {
        err(1, "failed to set server socket to non-blocking");
    }

    if ((evbase_accept = event_base_new()) == NULL) {
        perror("Unable to create socket accept event base");
        close(listenfd);
        return 1;
    }

    /* Initialize work queue. */
    if (workqueue_init(&workqueue, NUM_THREADS)) {
        perror("Failed to create work queue");
        close(listenfd);
        workqueue_shutdown(&workqueue);
        return 1;
    }

    /* We now have a listening socket, we create a read event to
     * be notified when a client connects. */
    ev_accept = event_new(evbase_accept, listenfd, EV_READ|EV_PERSIST,
                          on_accept, (void *)&workqueue);
    event_add(ev_accept, NULL);

    printf("Server running.\n");

    /* Start the event loop. */
    event_base_dispatch(evbase_accept);

    event_base_free(evbase_accept);
    evbase_accept = NULL;

    close(listenfd);

    printf("Server shutdown.\n");

    return 0;
}

/**
 * Kill the server.  This function can be called from another thread to kill
 * the server, causing runServer() to return.
 */
void killServer(void) {
    fprintf(stdout, "Stopping socket listener event loop.\n");
    if (event_base_loopexit(evbase_accept, NULL)) {
        perror("Error shutting down server");
    }
    fprintf(stdout, "Stopping workers.\n");
    workqueue_shutdown(&workqueue);
}

static void sighandler(int signal) {
    fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal,
            strsignal(signal));
    killServer();
}

/* Main function for demonstrating the echo server.
 * You can remove this and simply call runServer() from your application. */
//int main(int argc, char *argv[]) {
//    return runServer();
//}
