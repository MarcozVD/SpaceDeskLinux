#include "net.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int sdv_conn_open(struct sdv_conn *c, const char *host, int port)
{
    struct addrinfo hints = { 0 }, *res, *ai;
    char portstr[16];
    int fd = -1, one = 1, rcvbuf = 4 * 1024 * 1024, rc;

    c->fd = -1;
    c->send_lock = SDL_CreateMutex();
    if (!c->send_lock)
        return -1;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%d", port);
    rc = getaddrinfo(host, portstr, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "net: %s: %s\n", host, gai_strerror(rc));
        return -1;
    }
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) {
        fprintf(stderr, "net: cannot connect to %s:%d: %s\n", host, port, strerror(errno));
        return -1;
    }
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    c->fd = fd;
    return 0;
}

int sdv_conn_send(struct sdv_conn *c, const void *buf, size_t len)
{
    const char *p = buf;
    int ret = 0;

    if (c->fd < 0)
        return -1;
    SDL_LockMutex(c->send_lock);
    while (len > 0) {
        ssize_t n = send(c->fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            ret = -1;
            break;
        }
        p += n;
        len -= (size_t)n;
    }
    SDL_UnlockMutex(c->send_lock);
    return ret;
}

int sdv_conn_recv(struct sdv_conn *c, void *buf, size_t len)
{
    char *p = buf;

    while (len > 0) {
        ssize_t n = recv(c->fd, p, len, 0);
        if (n == 0)
            return -1;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

void sdv_conn_shutdown(struct sdv_conn *c)
{
    if (c->fd >= 0)
        shutdown(c->fd, SHUT_RDWR);
}

void sdv_conn_close(struct sdv_conn *c)
{
    if (c->fd >= 0)
        close(c->fd);
    c->fd = -1;
    if (c->send_lock)
        SDL_DestroyMutex(c->send_lock);
    c->send_lock = NULL;
}
