#ifndef UDEVs_MONITOR_H
#define UDEVs_MONITOR_H

#include <pthread.h>

struct monitor_t;
typedef struct monitor_t* monitor_t;

#define UDEV_MONITOR_TYPE_POWER		1
#define UDEV_MONITOR_TYPE_BLUETOOTH	2

struct z_udev_monitor {
	int type;

	pthread_t thread;
	pthread_mutex_t state_mutex;
};

typedef struct z_udev_monitor* udev_monitor_t;


int udev_monitor_from_args(int argc, char* argv[], monitor_t*);

int udev_start(monitor_t);

int udev_stop(monitor_t);

void udev_join(monitor_t);

int udev_monitor_destroy(monitor_t);


void udev_print_usage(int type);

#endif