/*------------------------------------------------------------------------------
 * File: sr_base.c
 * Date: Spring 2002
 * Author: Martin Casado <casado@stanford.edu>
 * Extended for Mininet compatibility
 *                 August 2013
 *                 Matthew Ireland <mti20@cam.ac.uk>
 *
 * Entry module to the low level networking subsystem of the router.
 *
 * Caveats:
 *
 *  - sys_thread_init(..) in sr_lwip_transport_startup(..) MUST be called from
 *    the main thread before other threads have been started.
 *
 *  - lwip requires that only one instance of the IP stack exist, therefore
 *    at the moment we don't support multiple instances of sr.  However
 *    support for this (given a cooperative tcp stack) would be simple,
 *    simple allow sr_init_low_level_subystem(..) to create new sr_instances
 *    each time they are called and return an identifier.  This identifier
 *    must be passed into sr_global_instance(..) to return the correct
 *    instance.
 *
 *  - lwip needs to keep track of all the threads so we use its
 *    sys_thread_new(), this is essentially a wrapper around
 *    pthread_create(..) that saves the thread's ID.  In the future, if
 *    we move away from lwip we should simply use pthread_create(..)
 *
 *----------------------------------------------------------------------------*/

#ifdef _SOLARIS_
#define __EXTENSIONS__
#endif /* _SOLARIS_ */

/* get unistd.h to declare gethostname on linux */
#define __USE_BSD 1

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#ifdef _LINUX_
#include <getopt.h>
#endif /* _LINUX_ */

#include "lwip/tcp.h"
#include "lwip/memp.h"
#include "lwip/transport_subsys.h"

#include "sr_base.h"
#include "sr_base_internal.h"
#include "debug.h"

#ifdef _CPUMODE_
#include "sr_cpu_extension_nf2.h"
#endif

#ifdef MININET_MODE
#include "sr_mininet_extension.h"
#endif

extern char* optarg;

/** current state of the router */
router_status_t sr_status;
router_status_t sr_get_status() { return sr_status; }
void sr_set_status( router_status_t status ) { sr_status = status; }

static void usage(char* );
static int  sr_lwip_transport_startup(void);
static void sr_set_user(struct sr_instance* sr);
static void sr_init_instance(struct sr_instance* sr);
static void sr_low_level_network_subsystem(void *arg);
static void sr_destroy_instance(struct sr_instance* sr);

/**
 * Returns a logfile name which is in the format:
 *   log/topo<topo>-<host>-<count>.log
 *
 * The <count> is obtained from the file log/.count (the number in the file is
 * then incremented by one).
 *
 * @return logfile name (caller responsible for freeing it)
 */
static char* gen_logfile( unsigned topo, char* host ) {
    FILE* fp;
    char str_num[12];
    unsigned num;
    char* logfile;

    /* read the current count */
    fp = fopen( "log/.count", "r" );
    if( ! fp )
        die( "Error: could not open file log/.count for read-access" );

    fgets( str_num, 12, fp );
    sscanf( str_num, "%u", &num );
    fclose( fp );

    /* write the new count */
    fp = fopen( "log/.count", "w" );
    if( ! fp )
        die( "Error: could not open file log/.count for write-access" );

    fprintf( fp, "%u", num+1 );
    fclose( fp );

    /* create the logfile name */
    logfile = malloc( 512 * sizeof(char) );
    assert( logfile && "malloc failed" );
    snprintf( logfile, 512, "log/topo%u-%s-%u.log", topo, host, num );
    return logfile;
}

/*----------------------------------------------------------------------------
 * sr_init_low_level_subystem
 *
 * Entry method to the sr low level network subsystem. Responsible for
 * managing, connecting to the server, reserving the topology, reading
 * the hardware information and starting the packet recv(..) loop in a
 * seperate thread.
 *
 * Caveats :
 *  - this method can only be called once!
 *
 *---------------------------------------------------------------------------*/

