#include <malloc.h>
#include <memory.h>
#include <logging/logging.h>
#include "monitor.h"
#include "errors.h"
#include "udev_monitor.h"
#include "dbus_monitor.h"


int monitor_from_args(int argc, char* argv[], monitor_t* monitor) {
	monitor_t created_monitor;

	if (argc < 1) {
		return E_INVALID_INPUT;
	}else if (strcmp(argv[0], "--file") == 0) {
		int return_value = inotify_monitor_from_args(argc, argv, &created_monitor);
		*monitor = created_monitor;
		return return_value;
	} else if (strcmp(argv[0], "--network") == 0
			   || strcmp(argv[0], "--disks") == 0) {
//		int return_value = dbus_monitor_from_args(argc-1, argv+1, &created_monitor);
//		*monitor = created_monitor;
//		return return_value;
		return E_INVALID_MONITOR_TYPE;
	} else if (strcmp(argv[0], "--power") == 0
			   || strcmp(argv[0], "--bluetooth") == 0) {
//		int return_value = udev_monitor_from_args(argc-1, argv+1, &created_monitor);
//		*monitor = created_monitor;
//		return return_value;
		return E_INVALID_MONITOR_TYPE;
	} else {
		return E_INVALID_INPUT;
	}
	return CALL_SUCCESS;
}

int start_monitor(monitor_t monitor) {
	switch (monitor->type) {
		case MONITOR_TYPE_INOTIFY : {
			return inotify_start(monitor);
		}
		case MONITOR_TYPE_DBUS : {
			//return dbus_start(monitors->inotify);
			return MONITOR_TYPE_INVALID;
		}
		case MONITOR_TYPE_UDEV : {
			//return udev_start(monitors->inotify);
			return MONITOR_TYPE_INVALID;
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
			//return dbus_stop(monitor);
			return MONITOR_TYPE_INVALID;
		}
		case MONITOR_TYPE_UDEV : {
			//return udev_stop(monitor);
			return MONITOR_TYPE_INVALID;
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
		}
		case MONITOR_TYPE_DBUS : {
			//dbus_join(monitor);
		}
		case MONITOR_TYPE_UDEV : {
			//udev_join(monitor);
		}
		default: {}
	}
}

int destroy_monitor(monitor_t monitor) {
	switch (monitor->type) {
		case MONITOR_TYPE_INOTIFY : {
			inotify_monitor_destroy(monitor);
		}
		case MONITOR_TYPE_DBUS : {
			//dbus_monitor_destroy(monitor);
		}
		case MONITOR_TYPE_UDEV : {
			//udev_monitor_destroy(monitor);
		}
		default: {}
	}
}

void monitor_show_usage(monitor_t monitor) {
	switch (monitor->type) {
		case MONITOR_TYPE_INOTIFY : {
			inotify_print_usage();
		}
		case MONITOR_TYPE_DBUS : {
			//return dbus_print_usage();
		}
		case MONITOR_TYPE_UDEV : {
			//return udev_print_usage();
		}
		default: {}
	}
}