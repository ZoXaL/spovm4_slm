#include <memory.h>
#include <stdio.h>
#include <logging/logging.h>
#include "monitor.h"
#include "errors.h"
#include "udev_monitor.h"
#include "dbus_monitor.h"


int monitor_from_args(int argc, char* argv[], monitor_t* monitor) {
	if (argc < 1) {
		return E_INVALID_INPUT;
	}else if (strcmp(argv[0], "--file") == 0) {
		int return_code = inotify_monitor_from_args(argc, argv, monitor);
#ifndef DAEMON
		if (return_code == E_INVALID_MONITOR_ARGUMENT) {
			inotify_print_usage();
		}
#endif
		return return_code;
	} else if (strcmp(argv[0], "--network") == 0
			   || strcmp(argv[0], "--disks") == 0) {
		int return_code = dbus_monitor_from_args(argc, argv, monitor);
#ifndef DAEMON
		if (return_code == E_INVALID_MONITOR_ARGUMENT) {
			if (strcmp(argv[0], "--network") == 0) {
				dbus_print_usage(DBUS_MONITOR_TYPE_NM);
			} else {
				dbus_print_usage(DBUS_MONITOR_TYPE_UDISKS);
			}
		}
#endif
		return return_code;
	} else if (strcmp(argv[0], "--power") == 0
			   || strcmp(argv[0], "--bluetooth") == 0) {
		int return_code = udev_monitor_from_args(argc, argv, monitor);
#ifndef DAEMON
		if (return_code == E_INVALID_MONITOR_ARGUMENT) {
			if (strcmp(argv[0], "--bluetooth") == 0) {
				udev_print_usage(UDEV_MONITOR_TYPE_BLUETOOTH);
			} else {
				udev_print_usage(UDEV_MONITOR_TYPE_POWER);
			}
		}
#endif
		return return_code;
	} else {
		return E_INVALID_INPUT;
	}
	return CALL_SUCCESS;
}

int start_monitor(monitor_t monitor) {
	printf("starting monitor of type %d", monitor->type);
	switch (monitor->type) {
		case MONITOR_TYPE_INOTIFY : {
			return inotify_start(monitor);
		}
		case MONITOR_TYPE_DBUS : {
			return dbus_start(monitor);
		}
		case MONITOR_TYPE_UDEV : {
			return udev_start(monitor);
		}
		default: {
			return MONITOR_TYPE_INVALID;
		}
	}
}

int stop_monitor(monitor_t monitor) {
	switch (monitor->type) {
		case MONITOR_TYPE_INOTIFY : {
			return inotify_stop(monitor);
		}
		case MONITOR_TYPE_DBUS : {
			return dbus_stop(monitor);
		}
		case MONITOR_TYPE_UDEV : {
			return udev_stop(monitor);
		}
		default: {
			return MONITOR_TYPE_INVALID;
		}
	}
}

void join_monitor(monitor_t monitor) {
	switch (monitor->type) {
		case MONITOR_TYPE_INOTIFY : {
			inotify_join(monitor);
			break;
		}
		case MONITOR_TYPE_DBUS : {
			dbus_join(monitor);
			break;
		}
		case MONITOR_TYPE_UDEV : {
			udev_join(monitor);
			break;
		}
		default: {}
	}
}

int destroy_monitor(monitor_t monitor) {
	switch (monitor->type) {
		case MONITOR_TYPE_INOTIFY : {
			return inotify_monitor_destroy(monitor);
		}
		case MONITOR_TYPE_DBUS : {
			return dbus_monitor_destroy(monitor);
		}
		case MONITOR_TYPE_UDEV : {
			return udev_monitor_destroy(monitor);
		}
		default: {
			return E_INVALID_MONITOR_TYPE;
		}
	}
}