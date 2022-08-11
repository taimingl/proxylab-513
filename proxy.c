/*
 * Starter code for proxy lab.
 * Feel free to modify this code in whatever way you wish.
 */

/* Some useful includes to help you get started */

#include "csapp.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Debug macros, which can be enabled by adding -DDEBUG in the Makefile
 * Use these if you find them useful, or delete them if not
 */
#ifdef DEBUG
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_assert(...)
#define dbg_printf(...)
#endif

/*
 * Max cache and object sizes
 * You might want to move these to the file containing your cache implementation
 */
#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)

#define HOSTLEN 256
#define SERVLEN 8

/*Typedef for convenience. */
typedef struct sockaddr SA;

/* Information about a connected client. */
typedef struct {
    struct sockaddr_in addr; // Socket address
    socklen_t addrlen;       // Socket address length
    int connfd;              // Client connection file descriptor
    char host[HOSTLEN];      // Client host
    char serv[SERVLEN];      // Client service (port)
} client_info;

/*
 * String to use for the User-Agent header.
 * Don't forget to terminate with \r\n
 */
static const char *header_user_agent = "User-Agent: Mozilla/5.0"
                                       " (X11; Linux x86_64; rv:3.10.0)"
                                       " Gecko/20220411 Firefox/63.0.1\r\n";
static const char *header_connection = "Connection: close\r\n";
static const char *proxy_header_connection = "Proxy-Connection: close\r\n";

/**
 * clienterror - returns an error message to the client
 *
 */
void clienterror(int fd, const char *errnum, const char *shortmsg,
                 const char *longmsg) {
    char buf[MAXLINE];
    char body[MAXBUF];
    size_t buflen;
    size_t bodylen;

    /* Build the HTTP response body */
    bodylen = snprintf(body, MAXBUF,
                       "<!DOCTYPE html>\r\n"
                       "<html>\r\n"
                       "<head><title>Proxy Error</title></head>\r\n"
                       "<body bgcolor=\"ffffff\">\r\n"
                       "<h1>%s: %s</h1>\r\n"
                       "<p>%s</p>\r\n"
                       "<hr /><em>The Proxy Web server</em>\r\n"
                       "</body></html>\r\n",
                       errnum, shortmsg, longmsg);
    if (bodylen >= MAXBUF) {
        return; // Overflow!
    }

    /* Build the HTTP response headers */
    buflen = snprintf(buf, MAXLINE,
                      "HTTP/1.0 %s %s\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: %zu\r\n\r\n",
                      errnum, shortmsg, bodylen);
    if (buflen >= MAXLINE) {
        return; // Overflow!
    }

    /* Write the headers */
    if (rio_writen(fd, buf, buflen) < 0) {
        fprintf(stderr, "Error writing error response headers to client\n");
        return;
    }

    /* Write the body */
    if (rio_writen(fd, body, bodylen) < 0) {
        fprintf(stderr, "Error writing error response body to client\n");
        return;
    }
}

/**
 * parse_uri parse URI to make sure valid request
 *
 */

int parse_uri(char *uri, char *host, char *path) {
    // /* Asuume URI starts with / */
    // if (uri[0] != '/') {
    //     return 1;
    // }

    /* Make a valiant effort to prevent directory traversal attacks */
    if (strstr(uri, "/../") != NULL) {
        return 1;
    }

    char protocol[MAXLINE];
    strcpy(path, "/");
    if (strstr(uri, "://") != NULL) {
        sscanf(uri, "%[^:]://%[^/]%s", protocol, host, path);
        if (strcasecmp(protocol, "http")) {
            printf("Proxy does not support protocol: %s\n", protocol);
            return 1;
        } else {
            return 0;
        }
    } else {
        sscanf(uri, "%[^/]%s", host, path);
        return 0;
    }
}

/**
 * parse_port: prases hostname and port number of actual web server.
 *
 * If no port number provided, 80 will be used as default.
 */
