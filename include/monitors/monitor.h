#include "inotify_monitor.h"

#ifndef MONITOR_FACTORY_H
#define MONITOR_FACTORY_H

#define MONITOR_TYPE_INVALID 		0
#define MONITOR_TYPE_INOTIFY		1
#define MONITOR_TYPE_DBUS 			2
#define MONITOR_TYPE_UDEV		 	3

#define MONITOR_STATE_NOT_INITIALIZED 	0
#define MONITOR_STATE_INITIALIZED 		1
#define MONITOR_STATE_RUNNING 			2
#define MONITOR_STATE_DYING 			3
#define MONITOR_STATE_DEAD	 			4

struct monitor_t {
	int type;
	union {
		inotify_monitor_t inotify;
		//dbus_monitor_t dbus;
		//udev_monitor_t udev;
	};

	int state;
};
typedef struct monitor_t* monitor_t;

/**
 * Creates monitor from command line
 *
 * @param argc
 * @param argv
 * @param monitor
 * @return INVALID_MONITOR_TYPE
 * @return see concrete monitors functions
 */
int monitor_from_args(int argc, char* argv[], monitor_t*);

/**
 * @param monitor
 * @return INVALID_MONITOR_TYPE
 * @return see concrete monitors functions
 */
int start_monitor(monitor_t);

/**
 * @param monitor
 * @return INVALID_MONITOR_TYPE
 * @return see concrete monitors functions
 */
int stop_monitor(monitor_t);

/**
 * @param monitor
 * @return INVALID_MONITOR_TYPE
 * @return see concrete monitors functions
 */
void join_monitor(monitor_t);

/**
 * Can be used only after monitor death
 *
 * @param monitor
 * @return INVALID_MONITOR_STATE
 * @return see concrete monitors functions
 */
int destroy_monitor(monitor_t);

/**
 * Prints usage for concrete monitor
 * @param monitor
 */
void monitor_show_usage(monitor_t);

#endif