/*
 * Filename: sr_common.h
 * Purpose: include common code and define common basic types for the router
 */

#ifndef SR_COMMON_H
#define SR_COMMON_H

#ifdef _LINUX_
#include <stdint.h> /* uintX_t */
#endif

/** number of seconds to give helper threads to shutdown */
#define SHUTDOWN_TIME 5

/** length in bytes of an IPv4 address */
#define IP_ADDR_LEN 4

/** length in bytes of an Ethernet (MAC) address */
#define ETH_ADDR_LEN 6

/* 4 octets of 3 chars each, 3 periods, 1 nul => 16 chars */
#define STRLEN_IP  16

/* 12 hex chars, 5 colons, 1 nul => 18 bytes */
#define STRLEN_MAC 18

/* add '/' and two numbers at most to a typical IP */
#define STRLEN_SUBNET (STRLEN_IP+3)

/** length of a time string */
#define STRLEN_TIME 26
#define STRLEN_COST 11

/** byte is an 8-bit unsigned integer */
typedef uint8_t byte;

/** boolean is stored in an 8-bit unsigned integer */
typedef uint8_t bool;
#define TRUE 1
#define FALSE 0

/** IP address type */
typedef uint32_t addr_ip_t;

/** MAC address type */
typedef struct {
    byte octet[ETH_ADDR_LEN];
} __attribute__ ((packed)) addr_mac_t;

#include "debug.h"

/* define endianness constants */
#ifndef __LITTLE_ENDIAN
#  define __LITTLE_ENDIAN 1
#endif
#ifndef __BIG_ENDIAN
#  define __BIG_ENDIAN 2
#endif

/* determine the appropriate byte order */
#ifndef __BYTE_ORDER
#  ifdef _CYGWIN_
#    define __BYTE_ORDER __LITTLE_ENDIAN
#  elif _LINUX_
#    define __BYTE_ORDER __LITTLE_ENDIAN
#  elif _SOLARIS_
#    define __BYTE_ORDER __BIG_ENDIAN
#  elif _DARWIN_
#    define __BYTE_ORDER __BIG_ENDIAN
#  else
#    error "unknown endian!"
#  endif
#endif

/**
 * Returns the sum of the start time plus the specified number of
 * microseconds.
 */
struct timeval time_add_usec( struct timeval* start, unsigned usec );

/** Returns TRUE if t1 is later than t2. */
bool is_later( struct timeval* t1, struct timeval* t2 );

/**
 * Returns TRUE if microsec_threshold microseconds or more have passed between
 * the start and end times.  If end is NULL, then the current time is used.
 */
bool has_time_passed( struct timeval* start /* borrowed */,
                      struct timeval* end   /* borrowed */,
                      uint64_t        microsec_threshold );

/**
 * Returns the numbers of microseconds passed between the two times.  If end is
 * NULL then the current time is used.
 */
uint64_t time_passed( struct timeval* start, struct timeval* end );

/** returns true if the mac is not all 0s */
bool is_valid_mac( addr_mac_t mac );

/** Uses the string to make an IP address */
addr_ip_t make_ip_addr( const char* str );

/** Uses the specified bytes to create a MAC address */
addr_mac_t make_mac_addr(byte byte5, byte byte4, byte byte3, byte byte2, byte byte1, byte byte0);

#ifdef _CPUMODE_
/** Returns the upper 16 bits of the MAC address */
inline uint16_t mac_hi( addr_mac_t* mac );

/** Returns the lower 32 bits of the MAC address */
inline uint32_t mac_lo( addr_mac_t* mac );

/** Sets the upper 16 bits of the MAC address */
inline void mac_set_hi( addr_mac_t* mac, uint16_t hi );

/** Sets the lower 32 bits of the MAC address */
inline void mac_set_lo( addr_mac_t* mac, uint32_t lo );

/** Returns TRUE if the all of the MAC's bits are zero. */
inline bool mac_is_zero( addr_mac_t* mac );
#endif

/**
 * Converts ip into a string and stores it in buf.  buf must be at least
 * STRLEN_IP bytes long.
 */
void ip_to_string( char* buf, addr_ip_t ip );

/**
 * Converts mac into a string and stores it in buf.  buf must be at least
 * STRLEN_MAC bytes long.
 */
void mac_to_string( char* buf, addr_mac_t* mac );

/**
 * Converts a subnet into a string and stores it in buf.  buf must be at least
 * STRLEN_SUBNET bytes long.
 */
void subnet_to_string( char* buf, addr_ip_t subnet, addr_ip_t mask );

/**
 * Converts a unix timestamp to a string.
 */
void time_to_string( char* buf, unsigned sec );

/**
 * Wrapper for snprintf.
 *
 * @return 0 on failure and otherwise returns the number of bytes written, not
 *         including the terminating NUL character.
 */
unsigned my_snprintf( char* str, unsigned size, const char* format, ... );

#ifdef _MEM_DEBUG_
#define myfree(ptr) { debug_print( "free %p (%s:%u) ... ", ptr,__FILE__,__LINE__ ); free(ptr); debug_print_more( "free done\n" ); }
#define calloc_or_die(num,size) zcalloc_or_die(num,size,__FILE__,__LINE__)
#define malloc_or_die(size) zmalloc_or_die(size,__FILE__,__LINE__)
#define realloc_or_die(ptr,size) zrealloc_or_die(ptr,size,__FILE__,__LINE__)
inline void* zcalloc_or_die( unsigned num, unsigned size, const char* file, unsigned line );
inline void* zrealloc_or_die( void* ptr, unsigned size, const char* file, unsigned line );
inline void* zmalloc_or_die( unsigned size, const char* file, unsigned line );
#else
#define myfree(ptr) { free(ptr); }
inline void* calloc_or_die( unsigned num, unsigned size );
inline void* realloc_or_die( void* ptr, unsigned size );
inline void* malloc_or_die( unsigned size );
#endif

#endif /* SR_COMMON_H */