void parse_port(char *host, char *hostname, char *port) {
    strcpy(hostname, host);
    char *pos = NULL;
    pos = strchr(hostname, ':');
    if (pos != NULL) {
        strcpy(port, pos + 1);
        *pos = '\0';
    } else {
        strcat(host, ":80");
        strcpy(port, "80");
    }
}

/**
 * build_requesthdrs - read from client request header and
 * builds a new header for proxy.
 *
 * Returns true if an error occured, or false otherwise.
 *
 */
bool build_requesthdrs(client_info *client, rio_t *rp, char *proxy_request,
                       char *method, char *path, char *host) {
    char buf[MAXLINE];
    char name[MAXLINE];
    char value[MAXLINE];
    memset(buf, 0, MAXLINE * sizeof(char));
    memset(name, 0, MAXLINE * sizeof(char));
    memset(value, 0, MAXLINE * sizeof(char));

    /* Write the uniform name:value first */
    strcat(proxy_request, method); // Req Method
    strcat(proxy_request, " ");
    strcat(proxy_request, path);
    strcat(proxy_request, " HTTP/1.0\r\n");
    strcat(proxy_request, "Host: "); // Req Host
    strcat(proxy_request, host);
    strcat(proxy_request, "\r\n");
    strcat(proxy_request, header_connection); // Req header connection
    strcat(proxy_request,
           proxy_header_connection);          // Req header proxy connection
    strcat(proxy_request, header_user_agent); // Req user agent

    while (true) {
        if (rio_readlineb(rp, buf, sizeof(buf)) <= 0) {
            return true;
        }

        /* Check for end of request headers */
        if (strcmp(buf, "\r\n") == 0) {
            strcat(proxy_request, "\r\n");
            return false;
        }

        /* Parse header into name and value */
        if (sscanf(buf, "%[^:]: %[^\r\n]", name, value) != 2) {
            /* Error parsing header */
            clienterror(client->connfd, "400", "Bad Request",
                        "Proxy could not parse request headers");
            return true;
        }

        /* Convert name to lowercase */
        for (size_t i = 0; name[i] != '\0'; i++) {
            name[i] = tolower(name[i]);
        }
        /* Skip if already added */
        if (strcmp(name, "host") == 0 || strcmp(name, "connection") == 0 ||
            strcmp(name, "user-agent") == 0) {
            continue;
        }

        strcat(proxy_request, buf);

        // /* Convert name to lowercase */
        // for (size_t i = 0; name[i] != '\0'; i++) {
        //     name[i] = tolower(name[i]);
        // }

        // printf("%s: %s\n", name, value);
    }
}

/**
 * do_proxy - fetch from real web server and respond to client.
 *
 * Forwards requests from clients to web servers and forwards responses
 * from webservers back to clients
 *
 */
void do_proxy(client_info *client, char *proxy_request, char *srv_hostname,
              char *srv_port) {
    int proxy_clientfd;
    char srv_buf[MAXLINE];
    rio_t srv_rio;
    memset(srv_buf, 0, MAXLINE * sizeof(char));

    proxy_clientfd = open_clientfd(srv_hostname, srv_port);
    if (proxy_clientfd < 0) {
        fprintf(stderr, "Failed to connect to web server: %s:%s\n",
                srv_hostname, srv_port);
        return;
    }

    // rio_readinitb(&srv_rio, proxy_clientfd);
    if (rio_writen(proxy_clientfd, proxy_request, strlen(proxy_request)) < 0) {
        fprintf(stderr, "Error: writing to web server error\n");
        return;
    }

    rio_readinitb(&srv_rio, proxy_clientfd);
    int size = 0;
    while ((size = rio_readlineb(&srv_rio, srv_buf, MAXLINE)) > 0) {
        rio_writen(client->connfd, srv_buf, size);
        // printf("%s\n", srv_buf);
    }
    close(proxy_clientfd);
}

/**
 * serve - handles one HTTP request/response transaction
 *
 */
