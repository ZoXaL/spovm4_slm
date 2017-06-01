#ifndef INOTIFY_MONITOR_H
#define INOTIFY_MONITOR_H

#include <pthread.h>

struct monitor_t;
typedef struct monitor_t* monitor_t;

struct inotify_monitor {
	int inotify_file_descriptor;
	char* file_path;
	char* mode;

	pthread_t thread;
	pthread_mutex_t state_mutex;
};

typedef struct inotify_monitor* inotify_monitor_t;

int inotify_monitor_from_args(int argc, char* argv[], monitor_t*);

int inotify_start(monitor_t);

int inotify_stop(monitor_t);

void inotify_join(monitor_t);

int inotify_monitor_destroy(monitor_t);

void inotify_print_usage();

#endif 