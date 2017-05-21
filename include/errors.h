#ifndef ERRORS_H
#define ERRORS_H

/**
 * Success call
 */
#define CALL_SUCCESS		        0

/**
 * Common error
 */
#define CALL_FAILURE		        1

/**
 * If got invalid argument during monitor parsing
 */
#define E_INVALID_MONITOR_ARGUMENT	2

/**
 * On out of memory error
 */
#define E_OUT_OF_MEMORY		        3

/**
 * When trying to use monitor of illegal type
 */
#define E_INVALID_MONITOR_TYPE      4

/**
 * On attempt to use not initialized monitor
 */
#define E_MONITOR_NOT_INITIALIZED	5

/**
 * On attempt to parse invalid command line
 */
#define E_INVALID_INPUT				6

/**
 * On attempt to use monitor in inproper state
 */
#define E_MONITOR_INVALID_STATE		7

#ifdef DAEMON
	#define E_FORK 					100
	#define E_SET_SID 				101
	#define E_OPEN_CONFIGS 			102
	#define E_OPEN_LOGS 			103
#endif

#endif