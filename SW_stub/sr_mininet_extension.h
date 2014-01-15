/*-----------------------------------------------------------------------------
 * File:  sr_mininet_extension.h
 * Date:  Mon Aug 11 13:50:00 BST 2013
 * Author: Matthew Ireland <mti20@cam.ac.uk>
 *
 * Description: Functions for reading and writing to raw sockets in mininet.
 *
 *---------------------------------------------------------------------------*/

#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "debug.h"
#include "real_socket_helper.h"
#include "sr_base_internal.h"
#include "sr_common.h"
#include "sr_dumper.h"

#ifdef MININET_MODE
#ifndef SR_MININET_EXTENSION_H_
#define SR_MININET_EXTENSION_H_

int sr_mininet_init_intf_socket( char* router_name /* name of router, e.g. r0 */, 
				int interface_index /* index of interface e.g. 0, 1, 2, 3 */ );

int sr_mininet_read_packet( struct sr_instance* sr );

int sr_mininet_output( uint8_t* buf, unsigned len, interface_t* intf );

#endif /* SR_MININET_EXTENSION_H_ */
#endif /* MININET_MODE */
