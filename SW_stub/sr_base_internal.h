/*--------------------------------------------------------------------------
 * file:  sr_base_internal.h
 * date:  Tue Feb 03 11:18:32 PST 2004
 * Author: Martin Casado
 * Modified for Mininet compatibility,
 * August 2013, Matthew Ireland <mti20@cam.ac.uk>
 *
 * Description:
 *
 * House all the definitions for the basic core router definitions.  This
 * should not be included by any "user level" files such as main or
 * network applications that run on the router.  Any low level code
 * (which would normally be kernel level) will require these definitions).
 *
 * Low level network code should use the functios:
 *
 * sr_get_global_instance(..) - to gain a pointer to the global sr context
 *
 *  and
 *
 * sr_get_subsystem(..)       - to get the router subsystem from the context
 *
 *-------------------------------------------------------------------------*/

#ifndef SR_BASE_INTERNAL_H
#define SR_BASE_INTERNAL_H

#ifdef _LINUX_
#include <stdint.h>
#endif /* _LINUX_ */

#ifdef _DARWIN_
#include <inttypes.h>
#endif /* _DARWIN_ */

#include <stdio.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_router.h"

#define SR_NAMELEN 32

#define CPU_HW_FILENAME "cpuhw"

#ifndef _MANUAL_MODE_
int main(int argc, char** argv);
#else
int original_main(int argc, char** argv);
#endif

/* -- gcc specific vararg macro support ... but its so nice! -- */
#ifdef _DEBUG_
#define Debug(x, args...) fprintf(stderr,x, ## args)
#define DebugIP(x) \
  do { struct in_addr addr; addr.s_addr = x; fprintf(stderr,"%s",inet_ntoa(addr));\
     } while(0)
#define DebugMAC(x) \
  do { int ivyl; for(ivyl=0; ivyl<5; ivyl++) fprintf(stderr,"%02x:", \
  (unsigned char)(x[ivyl])); fprintf(stderr,"%02x",(unsigned char)(x[5])); } while (0)
#else
#define Debug(x, args...) do{}while(0)
#define DebugMAC(x) \
  do { int ivyl; for(ivyl=0; ivyl<5; ivyl++) fprintf(stderr,"%02x:", \
  (unsigned char)(x[ivyl])); fprintf(stderr,"%02x",(unsigned char)(x[5])); } while (0)
#endif


/* ----------------------------------------------------------------------------
 * struct sr_instance
 *
 * Encapsulation of the state for a single virtual router.
 *
 * -------------------------------------------------------------------------- */

struct sr_instance
{
  int  sockfd;    /* socket to server */
  char user[32];  /* user name */
  char vhost[32]; /* host name */
  char lhost[32]; /* host name of machine running client */
  char rtable[32];/* filename for routing table          */
  char server[32];
  unsigned short topo_id; /* topology id */
  struct sockaddr_in sr_addr; /* address to server */
  FILE* logfile; /* file to log all received/sent packets to */
  volatile uint8_t  hw_init; /* bool : hardware has been initialized */
  pthread_mutex_t   send_lock; /* experimental */
  
  router_t* interface_subsystem; /* subsystem to send/recv packets from */
  
  #ifdef MININET_MODE
  char* router_name;   /* router name, needed for interface initialisation */
  #endif /* MININET_MODE */

};

/* ----------------------------------------------------------------------------
 * See method definitions in sr_base.c for detailed explanation of the
 * following two methods.
 * -------------------------------------------------------------------------*/

router_t* sr_get_subsystem(struct sr_instance* sr);
void  sr_set_subsystem(struct sr_instance* sr, router_t* core);
struct sr_instance* sr_get_global_instance(struct sr_instance* sr);


/* ----------------------------------------------------------------------------
 * Integration methods for calling subsystem (e.g. the router).  These
 * may be replaced by callback functions that get registered with
 * sr_instance.
 * -------------------------------------------------------------------------*/

void sr_integ_init(struct sr_instance* );
void sr_integ_hw_setup(struct sr_instance* ); /* called after hwinfo */
void sr_integ_destroy(struct sr_instance* );
void sr_integ_input(struct sr_instance* sr,
                   const uint8_t * packet/* borrowed */,
                   unsigned int len,
#if defined _CPUMODE_ || defined MININET_MODE
                    interface_t* intf );
#else
                    const char* interface );
#endif

int sr_integ_output(struct sr_instance* sr /* borrowed */,
                    uint8_t* buf /* borrowed */ ,
                    unsigned int len,
                    const char* iface /* borrowed */);

uint32_t sr_findsrcip(uint32_t dest /* nbo */);
uint32_t sr_integ_ip_output(uint8_t* payload /* given */,
                            uint8_t  proto,
                            uint32_t src, /* nbo */
                            uint32_t dest, /* nbo */
                            int len);
uint32_t sr_integ_findsrcip(uint32_t dest /* nbo */);


#endif  /* -- SR_BASE_INTERNAL_H -- */