int sr_init_low_level_subystem(int argc, char **argv)
{
    
    char  *host   = "vrhost";
    char  *rtable = "rtable.conf";
    char  *itable = CPU_HW_FILENAME;
    char  *server = "171.67.71.18";
    uint16_t port =  12345;
    uint16_t topo =  0;
    int ospf = 1;

    char  *client = 0;
    char  *logfile = 0;
    int free_logfile = 0;

    /* -- singleton instance of router, passed to sr_get_global_instance
          to become globally accessible                                  -- */
    static struct sr_instance* sr = 0;

    int c;

    if ( sr )
    {
        fprintf(stderr,  "Warning: because of limitations in lwip, ");
        fprintf(stderr,  " sr supports 1 router instance per process. \n ");
        return 1;
    }

    sr = (struct sr_instance*) malloc(sizeof(struct sr_instance));

    #ifdef MININET_MODE
        char* router_name = NULL;
        int itable_not_specified = 1;
    #endif /* MININET_MODE */

    while ((c = getopt(argc, argv, "hns:v:p:c:t:r:l:i:z:")) != EOF)
    {
        switch (c)
        {
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'p':
                port = atoi((char *) optarg);
                break;
            case 't':
                topo = atoi((char *) optarg);
                break;
            case 'v':
                host = optarg;
                break;
            case 'r':
                rtable = optarg;
                break;
            case 'c':
                client = optarg;
                break;
            case 's':
                server = optarg;
                break;
            case 'l':
                logfile = optarg;
                if( strcmp( "auto", logfile )==0 ) {
                    logfile = gen_logfile( topo, host );
                    free_logfile = 1;
                }
                Debug("\nLOGGING to %s\n\n", logfile);
                break;
            case 'i':
                itable = optarg;
                #ifdef MININET_MODE
		    itable_not_specified=0;
                #endif
                break;
            case 'n':
                Debug("\nOSPF disabled!\n\n");
                ospf = 0;
                break;
          #ifdef MININET_MODE
            case 'z':
	      router_name = optarg;
              break;
          #endif /* MININET_MODE */
        } /* switch */
    } /* -- while -- */

    // fill in router name and check that the interface file is specified (in the case of Mininet)
    #ifdef MININET_MODE
        if (router_name == NULL) {
	    die( "Router name must be set in command line arguments for mininet mode! See usage for more details." );
	} else if (itable_not_specified) {
	    die( "The file containing the interface table must be specified in mininet mode!" );
	} else {
	    sr->router_name = router_name;
	}
    #endif  /* MININET_MODE */

    Debug("\n");

#ifdef MININET_MODE
    Debug("< -- Starting sr in Mininet router mode -- >");
#else   /* not MININET_MODE */
#ifdef _CPUMODE_
    Debug("< -- Starting sr in NetFPGA (CPU) mode -- >");
#else   /* not CPUMODE or MININET_MODE */
# ifdef _MANUAL_MODE_
    Debug("< -- Starting sr in Manual router mode -- >");
# endif /* MANUAL_MODE */
#endif  /* CPUMODE */
#endif  /* MININET_MODE */

    Debug("\n\n");

    /* -- required by lwip, must be called from the main thread -- */
    sys_thread_init();

    /* -- zero out sr instance and set default configurations -- */
    debug_println("*****SR_INIT_INSTANCE");
    sr_init_instance(sr);
    sr->interface_subsystem->use_ospf = ospf;

    strncpy(sr->rtable, rtable, SR_NAMELEN);

    // topo_id for MININET_MODE, _CPUMODE_ and _MANUAL_MODE
    sr->topo_id = 0;

#if defined MININET_MODE || defined _CPUMODE_
    printf("*****SR_READ_INTF_FROM_FILE\n");
    sr_read_intf_from_file( sr->interface_subsystem, itable );
    debug_println("*****INTERFACES INITIALISED\n");
    sr->hw_init = 1;
    sr_integ_hw_setup(sr);
#   ifdef MININET_MODE
        strncpy(sr->vhost,  "mininet",    SR_NAMELEN);
        strncpy( sr->server, "mininet mode (no server)", SR_NAMELEN );
#   else  /* _CPUMODE_ */
        strncpy(sr->vhost,  "cpu",    SR_NAMELEN);
        strncpy( sr->server, "hw mode (no server)", SR_NAMELEN );
#   endif /* MININET_MODE */
#else     /* NOT _CPUMODE_ or MININET_MODE (i.e. manual) */
# ifdef _MANUAL_MODE_
    strncpy( sr->vhost, "manual", SR_NAMELEN );
    strncpy( sr->server, "manual mode (no server)", SR_NAMELEN );
# endif /* MANUAL_MODE */
#endif  /* _CPUMODE_ or MININET_MODE */

    if(! client )
    { sr_set_user(sr); }
    else

    { strncpy(sr->user, client,  SR_NAMELEN); }

    if ( gethostname(sr->lhost,  SR_NAMELEN) == -1 )
    {
        perror("gethostname(..)");
        return 1;
    }
    sr_lwip_transport_startup();


#if !defined _CPUMODE_ && !defined MININET_MODE
#if defined _MANUAL_MODE_
    /* create interfaces for the router */
    sr_read_intf_from_file( sr->interface_subsystem, itable );
    sr->hw_init = 1;
    sr_integ_hw_setup(sr);
#   ifdef _MANUAL_MODE_
      sr_manual_init( sr );
#   endif /* _MANUAL_MODE_ */

#endif  /* _MANUAL_MODE_ || MININET_MODE */
#endif  /* ifndef _CPUMODE_ */

    /* -- start low-level network thread -- */
    Debug( "Starting the low-level network thread\n" );
    sr_set_status( STATUS_RUNNING );
    sys_thread_new(sr_low_level_network_subsystem, NULL);

    return 0;
}/* -- main -- */

/*-----------------------------------------------------------------------------
 * Method: sr_set_subsystem(..)
 * Scope: Global
 *
 * Set the router core in sr_instance
 *
 *---------------------------------------------------------------------------*/

