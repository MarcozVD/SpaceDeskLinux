#include "discovery.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <SDL.h>

#include "protocol.h"

#define PROBE_INTERVAL_MS 2500
#define FORGET_AFTER_MS 10000
#define MAX_EXTRA 8

struct sdv_discovery {
    int fd;
    SDL_Thread *thread;
    SDL_atomic_t running;
    SDL_mutex *lock;
    struct sdv_server_info servers[SDV_DISCOVERY_MAX];
    int count;
    char extra[MAX_EXTRA][128];
    int nextra;
};

static void send_to(int fd, const struct sockaddr_in *dst)
{
    static const char req[] = SD_DISCOVERY_REQUEST; /* sent with its terminating NUL */
    sendto(fd, req, sizeof(req), 0, (const struct sockaddr *)dst, sizeof(*dst));
}

static void probe(struct sdv_discovery *d)
{
    struct ifaddrs *ifs, *it;
    struct sockaddr_in dst = { .sin_family = AF_INET, .sin_port = htons(SD_PORT) };

    dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    send_to(d->fd, &dst);
    if (getifaddrs(&ifs) == 0) {
        for (it = ifs; it; it = it->ifa_next) {
            if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET || !(it->ifa_flags & IFF_UP) ||
                (it->ifa_flags & IFF_LOOPBACK) || !(it->ifa_flags & IFF_BROADCAST) || !it->ifa_broadaddr)
                continue;
            dst.sin_addr = ((struct sockaddr_in *)it->ifa_broadaddr)->sin_addr;
            send_to(d->fd, &dst);
        }
        freeifaddrs(ifs);
    }
    for (int i = 0; i < d->nextra; i++) {
        struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM }, *res;
        if (getaddrinfo(d->extra[i], NULL, &hints, &res) == 0) {
            dst.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
            send_to(d->fd, &dst);
            freeaddrinfo(res);
        }
    }
}

static void record(struct sdv_discovery *d, const struct sockaddr_in *from, const struct sd_discovery_reply *r)
{
    char ip[INET_ADDRSTRLEN];
    int slot = -1;
    Uint32 now = SDL_GetTicks();

    inet_ntop(AF_INET, &from->sin_addr, ip, sizeof(ip));
    SDL_LockMutex(d->lock);
    for (int i = 0; i < d->count; i++)
        if (!strcmp(d->servers[i].address, ip))
            slot = i;
    if (slot < 0 && d->count < SDV_DISCOVERY_MAX)
        slot = d->count++;
    if (slot >= 0) {
        struct sdv_server_info *s = &d->servers[slot];
        snprintf(s->address, sizeof(s->address), "%s", ip);
        snprintf(s->name, sizeof(s->name), "%s", r->name[0] ? r->name : ip);
        s->port = (int)r->port;
        s->last_seen_ms = now;
    }
    SDL_UnlockMutex(d->lock);
}

static int discovery_thread(void *arg)
{
    struct sdv_discovery *d = arg;
    Uint32 next_probe = 0;
    uint8_t buf[1024];

    while (SDL_AtomicGet(&d->running)) {
        struct pollfd pfd = { .fd = d->fd, .events = POLLIN };
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        struct sd_discovery_reply r;
        ssize_t n;

        if (SDL_TICKS_PASSED(SDL_GetTicks(), next_probe)) {
            probe(d);
            next_probe = SDL_GetTicks() + PROBE_INTERVAL_MS;
        }
        if (poll(&pfd, 1, 200) <= 0)
            continue;
        n = recvfrom(d->fd, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fl);
        if (n > 0 && sd_parse_discovery_reply(buf, (size_t)n, &r) == 0)
            record(d, &from, &r);
    }
    return 0;
}

struct sdv_discovery *sdv_discovery_start(const char *const *extra_hosts)
{
    struct sdv_discovery *d = calloc(1, sizeof(*d));
    int one = 1;
    struct sockaddr_in any = { .sin_family = AF_INET };

    if (!d)
        return NULL;
    d->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (d->fd < 0 || setsockopt(d->fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0 ||
        bind(d->fd, (struct sockaddr *)&any, sizeof(any)) < 0) {
        if (d->fd >= 0)
            close(d->fd);
        free(d);
        return NULL;
    }
    for (int i = 0; extra_hosts && extra_hosts[i] && d->nextra < MAX_EXTRA; i++)
        if (extra_hosts[i][0])
            snprintf(d->extra[d->nextra++], sizeof(d->extra[0]), "%s", extra_hosts[i]);
    d->lock = SDL_CreateMutex();
    SDL_AtomicSet(&d->running, 1);
    d->thread = d->lock ? SDL_CreateThread(discovery_thread, "sdv-discovery", d) : NULL;
    if (!d->thread) {
        if (d->lock)
            SDL_DestroyMutex(d->lock);
        close(d->fd);
        free(d);
        return NULL;
    }
    return d;
}

void sdv_discovery_stop(struct sdv_discovery *d)
{
    if (!d)
        return;
    SDL_AtomicSet(&d->running, 0);
    SDL_WaitThread(d->thread, NULL);
    close(d->fd);
    SDL_DestroyMutex(d->lock);
    free(d);
}

int sdv_discovery_list(struct sdv_discovery *d, struct sdv_server_info *out, int max)
{
    int n = 0;
    Uint32 now = SDL_GetTicks();

    SDL_LockMutex(d->lock);
    for (int i = 0; i < d->count && n < max; i++)
        if (now - d->servers[i].last_seen_ms < FORGET_AFTER_MS)
            out[n++] = d->servers[i];
    SDL_UnlockMutex(d->lock);
    return n;
}
