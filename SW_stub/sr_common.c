/* Filename: sr_common.c */

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>      /* snprintf() */
#include <stdlib.h>     /* calloc, malloc, realloc */
#include <string.h>     /* strncpy */
#include <sys/time.h>   /* struct timeval */
#include <time.h>
#include "sr_common.h"

struct timeval time_add_usec( struct timeval* start, unsigned usec ) {
    struct timeval ret;
    unsigned carry_sec;
    unsigned tot_usec;

    tot_usec = usec + start->tv_usec;
    carry_sec = tot_usec / 1000000;

    ret.tv_sec  = start->tv_sec  + carry_sec;
    ret.tv_usec = tot_usec - (1000000 * carry_sec);
    return ret;
}

bool is_later( struct timeval* t1, struct timeval* t2 ) {
    return( (t1->tv_sec > t2->tv_sec) ||
            (t1->tv_sec==t2->tv_sec && t1->tv_usec>t2->tv_usec) );
}


bool has_time_passed( struct timeval* start,
                      struct timeval* end,
                      uint64_t        microsec_threshold ) {
    return( time_passed(start, end) > microsec_threshold );
}

uint64_t time_passed( struct timeval* start, struct timeval* end ) {
    struct timeval now;
    uint64_t start_usec;
    uint64_t end_usec;

    if( !end ) {
        gettimeofday( &now, 0 );
        end = &now;
    }

    start_usec = start->tv_sec*1000000 + start->tv_usec;
    end_usec   =   end->tv_sec*1000000 +   end->tv_usec;
    return( end_usec - start_usec );
}

bool is_valid_mac( addr_mac_t mac ) {
    unsigned i;
    for( i=0; i<ETH_ADDR_LEN; i++ )
        if( mac.octet[i] != 0 )
            return TRUE;

    return FALSE;
}

addr_ip_t make_ip_addr( const char* str ) {
    struct in_addr addr;
    if( inet_aton( str, &addr) == 0 )
        die( "Error: cannot convert %s to valid IP", str );

    return addr.s_addr;
}

addr_mac_t make_mac_addr(byte byte5, byte byte4, byte byte3, byte byte2, byte byte1, byte byte0){
  addr_mac_t addr_mac;

  addr_mac.octet[5] =byte5;
  addr_mac.octet[4] =byte4;
  addr_mac.octet[3] =byte3;
  addr_mac.octet[2] =byte2;
  addr_mac.octet[1] =byte1;
  addr_mac.octet[0] =byte0;

  return addr_mac;

}

#ifdef _CPUMODE_
inline uint16_t mac_hi( addr_mac_t* mac ) {
  return (mac->octet[0]<<8) + mac->octet[1];
}

inline uint32_t mac_lo( addr_mac_t* mac ) {
  return (mac->octet[2]<<24)+(mac->octet[3]<<16)+(mac->octet[4]<<8)+mac->octet[5];
}

inline void mac_set_hi( addr_mac_t* mac, uint16_t hi ) {
  mac->octet[0] = hi >> 8U;
  mac->octet[1] = (hi & 0x00FF);
}

inline void mac_set_lo( addr_mac_t* mac, uint32_t lo ) {
  mac->octet[2] = lo >> 24U;
  mac->octet[3] = (lo & 0x00FF0000) >> 16U;
  mac->octet[4] = (lo & 0x0000FF00) >> 8U;
  mac->octet[5] = (lo & 0x000000FF);
}

inline bool mac_is_zero( addr_mac_t* mac ) {
    return (*(uint32_t*)(&mac->octet[0]))==0 && (*(uint16_t*)(&mac->octet[4]))==0;
}
#endif

void ip_to_string( char* buf, addr_ip_t ip ) {
    byte* bytes;


    bytes = (byte*)&ip;

    snprintf( buf, STRLEN_IP, "%u.%u.%u.%u",
              bytes[0],
              bytes[1],
              bytes[2],
              bytes[3] );
}

