/* LAN discovery of spacedesk servers (UDP broadcast "SPACEDESK-NET-CLIENT" on port 28252). */
#ifndef SDV_DISCOVERY_H
#define SDV_DISCOVERY_H

#include <stdint.h>

#define SDV_DISCOVERY_MAX 16

struct sdv_server_info {
    char address[64];
    char name[128];
    int port;
    uint32_t last_seen_ms;
};

struct sdv_discovery;

/* extra_hosts: optional NULL-terminated list of addresses probed by unicast (recent servers). */
struct sdv_discovery *sdv_discovery_start(const char *const *extra_hosts);
void sdv_discovery_stop(struct sdv_discovery *d);
/* Copies the servers seen recently; returns how many. */
int sdv_discovery_list(struct sdv_discovery *d, struct sdv_server_info *out, int max);

#endif
