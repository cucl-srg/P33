/* Filename: sr_cpu_extension_nf.c */

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
#include "sr_cpu_extension_nf2.h"
#include "sr_dumper.h"

#define DECAP_NEXT_INTF 1 /* nf2c1 because it has the rate limiter */

#ifdef _CPUMODE_
int sr_cpu_init_intf_socket( int interface_index ) {
    true_or_die( interface_index>=0 && interface_index<=3,
                 "Error: unexpected interface_index: %u", interface_index );

    char iface_name[32];
    sprintf( iface_name, "nf%u", interface_index );
    int s = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL) );
    if( s == -1 ) {
        perror( "Socket creation failed" );
        die( "Error: socket creation failed" );
    }
    else
        debug_println( "got socket to intf %d", s );


    struct ifreq ifr;
    bzero( &ifr, sizeof(struct ifreq) );
    strncpy( ifr.ifr_ifrn.ifrn_name, iface_name, IFNAMSIZ );
    //strncpy( ifr.ifr_ifrn.ifrn_name, "nf0", IFNAMSIZ );
    if( ioctl(s, SIOCGIFINDEX, &ifr) < 0 ) {
        perror("ioctl SIOCGIFINDEX");
        die( "Error: ioctl SIOCGIFINDEX failed" );
    }

    struct sockaddr_ll saddr;
    bzero( &saddr, sizeof(struct sockaddr_ll) );
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = ifr.ifr_ifru.ifru_ivalue;
    if (bind(s, (struct sockaddr*)(&saddr), sizeof(saddr)) < 0) {
        perror( "bind error" );
        die( "Error: bind failed" );
    }

    debug_println( "Connected to hardware interface %s", iface_name );

    return s;
}
static uint64_t count=0;
int sr_cpu_input( struct sr_instance* sr ) {
    byte buf[ETH_MAX_LEN];
    ssize_t len;
    router_t* router;
    interface_t* intf;
    fd_set rdset, errset;
    int ret, max_fd;
    unsigned i;
    struct timeval timeout;

    /* loop until something interesting happens */
    do {
        /* clear the sets */
        FD_ZERO( &rdset );
        FD_ZERO( &errset );

        /* set the bits on interfaces' fd's that we care about */
        max_fd = -1;
        router = sr->interface_subsystem;
        for( i=0; i<router->num_interfaces; i++ ) {
            if( router->interface[i].enabled ) {
                FD_SET( router->interface[i].hw_fd, &rdset );
                FD_SET( router->interface[i].hw_fd, &errset );

                if( router->interface[i].hw_fd > max_fd )
                    max_fd = router->interface[i].hw_fd;

#ifdef _DEBUG_
                if( router->interface[i].hw_fd > FD_SETSIZE )
                    die( "Error: fd too big for select (%d > %d)",
                         router->interface[i].hw_fd, FD_SETSIZE );
#endif
            }
        }

        /* wait for something to happen */
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;
        ret = select( max_fd + 1, &rdset, NULL, &errset, &timeout );

        /* check each intf to see if something interesting happened on it */
        for( i=0; i<router->num_interfaces; i++ ) {
            intf = &router->interface[i];
            if( intf->enabled ) {
                /* check for available bytes in the input buffer */
                if( FD_ISSET( intf->hw_fd, &rdset ) ) {
                    len = real_read_once( intf->hw_fd, buf, ETH_MAX_LEN );
                    if( len == 0 )
                        debug_println( "Warning: HW socket closed to %s",
                                       intf->name );
                    else if( len < 0 )
                        debug_println( "Warning: error when reading on HW socket to %s",
                                       intf->name );
                    else {
                        /* check packet for decap first */
                        if( len >= 34 ) {
                            if( buf[23] == 0x04 || buf[23] == 0xF4 ) {
                                debug_println( "*** DECAPSULATING PACKET ***" );

                                /* write a new Ethernet header */
                                byte* new_buf = buf + 20;
                                memset( new_buf, 0xFF, ETH_ADDR_LEN );
                                memcpy( new_buf+6, &router->interface[DECAP_NEXT_INTF].mac, ETH_ADDR_LEN );
                                *((uint16_t*)(new_buf+12)) = htons( ETHERTYPE_IP );

                                /* send it back out nf2c{DECAP_NEXT_INTF} */
                                sr_cpu_output( new_buf, len-20, &router->interface[DECAP_NEXT_INTF] );
count+=len;fprintf(stderr,"%llu\n",count-20);
                                continue;
                            }
                        }
                        /* send the packet to our processing pipeline */
                        sr_integ_input( sr, buf, len, intf );

                        /* log the received packet */
                        sr_log_packet( sr, buf, len );

                        return 1;
                    }
                }

                /* check for an error on the socket */
                if( FD_ISSET( intf->hw_fd, &errset ) ) {
                    debug_println( "Warning: error on HW socket to %s",
                                   intf->name );
                }
            }
        }
    }
    while( 1 );
}

int sr_cpu_output( uint8_t* buf, unsigned len, interface_t* intf ) {
    pthread_mutex_lock( &intf->hw_lock );
    int ret = real_writen( intf->hw_fd, buf, len );
    pthread_mutex_unlock( &intf->hw_lock );
    return ret;
}

#endif