void mac_to_string( char* buf, addr_mac_t* mac ) {
    snprintf( buf, STRLEN_MAC, "%X%X:%X%X:%X%X:%X%X:%X%X:%X%X",
              mac->octet[0] >> 4, 0x0F & mac->octet[0],
              mac->octet[1] >> 4, 0x0F & mac->octet[1],
              mac->octet[2] >> 4, 0x0F & mac->octet[2],
              mac->octet[3] >> 4, 0x0F & mac->octet[3],
              mac->octet[4] >> 4, 0x0F & mac->octet[4],
              mac->octet[5] >> 4, 0x0F & mac->octet[5] );
}

/* ones(x) -- counts the number of bits in x with the value 1.
 * Based on Hacker's Delight (2003) by Henry S. Warren, Jr.
 */
uint32_t ones( register uint32_t x ) {
  x -= ((x >> 1) & 0x55555555);
  x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
  x = (((x >> 4) + x) & 0x0f0f0f0f);
  x += (x >> 8);
  x += (x >> 16);

  return( x & 0x0000003f );
}

void subnet_to_string( char* buf, addr_ip_t subnet, addr_ip_t mask ) {
    unsigned num_ones;
    byte* bytes;

    num_ones = ones(mask);
    bytes = (byte*)&subnet;

    if( num_ones == 0 && subnet == 0 )
        snprintf( buf, STRLEN_SUBNET, "<catch-all>" );
    else if( num_ones <= 8 )
        snprintf( buf, STRLEN_SUBNET, "%u/%u",
                  bytes[0], num_ones );
    else if( num_ones <= 16 )
        snprintf( buf, STRLEN_SUBNET, "%u.%u/%u",
                  bytes[0], bytes[1], num_ones );
    else if( num_ones <= 24 )
        snprintf( buf, STRLEN_SUBNET, "%u.%u.%u/%u",
                  bytes[0], bytes[1], bytes[2], num_ones );
    else
        snprintf( buf, STRLEN_SUBNET, "%u.%u.%u.%u/%u",
                  bytes[0], bytes[1], bytes[2], bytes[3], num_ones );
}

void time_to_string( char* buf, unsigned sec ) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    /* lock protects the static buffer which ctime populats */
    time_t t = sec;
    pthread_mutex_lock( &lock );
    strncpy( buf, ctime(&t), STRLEN_TIME );
    pthread_mutex_unlock( &lock );
}

unsigned my_snprintf( char* str, unsigned size, const char* format, ... ) {
    int written;
    va_list args;
    va_start( args, format );

    written = vsnprintf( str, size, format, args );
    if( written<0 || written>=size )
        return 0;
    else
        return written;

    va_end( args );
}

#ifdef _MEM_DEBUG_
inline void* zcalloc_or_die( unsigned num, unsigned size, const char* file, unsigned line ) {
    void* ret;
    ret = calloc( num, size );
    true_or_die( ret!=NULL, "Error: calloc failed (%s:%u)", file, line );
    debug_println( "calloc => %p (%s:%u)", ret, file, line );
    return ret;
}

inline void* zmalloc_or_die( unsigned size, const char* file, unsigned line ) {
    void* ret;
    ret = malloc( size );
    true_or_die( ret!=NULL, "Error: malloc failed (%s:%u)", file, line );
    debug_println( "malloc => %p (%s:%u)", ret, file, line );
    return ret;
}

inline void* zrealloc_or_die( void* ptr, unsigned size, const char* file, unsigned line ) {
    void* ret;
    ret = realloc( ptr, size );
    true_or_die( ret!=NULL, "Error: realloc failed (%s:%u)", file, line );
    debug_println( "realloc => %p (%s:%u) (was %p)", ret, file, line, ptr );
    return ret;
}
#else
inline void* calloc_or_die( unsigned num, unsigned size ) {
    void* ret;
    ret = calloc( num, size );
    true_or_die( ret!=NULL, "Error: calloc failed" );
    return ret;
}

inline void* malloc_or_die( unsigned size ) {
    void* ret;
    ret = malloc( size );
    true_or_die( ret!=NULL, "Error: malloc failed" );
    return ret;
}

inline void* realloc_or_die( void* ptr, unsigned size ) {
    void* ret;
    ret = realloc( ptr, size );
    true_or_die( ret!=NULL, "Error: realloc failed" );
    return ret;
}
#endif
