/*-----------------------------------------------------------------------------
 * file:  sr_base.h
 * date:  Tue Feb 03 10:49:47 PST 2004
 * Author: Martin Casado
 *
 * Description: start the sr low_level subsystems.  This the is basic
 *              "user level" access to the subsystem.
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_BASE_H
#define SR_BASE_H

/** possible states the router may be in */
typedef enum router_status_t {
    STATUS_INITIALIZING,
    STATUS_RUNNING,
    STATUS_SHUTTING_DOWN,
    STATUS_TERMINATED
} router_status_t;

/** gets the current status of the router */
router_status_t sr_get_status();

/** sets the current status of the router */
void sr_set_status( router_status_t status );

/** initialize the router and start running it in a separate thread */
int   sr_init_low_level_subystem(int argc, char **argv);

#endif  /* -- SR_BASE_H -- */