void serve(client_info *client) {
    // Get some extra info about the client (hostname/port)
    // This is optional, but it's nice to know who's connected
    int res = getnameinfo((SA *)&client->addr, client->addrlen, client->host,
                          sizeof(client->host), client->serv,
                          sizeof(client->serv), 0);
    if (res == 0) {
        printf("Accepted connection from %s:%s\n", client->host, client->serv);
    } else {
        fprintf(stderr, "getnameinfo failed: %s\n", gai_strerror(res));
    }

    rio_t rio;
    // Associate a descriptor with a read buffer and reset buffer
    rio_readinitb(&rio, client->connfd);

    /* Read request line */
    char buf[MAXLINE];
    // Robustly read a text line (buffered)
    if (rio_readlineb(&rio, buf, sizeof(buf)) <= 0) {
        return;
    }

    printf("%s\n", buf);

    /* parse the request line and check if it's well-formed */
    char method[MAXLINE];
    char uri[MAXLINE];
    char version;

    /* sscanf must parse exactly 3 things for request line to be well-formed */
    /* version must be either HTTP/1.0 or HTTP/1.1 */
    if (sscanf(buf, "%s %s HTTP/1.%c", method, uri, &version) != 3 ||
        (version != '0' && version != '1')) {
        clienterror(client->connfd, "400", "Bad Request",
                    "Proxy received a malformed request");
        return;
    }

    /* Check that method is GET */
    if (strcmp(method, "GET") != 0) {
        clienterror(client->connfd, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }

    /* Parse URI from GET request */
    char host[MAXLINE];
    char path[MAXLINE];
    memset(host, 0, MAXLINE * sizeof(char));
    memset(path, 0, MAXLINE * sizeof(char));
    if (parse_uri(uri, host, path)) {
        printf("Failed to parse URI.\n");
        return;
    }

    /* Parse server hostname and port */
    char srv_hostname[MAXLINE];
    char srv_port[MAXLINE];
    memset(srv_hostname, 0, MAXLINE * sizeof(char));
    memset(srv_port, 0, MAXLINE * sizeof(char));
    parse_port(host, srv_hostname, srv_port);

    /* Make proxy_req for proxy */
    char proxy_request[MAXLINE];
    memset(proxy_request, 0, MAXLINE * sizeof(char));
    if (build_requesthdrs(client, &rio, proxy_request, method, path, host)) {
        return;
    }

    /* finally, proxy the request for client */
    do_proxy(client, proxy_request, srv_hostname, srv_port);
}

/* Thread routine */
void *thread(void *vargp) {
    client_info client_data = *((client_info *)vargp);
    client_info *client = &client_data;
    pthread_detach(pthread_self());
    free((client_info *)vargp);
    serve(client);
    close(client->connfd);
    return NULL;
}

int main(int argc, char **argv) {
    int listenfd;
    pthread_t tid;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Signal(SIGPIPE, SIG_IGN);

    listenfd = open_listenfd(argv[1]);
    if (listenfd < 0) {
        fprintf(stderr, "Failed to listen on port: %s\n", argv[1]);
    } else {
        printf("Proxy starts to listen on port: %s\n", argv[1]);
    }

    while (1) {
        /* Allocate space on the stack for client info */
        // client_info client_data;
        // client_info *client = &client_data;

        client_info *client = malloc(sizeof(client_info));

        /* Initialize the length of the address */
        client->addrlen = sizeof(client->addr);

        // &client->connfd = malloc(sizeof(int));

        /* accept() will block until a client connects to the port */
        client->connfd =
            accept(listenfd, (SA *)&client->addr, &client->addrlen);
        if (client->connfd < 0) {
            perror("accept");
            continue;
        }

        /* Connection is established; serve client */
        // serve(client);
        // close(client->connfd);

        /* Spawn new thread to handle client */
        if (pthread_create(&tid, NULL, thread, (void *)client) != 0) {
            perror("Error creating thread");
        }
    }

    return -1; // never reaches here
}
