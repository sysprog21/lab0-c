/* Copyright (c) 2020 Thomas Brand <tom@trellis.ch>
 * MIT License.
 */

#include <arpa/inet.h> /* inet_ntoa */
#include <errno.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LISTENQ 1024 /* second argument to listen() */
#define MAXLINE 1024 /* max length of a line */
#define BUFSIZE 1024

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 9999 /* use this port if none given as arg to main() */
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#define TCP_CORK TCP_NOPUSH
#endif

static int server_fd;

typedef struct {
    int fd;            /* descriptor for this buf */
    int count;         /* unread byte in this buf */
    char *bufptr;      /* next unread byte in this buf */
    char buf[BUFSIZE]; /* internal buffer */
} rio_t;

typedef struct {
    char filename[512];
    off_t offset; /* for support Range */
    size_t end;
} http_request_t;

static void rio_readinitb(rio_t *rp, int fd)
{
    rp->fd = fd;
    rp->count = 0;
    rp->bufptr = rp->buf;
}

/* This is a wrapper for the Unix read() function that transfers min(n, count)
 * bytes from an internal buffer to a user buffer, where n is the number of
 * bytes requested by the user and count is the number of unread bytes in the
 * internal buffer. On entry, rio_read() refills the internal buffer via a call
 * to read() if the internal buffer is empty.
 */
/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;
    while (rp->count <= 0) { /* refill if buf is empty */
        rp->count = read(rp->fd, rp->buf, sizeof(rp->buf));
        if (rp->count < 0) {
            if (errno != EINTR) /* interrupted by sig handler return */
                return -1;
        } else if (rp->count == 0) { /* EOF */
            return 0;
        } else
            rp->bufptr = rp->buf; /* reset buffer ptr */
    }

    /* Copy min(n, rp->count) bytes from internal buf to user buf */
    cnt = n;
    if (rp->count < n)
        cnt = rp->count;
    memcpy(usrbuf, rp->bufptr, cnt);
    rp->bufptr += cnt;
    rp->count -= cnt;
    return cnt;
}

static ssize_t writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    char *bufp = usrbuf;

    while (nleft > 0) {
        ssize_t nwritten = write(fd, bufp, nleft);
        if (nwritten <= 0) {
            if (errno == EINTR) { /* interrupted by sig handler return */
                nwritten = 0;     /* and call write() again */
            } else
                return -1; /* errorno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}

/* robustly read a text line (buffered) */
static ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    char c, *bufp = usrbuf;

    int n;
    for (n = 1; n < maxlen; n++) {
        int rc;
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (n == 1)
                return 0; /* EOF, no data read */
            break;        /* EOF, some data was read */
        } else
            return -1; /* error */
    }
    *bufp = 0;
    return n;
}

void web_send(int out_fd, char *buf)
{
    writen(out_fd, buf, strlen(buf));
}

int web_open(int port)
{
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval,
                   sizeof(int)) < 0)
        return -1;

    // 6 is TCP's protocol number
    // enable this, much faster : 4000 req/s -> 17000 req/s
    if (setsockopt(listenfd, IPPROTO_TCP, TCP_CORK, (const void *) &optval,
                   sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) port);
    if (bind(listenfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;

    server_fd = listenfd;

    return listenfd;
}

static void url_decode(char *src, char *dest, int max)
{
    char *p = src;
    char code[3] = {0};
    while (*p && --max) {
        if (*p == '%') {
            memcpy(code, ++p, 2);
            *dest++ = (char) strtoul(code, NULL, 16);
            p += 2;
        } else {
            *dest++ = *p++;
        }
    }
    *dest = '\0';
}

static void parse_request(int fd, http_request_t *req)
{
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE];
    req->offset = 0;
    req->end = 0; /* default */

    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%1023s %1023s", method, uri); /* version is not cared */
    /* read all */
    while (buf[0] != '\n' && buf[1] != '\n') { /* \n || \r\n */
        rio_readlineb(&rio, buf, MAXLINE);
        if (buf[0] == 'R' && buf[1] == 'a' && buf[2] == 'n') {
            sscanf(buf, "Range: bytes=%lu-%lu", (unsigned long *) &req->offset,
                   (unsigned long *) &req->end);
            /* Range: [start, end] */
            if (req->end != 0)
                req->end++;
        }
    }
    char *filename = uri;
    if (uri[0] == '/') {
        filename = uri + 1;
        int length = strlen(filename);
        if (length == 0) {
            filename = ".";
        } else {
            for (int i = 0; i < length; ++i) {
                if (filename[i] == '?') {
                    filename[i] = '\0';
                    break;
                }
            }
        }
    }
    url_decode(filename, req->filename, MAXLINE);
}

char *web_recv(int fd, struct sockaddr_in *clientaddr)
{
    http_request_t req;
    parse_request(fd, &req);

    char *p = req.filename;
    /* Change '/' to ' ' */
    while (*p) {
        ++p;
        if (*p == '/')
            *p = ' ';
    }
    char *ret = malloc(strlen(req.filename) + 1);
    strncpy(ret, req.filename, strlen(req.filename) + 1);

    return ret;
}

int web_eventmux(char *buf)
{
    fd_set listenset;

    FD_ZERO(&listenset);
    FD_SET(STDIN_FILENO, &listenset);
    int max_fd = STDIN_FILENO;
    if (server_fd > 0) {
        FD_SET(server_fd, &listenset);
        max_fd = max_fd > server_fd ? max_fd : server_fd;
    }
    int result = select(max_fd + 1, &listenset, NULL, NULL, NULL);
    if (result < 0)
        return -1;

    if (server_fd > 0 && FD_ISSET(server_fd, &listenset)) {
        FD_CLR(server_fd, &listenset);
        struct sockaddr_in clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int web_connfd =
            accept(server_fd, (struct sockaddr *) &clientaddr, &clientlen);

        char *p = web_recv(web_connfd, &clientaddr);
        char *buffer = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
        web_send(web_connfd, buffer);
        strncpy(buf, p, strlen(p) + 1);
        free(p);
        close(web_connfd);
        return strlen(buf);
    }

    FD_CLR(STDIN_FILENO, &listenset);
    return 0;
}
