#ifndef INOTIFY_H
#define INOTIFY_H

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

/**
 * Creates monitor from command line. Command line must
 * consist of mode flags and file path after them.
 *
 * @param argc
 * @param argv
 * @param monitor
 * @return CALL_SUCCESS
 * @return E_OUT_OF_MEMORY
 * @return E_INVALID_MONITOR_ARGUMENT
 * @return CALL_FAILURE
 */
int inotify_monitor_from_args(int argc, char* argv[], monitor_t*);

/**
 * Initializes and starts inotify monitor thread
 *
 * @param monitor
 * @returns CALL_SUCCESS
 * @returns CALL_FAILURE
 */
int inotify_start(monitor_t);

/**
 * Sets state flag to MONITOR_STATE_DYING.
 *
 * @param monitor
 * @return	CALL_SUCCESS
 * @return	E_MONITOR_NOT_INITIALIZED
 */
int inotify_stop(monitor_t);

/**
 * Waits util monitor returns
 *
 * @param monitor
 * @return CALL_SUCCESS
 * @return E_MONITOR_NOT_INITIALIZED
 */
void inotify_join(monitor_t);

/**
 * Can be used only after monitor death
 *
 * @param monitor
 * @return INVALID_MONITOR_STATE
 * @return see concrete monitors functions
 */
int inotify_monitor_destroy(monitor_t);


void inotify_print_usage();

#endif 