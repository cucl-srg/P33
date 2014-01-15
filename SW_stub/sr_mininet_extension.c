/*--------------------------------------------------------------------------------
 * File:   sr_mininet_extension.c
 * Date:   Mon Aug 11 13:50:00 BST 2013
 * Author: Matthew Ireland <mti20@cam.ac.uk>
 *
 * Description: Functions for reading and writing to raw sockets in mininet.
 *              Based on sr_cpu_extension_nf2.c
 *              
 *
 *------------------------------------------------------------------------------*/

#include "sr_mininet_extension.h"

#ifdef MININET_MODE

/*
 *   Creates and initialises a blocking raw socket on the desired interface.
 */
int sr_mininet_init_intf_socket_withname ( char* iface_name ) {
  int s;   // socket file description

  debug_println("Mininet -- creating socket on interface %s", iface_name);
  if ( (s = socket( PF_PACKET, SOCK_RAW, htons(ETH_P_ALL) )) < 0 ) {
    perror("socket");
    die( "Error: socket creation failed" );
  } else {
    debug_println( "got socket to intf %d", s );
  }
  
  // at the moment, the socket will receive packets from all interfaces
  // bind interface to socket
  struct ifreq ifr;
  bzero( &ifr, sizeof(struct ifreq) );
  strncpy( ifr.ifr_ifrn.ifrn_name, iface_name, IFNAMSIZ );
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
  
  debug_println( "Connected to mn hardware interface %s", iface_name );   // TODO remove "mn"
  
  return s;
}

/*
 *   Pretty external interface to sr_mininet_init_intf_socket_withname.
 *   Instead of supplying the name (e.g. itable), will take the router_name and
 *   interface_index as arguments and setup the interface using the standard mininet
 *   naming convention. Useful if performing intialisation in a loop, passing the
 *   loop counter variable in as an argument for interface_index.
 *   Initialises an interface whose name takes the form rn-ethx for 0<=x<=3, where rn
 *   is a string containing the name of the router (e.g. r0, r1, ...).
 *   Creates and initialises a blocking raw socket on the desired interface.
 */
int sr_mininet_init_intf_socket( char* router_name, int interface_index ) {
  true_or_die( interface_index>=0 && interface_index<=ROUTER_MAX_INTERFACES,
	       "Error: unexpected interface_index: %u", interface_index );
  
  char iface_name[32];
  sprintf( iface_name, "%s-eth%u", router_name, interface_index );

  return sr_mininet_init_intf_socket_withname( iface_name );
}


/*
 *   TODO: proper description of how this works
 *   Blocks, using select to wait until something happens on one of the sockets and
 *   then polls each of the interfaces to see which one (if any) has data to read and
 *   passes the available data (if it exists) on to the student's code
 */
int sr_mininet_read_packet( struct sr_instance* sr ) {
    byte buf[2000];
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

            }
        }

        /* wait for something to happen */
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;
        ret = select( max_fd + 1, &rdset, NULL, &errset, &timeout );

        /* check each intf to see if something interesting happened on it */
        for( i=0; i<router->num_interfaces; i++ ) {
            intf = &router->interface[i];
            /* check for available bytes in the input buffer */
                if( FD_ISSET( intf->hw_fd, &rdset ) ) {
                    len = real_read_once( intf->hw_fd, buf, 1500 );
                    sr_integ_input( sr, buf, len, intf );
		    /* log the received packet */
                    sr_log_packet( sr, buf, len );
	            return 1;
                }
        }
    }
    while( 1 );
}

int sr_mininet_output( uint8_t* buf, unsigned len, interface_t* intf ) {
	return 0;
}

#endif /* MININET_MODE */
