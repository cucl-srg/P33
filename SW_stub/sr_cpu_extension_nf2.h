/*-----------------------------------------------------------------------------
 * file:  sr_cpu_extension_nf2.h
 * date:  Mon Feb 09 16:40:49 PST 2004
 * Author: Martin Casado <casado@stanford.edu>
 *
 * Description:
 *
 * Extensions to sr to operate with the NetFPGA in "cpu" mode.  That is,
 * provides the more complicated routing functionality such as ARP and
 * ICMP for a router built using the NetFPGA.
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_CPU_EXTENSIONS_H
#define SR_CPU_EXTENSIONS_H

#ifdef _CPUMODE_

#include "sr_base_internal.h"
#include "sr_interface.h"

static const uint16_t CPU_CONTROL_READ  = 0x8804;
static const uint16_t CPU_CONTROL_WRITE = 0x8805;
#define               CPU_CONTROL_ADDR    "0:20:ce:10:03"

/**
 * Initialize a socket to for communicating with the NetFPGA on the specified
 * interface.
 */
int sr_cpu_init_intf_socket( int interface_index );

/**
 * Listens for a packet to arrive and then passes it to the software router
 * using sr_integ_input.
 * @return 1 on success, otherwise 0
 */
int sr_cpu_input( struct sr_instance* sr );

/**
 * Writes buf of length len to the file descriptor intf->hw_fd.
 * @return 0 on success, otherwise -1
 */
int sr_cpu_output( uint8_t* buf, unsigned len, interface_t* intf );

#endif /* _CPUMODE_ */

#endif /* SR_CPU_EXTENSIONS_H */
