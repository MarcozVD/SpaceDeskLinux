/* Blocking TCP connection with a send lock (input and acks come from different threads). */
#ifndef SDV_NET_H
#define SDV_NET_H

#include <stddef.h>

#include <SDL.h>

struct sdv_conn {
    int fd;
    SDL_mutex *send_lock;
};

int sdv_conn_open(struct sdv_conn *c, const char *host, int port);
int sdv_conn_send(struct sdv_conn *c, const void *buf, size_t len);
/* Reads exactly len bytes. Returns 0 on success, -1 on error or EOF. */
int sdv_conn_recv(struct sdv_conn *c, void *buf, size_t len);
/* Unblocks a pending recv in another thread. */
void sdv_conn_shutdown(struct sdv_conn *c);
void sdv_conn_close(struct sdv_conn *c);

#endif
