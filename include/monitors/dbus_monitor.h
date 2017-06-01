#ifndef DBUS_MONITOR_H
#define DBUS_MONITOR_H

#define DBUS_MONITOR_TYPE_NM		1
#define DBUS_MONITOR_TYPE_UDISKS	2

#include <pthread.h>
#include <glib.h>

struct monitor_t;
typedef struct monitor_t* monitor_t;

struct dbus_monitor {
	int type;

	GMainLoop* subscription_loop;
	pthread_t thread;
	pthread_mutex_t state_mutex;
};

typedef struct dbus_monitor* dbus_monitor_t;

int dbus_monitor_from_args(int argc, char* argv[], monitor_t*);

int dbus_start(monitor_t);

int dbus_stop(monitor_t);

void dbus_join(monitor_t);

int dbus_monitor_destroy(monitor_t);

void dbus_print_usage(int type);

#endif