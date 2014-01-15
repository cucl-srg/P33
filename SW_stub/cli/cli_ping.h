/*
 * Filename: cli_ping.h
 * Purpose: implement a ping handler
 */

#ifndef CLI_PING_H
#define CLI_PING_H

#include "../sr_common.h"
#include "../sr_router.h"

/** ID field sent with outgoing pings */
#define PING_ID 3

/** maximum time the ping handler will block without considering new pings */
#define PING_HANDLER_MIN_RESPONSE_TIME_USEC 100000 /* 100ms */

/** default maximum time to wait for an echo reply */
#define PING_MAX_WAIT_FOR_REPLY_USEC 1000000 /* 1000ms */

/** Initialize the CLI ping handler */
void cli_ping_init();

/** Destroy the CLI ping handler */
void cli_ping_destroy();

/**
 * Send a ping request to IP and let fd know about the result.
 */
void cli_ping_request( router_t* rtr, int fd, addr_ip_t ip );

/** Handles an ICMP Echo Reply from ip with sequence number seq. */
void cli_ping_handle_reply( addr_ip_t src_ip, uint16_t seq );

#endif /* CLI_PING_H */
