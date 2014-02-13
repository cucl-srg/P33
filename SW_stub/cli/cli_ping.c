/* Filename: cli_ping.c */

#include <netdb.h>
#include <stdlib.h>              /* malloc()                          */
#include <sys/time.h>            /* gettimeofday(), select(), timeval */
#include "cli.h"                 /* cli_is_time_to_shutdown()         */
#include "cli_ping.h"
#include "cli_network.h"         /* make_thread()                     */
#include "socket_helper.h"       /* writenstr()                       */
#include "../sr_integration.h"   /* sr_integ_findsrcip()              */
#include "../sr_thread.h"

/** maximum number of bytes an echo reply may have */
#define PING_BUF_SIZE (sizeof(hdr_icmp_t) + 40)

/** Encapsulation of a ping we're waiting on */
typedef struct ping_t {
    int fd;                      /* the socket fd to notify on success/fail */
    addr_ip_t dst_ip;            /* IP the ping was sent to */
    uint16_t seq;                /* echo request seq number (NBO) */
    struct timeval send_time;    /* time sent */
    struct timeval expire_time;  /* time to give up on the reply */

    struct ping_t* prev;         /* prev ping we're waiting on, if any */
    struct ping_t* next;         /* next ping we're waiting on, if any */
} ping_t;

/** list of outstanding echo requests in order of expiration (earliest first) */
static ping_t* ping_list;

/** synchronize access to the ping list */
static pthread_mutex_t ping_list_lock;

/** ping scrubber thread sleeps on this condition */
static pthread_cond_t  ping_list_cond;

/** cond for the ping scrubber to die */
static pthread_cond_t ping_scrubber_off_cond;

/** count of pings sent; used to fill sequence number field */
static uint16_t ping_count;

/**
 * Entry point for the thread which will scan the ping list when it is woken
 * up.  Whenever it finds outstanding ping(s), it waits for a reply.  When a
 * reply is received or a ping times out the requesting client is notified.
 */
THREAD_RETURN_TYPE cli_ping_scrubber_main( void* nil );

void cli_ping_init() {
    ping_list = NULL;
    pthread_mutex_init( &ping_list_lock, NULL );
    pthread_cond_init(  &ping_list_cond, NULL );
    pthread_cond_init(  &ping_scrubber_off_cond, NULL );
    make_thread( cli_ping_scrubber_main, NULL );
}

void cli_ping_destroy() {
    ping_t* p_to_free;

    /* wait for the scrubber thread to terminate */
    pthread_mutex_lock( &ping_list_lock );
    pthread_cond_signal( &ping_list_cond );
    pthread_cond_wait( &ping_scrubber_off_cond, &ping_list_lock );

    /* cleanup anything left on the ping list */
    while( ping_list ) {
        p_to_free = ping_list;
        ping_list = ping_list->next;
        free( p_to_free );
    }

    pthread_mutex_destroy( &ping_list_lock );
    pthread_cond_destroy( &ping_list_cond );
    pthread_cond_destroy( &ping_scrubber_off_cond );
}

void cli_ping_request( router_t* rtr, int fd, addr_ip_t ip ) {
    ping_t* p_new;
    addr_ip_t src_ip;
    char str_ip[STRLEN_IP];
    struct in_addr addr;
    struct hostent* he;

    /* get the lock now so the handler thread doesn't run while we're sending
       the request and therefore maybe get the reply before we put the item in
       the list */
    pthread_mutex_lock( &ping_list_lock );

    /* send the echo request (via the router directly ...) */
    ip_to_string( str_ip, ip );
  /*  if( (src_ip=sr_integ_findsrcip(ip)) )
        icmp_send( rtr, ip, src_ip, NULL, 0,
                   ICMP_TYPE_ECHO_REQUEST, 0, htons(PING_ID), htons(ping_count) );
    else {
        pthread_mutex_unlock( &ping_list_lock );
        writenf( fd, "Error: cannot find route to %s\n", str_ip );
        return;
    }*/  


    /* create the ping request */
    p_new = malloc_or_die( sizeof(ping_t) );
    p_new->fd = fd;
    p_new->dst_ip = ip;
    p_new->seq = htons(ping_count);
    ping_count += 1;
    gettimeofday( &p_new->send_time, NULL );
    gettimeofday( &p_new->expire_time, NULL );
    p_new->expire_time = time_add_usec( &p_new->expire_time,
                                        PING_MAX_WAIT_FOR_REPLY_USEC );

    debug_println( "now=%u.%u  expire=%u.%u",
                   p_new->send_time.tv_sec,
                   p_new->send_time.tv_usec,
                   p_new->expire_time.tv_sec,
                   p_new->expire_time.tv_usec );

    /* insert it into the correct place in the list (earliest expire first) */
    ping_t* p = ping_list;
    while( p ) {
        if( is_later( &p->expire_time, &p_new->expire_time ) ) {
            /* insert it before p */
            if( p == ping_list )
                ping_list = p_new;
            else
                p->prev->next = p_new;

            p_new->prev = p->prev;
            p_new->next = p;
            p->prev = p_new;
            break;
        }
        else if( p->next )
            p = p->next; /* keep looking */
        else {
            /* put it at the end of the list */
            p->next = p_new;
            p_new->prev = p;
            p_new->next = NULL;
            break;
        }
    }
    if( !ping_list ) {
        ping_list = p_new;
        p_new->next = p_new->prev = NULL;
    }

    pthread_cond_signal( &ping_list_cond );
    pthread_mutex_unlock( &ping_list_lock );

    /* let the client know we sent the ping */
    addr.s_addr = ip;
    he = gethostbyaddr( &addr, sizeof(addr), AF_INET );
    if( he && he->h_name )
        writenf( fd, "PING %s (%s)\n", str_ip, he->h_name );
    else
        writenf( fd, "PING %s\n", str_ip );
}