void sr_set_subsystem(struct sr_instance* sr, router_t* core)
{
    sr->interface_subsystem = core;
} /* -- sr_set_subsystem -- */


/*-----------------------------------------------------------------------------
 * Method: sr_get_subsystem(..)
 * Scope: Global
 *
 * Return the sr router core
 *
 *---------------------------------------------------------------------------*/

router_t* sr_get_subsystem(struct sr_instance* sr)
{
    return sr->interface_subsystem;
} /* -- sr_get_subsystem -- */

/*-----------------------------------------------------------------------------
 * Method: sr_get_global_instance(..)
 * Scope: Global
 *
 * Provide the world with access to sr_instance(..)
 *
 *---------------------------------------------------------------------------*/

struct sr_instance* sr_get_global_instance(struct sr_instance* sr)
{
    static struct sr_instance* sr_global_instance = 0;

    if ( sr )
    { sr_global_instance = sr; }

    return sr_global_instance;
}

static void sr_low_level_network_subsystem(void *arg) {
    struct sr_instance* sr = sr_get_global_instance(NULL);

    debug_pthread_init( "Network", "Low-Level Network Subsystem Thread" );

    /* choose the method for reading/processing packets */
#ifdef _CPUMODE_
#    define SR_LOW_LEVEL_READ_METHOD sr_cpu_input(sr)
#else
# ifdef _MANUAL_MODE_
#    define SR_LOW_LEVEL_READ_METHOD sr_manual_read_packet(sr)
# else
#  ifdef MININET_MODE
#    define SR_LOW_LEVEL_READ_METHOD sr_mininet_read_packet(sr)
#  endif  /* MININET_MODE */
# endif   /* _MANUAL_MODE_ */
#endif    /* _CPUMODE_ */

    /* read packets until we can read no more or status changes from RUNNING */
    while( SR_LOW_LEVEL_READ_METHOD==1 && sr_get_status()==STATUS_RUNNING );

    /* cleanup */
    sr_destroy_instance(sr);
    sr_set_status( STATUS_TERMINATED );
}

/*-----------------------------------------------------------------------------
 * Method: sr_lwip_transport_startup(..)
 * Scope: local
 *---------------------------------------------------------------------------*/

static int sr_lwip_transport_startup(void)
{
    sys_init();
    mem_init();
    memp_init();
    pbuf_init();

    transport_subsys_init(0, 0);

    return 0;
} /* -- sr_lwip_transport_startup -- */


/*-----------------------------------------------------------------------------
 * Method: sr_set_user(..)
 * Scope: local
 *---------------------------------------------------------------------------*/

static void sr_set_user(struct sr_instance* sr)
{
    uid_t uid = getuid();
    struct passwd* pw = 0;

    /* REQUIRES */
    assert(sr);

    if(( pw = getpwuid(uid) ) == 0)
    {
        fprintf (stderr, "Error getting username, using something silly\n");
        strncpy(sr->user, "something_silly",  SR_NAMELEN);
    }
    else
    { strncpy(sr->user, pw->pw_name,  SR_NAMELEN); }

} /* -- sr_set_user -- */

/*-----------------------------------------------------------------------------
 * Method: sr_init_instance(..)
 * Scope:  Local
 *
 *----------------------------------------------------------------------------*/

static
void sr_init_instance(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* -- set argument as global singleton -- */
    sr_get_global_instance(sr);

    sr->sockfd   = -1;
    sr->user[0]  = 0;
    sr->vhost[0] = 0;
    sr->topo_id  = 0;
    sr->logfile  = 0;
    sr->hw_init  = 0;

    sr->interface_subsystem = 0;

    pthread_mutex_init(&(sr->send_lock), 0);

    sr_integ_init(sr);
} /* -- sr_init_instance -- */

/*-----------------------------------------------------------------------------
 * Method: sr_destroy_instance(..)
 * Scope:  local
 *
 *----------------------------------------------------------------------------*/

static void sr_destroy_instance(struct sr_instance* sr) {
    assert(sr);
    sr_integ_destroy(sr);
    free( sr );
}

/*-----------------------------------------------------------------------------
 * Method: usage(..)
 * Scope: local
 *---------------------------------------------------------------------------*/

static void usage(char* argv0)
{
  #ifdef MININET_MODE
    printf("SR -- user space Simple Routing program\n");
    printf("Format: %s [-h] -z router_name [-n]\n",argv0);
    printf("             [-r rtable_file] [-l log_file] [-i interface_file]\n");
  #else  /* NETFPGA or MANUAL modes */
    printf("Simple Router Client\n");
    printf("Format: %s [-h] [-v host] [-s server] [-p port] [-n] \n",argv0);
    printf("             [-t topo id] [-r rtable_file] [-l log_file] [-i interface_file]\n");
  #endif /* MININET_MODE */
} /* -- usage -- */
