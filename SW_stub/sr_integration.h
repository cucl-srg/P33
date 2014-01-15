/*
 * Filename: sr_integration.h
 * Purpose: Interface to low-level router functionality.
 */

#ifndef SR_INTEGRATION_H
#define SR_INTEGRATION_H

#include "sr_interface.h"

/** returns a pointer to the global sr (only valid after it is initialized) */
struct sr_instance* get_sr();

/** returns a pointer to the global router subsystem */
router_t* get_router();

int sr_integ_low_level_output( struct sr_instance* sr /* borrowed */,
                               uint8_t* buf /* borrowed */ ,
                               unsigned int len,
                               interface_t* intf );

/** returns the ip of the interface this will be sent via */
uint32_t sr_integ_findsrcip(uint32_t dest /* nbo */);

#endif /* INTEGRATION_H */