static void cli_ping_feedback( ping_t* p, bool worked ) {
    char str_ip[STRLEN_IP];
    float msec_passed;

    ip_to_string( str_ip, p->dst_ip );
    msec_passed = time_passed( &p->send_time, NULL ) / 1000.0f;

    if( worked )
        writenf( p->fd, "REPLY from %s: seq=%u, time=%.1fms\n",
                 str_ip, ntohs(p->seq), msec_passed );
    else {
        writenf( p->fd, "TIMEOUT: Ping to %s: seq=%u timed out (time=%.1fms)\n",
                 str_ip, ntohs(p->seq), msec_passed );
        debug_println( "Timed out ping request to %s (seq=%u)", str_ip, ntohs(p->seq) );
    }

    cli_send_prompt();
}

void cli_ping_handle_reply( addr_ip_t ip, uint16_t seq ) {
    ping_t* p;

    /* after shutdown is set, ping_list becomes garbage */
    if( cli_is_time_to_shutdown() )
        return;

    /* search the ping_list for a matching request */
    pthread_mutex_lock( &ping_list_lock );
    p = ping_list;
    while( p ) {
        if( p->dst_ip == ip ) {
            if( p->seq == seq ) {
                debug_println( "Received matching ping reply!" );

                /* remove p from the list */
                if( p == ping_list )
                    ping_list = p->next;
                else
                    p->prev->next = p->next;

                if( p->next )
                    p->next->prev = p->prev;

                /* tell the client about the reply */
                cli_ping_feedback( p, TRUE );

                /* all done with this ping request */
                free( p );
                break;
            }
        }

        p = p->next;
    }
    if( !ping_list )
        debug_println( "There are no outstanding requests to match to the Echo Reply." );
    else if( !p )
        debug_println( "Echo Reply does not match any outstanding request." );

    pthread_mutex_unlock( &ping_list_lock );
}

THREAD_RETURN_TYPE cli_ping_scrubber_main( void* nil ) {
    ping_t* p_to_free;
    struct timeval now, timeout;

    debug_pthread_init( "Ping Scrubber", "CLI Ping Request Scrubber" );
    pthread_detach( pthread_self() );

    pthread_mutex_lock( &ping_list_lock );
    while( !cli_is_time_to_shutdown() ) {
        while( ping_list && !cli_is_time_to_shutdown() ) {
            /* remove expired pings */
            gettimeofday( &now, NULL );
            while( is_later( &now, &ping_list->expire_time ) ) {

                debug_println( "now=%u.%u  expire=%u.%u",
                               now.tv_sec,
                               now.tv_usec,
                               ping_list->expire_time.tv_sec,
                               ping_list->expire_time.tv_usec );

                /* let the list know that ping_list has expired */
                cli_ping_feedback( ping_list, FALSE );

                p_to_free = ping_list;
                ping_list = ping_list->next;
                free( p_to_free );

                /* no pings left to check! */
                if( !ping_list )
                    break;
            }

            /* if no pings are left, nothing to do but wait */
            if( ! ping_list )
                break;

            /* determine timeout (next ping expiry or some min threshold) */
            timeout.tv_sec = now.tv_sec - ping_list->expire_time.tv_sec;
            timeout.tv_usec = now.tv_usec - ping_list->expire_time.tv_usec;
            if( timeout.tv_sec>0 || timeout.tv_usec>PING_HANDLER_MIN_RESPONSE_TIME_USEC ) {
                timeout.tv_sec = 0;
                timeout.tv_usec = PING_HANDLER_MIN_RESPONSE_TIME_USEC;
            }

            /* wait for timeout */
            pthread_mutex_unlock( &ping_list_lock );
            select( 0, NULL, NULL, NULL, &timeout );
            pthread_mutex_lock( &ping_list_lock );
        }

        /* wait for more work */
        pthread_cond_wait( &ping_list_cond, &ping_list_lock );
    }

    pthread_cond_signal( &ping_scrubber_off_cond );
    pthread_mutex_unlock( &ping_list_lock );
    THREAD_RETURN_NIL;
}
