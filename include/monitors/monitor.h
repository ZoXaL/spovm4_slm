#ifndef MONITOR_FACTORY_H
#define MONITOR_FACTORY_H

#include "inotify_monitor.h"
#include "dbus_monitor.h"
#include "udev_monitor.h"

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
		dbus_monitor_t dbus;
		udev_monitor_t udev;
	};

	int state;
};
typedef struct monitor_t* monitor_t;

int monitor_from_args(int argc, char* argv[], monitor_t*);

int start_monitor(monitor_t);

int stop_monitor(monitor_t);

void join_monitor(monitor_t);

int destroy_monitor(monitor_t);

#endif