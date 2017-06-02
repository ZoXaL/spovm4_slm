#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <monitors/monitor.h>
#include <logging/logging.h>
#include <errno.h>
#include <sys/epoll.h>
#include <libudev.h>
#include "errors.h"

#define EVENTS_COUNT_BUFFER 20


static void* monitoring_thread(void* udev_monitor_ptr);
static void mark_dead(monitor_t monitor);

int udev_monitor_from_args(int argc, char* argv[], monitor_t* monitor) {
	if (argc > 1) return E_INVALID_MONITOR_ARGUMENT;
	printf("her\n");
	*monitor = (monitor_t)malloc(sizeof(struct monitor_t));
	if (*monitor == NULL) {
		log_error("malloc: %s", strerror(errno));
		return E_OUT_OF_MEMORY;
	}
	(*monitor)->type = MONITOR_TYPE_UDEV;

	(*monitor)->udev = (udev_monitor_t)malloc(sizeof(struct z_udev_monitor));
	if ((*monitor)->udev == NULL) {
		log_error("malloc: %s", strerror(errno));
		free(monitor);
		return E_OUT_OF_MEMORY;
	}
	udev_monitor_t udev_monitor = (*monitor)->udev;

	if (pthread_mutex_init(&(udev_monitor->state_mutex), NULL) != 0) {
		free(udev_monitor);
		free((*monitor));
		return CALL_FAILURE;
	};

	if(strcmp(argv[0], "--power") == 0) {
		udev_monitor->type = UDEV_MONITOR_TYPE_POWER;
	} else if(strcmp(argv[0], "--bluetooth") == 0) {
		udev_monitor->type = UDEV_MONITOR_TYPE_BLUETOOTH;
	} else {
		log_error("unknow udev monitor type %s", argv[0]);
		free(udev_monitor);
		free((*monitor));
		return E_INVALID_MONITOR_ARGUMENT;
	}

	pthread_mutex_lock(&(udev_monitor->state_mutex));
	(*monitor)->state = MONITOR_STATE_INITIALIZED;
	pthread_mutex_unlock(&(udev_monitor->state_mutex));
	return CALL_SUCCESS;
}

int udev_start(monitor_t monitor) {
	pthread_mutex_lock(&(monitor->udev->state_mutex));
	if (monitor->state != MONITOR_STATE_INITIALIZED) {
		log_error("cannot start monitor wich is not in \'initialized\' state");
		pthread_mutex_unlock(&(monitor->udev->state_mutex));
		return E_MONITOR_INVALID_STATE;
	}
	if (pthread_create(&(monitor->udev->thread),
					   NULL, monitoring_thread, monitor) != 0) {
		log_error("udev pthread_create: %s", strerror(errno));
		pthread_mutex_unlock(&(monitor->udev->state_mutex));
		return CALL_FAILURE;
	}
	monitor->state = MONITOR_STATE_RUNNING;
	pthread_mutex_unlock(&(monitor->udev->state_mutex));
	return CALL_SUCCESS;
}

int udev_stop(monitor_t monitor) {
	pthread_mutex_lock(&(monitor->udev->state_mutex));
	if (monitor->state != MONITOR_STATE_RUNNING) {
		pthread_mutex_unlock(&(monitor->udev->state_mutex));
		return E_MONITOR_INVALID_STATE;
	}
	monitor->state = MONITOR_STATE_DYING;
	pthread_mutex_unlock(&(monitor->udev->state_mutex));
	return CALL_SUCCESS;
}

void udev_join(monitor_t monitor) {
	pthread_join(monitor->udev->thread, NULL);
	log_info("udev monitor was stopped");
}

void udev_print_usage(int type) {
	if (type == UDEV_MONITOR_TYPE_BLUETOOTH) {
		printf("%s%s",
			   "Aimed to monitor blutooth events (bluetooth on/off)\n",
			   "Usage: slm --bluetooth\n");
	} else {
		printf("%s%s",
			   "Aimed to monitor power supply events (power on/off)\n",
			   "Usage: slm --power\n");
	}
}

int udev_monitor_destroy(monitor_t monitor) {
	if (monitor->state == MONITOR_STATE_RUNNING
		|| monitor->state == MONITOR_STATE_DYING) {
		return E_MONITOR_INVALID_STATE;
	}
	log_info("udev monitor was killed");
	udev_monitor_t udev_monitor = monitor->udev;
	free(udev_monitor);
	free(monitor);
	return CALL_SUCCESS;
}

static void* monitoring_thread(void* monitor_ptr) {
	monitor_t monitor = (monitor_t)monitor_ptr;
	udev_monitor_t udev_monitor = monitor->udev;

	log_info("udev monitor was created");

	sigset_t blocking_mask;
	if (sigfillset(&blocking_mask) != 0) {
		log_error("cannot create sigmask for udev thread, exit");
		pthread_mutex_lock(&(udev_monitor->state_mutex));
		monitor->state = MONITOR_STATE_DEAD;
		pthread_mutex_unlock(&(udev_monitor->state_mutex));
		return NULL;
	}
	if (pthread_sigmask(SIG_BLOCK, &blocking_mask, NULL) != 0) {
		log_error("cannot mask udev thread signals, exit");
		pthread_mutex_lock(&(udev_monitor->state_mutex));
		monitor->state = MONITOR_STATE_DEAD;
		pthread_mutex_unlock(&(udev_monitor->state_mutex));
		return NULL;
	}

	struct udev* udev;
	struct udev_monitor* _udev_monitor;

	udev = udev_new();
	if (!udev) {
		log_error("can not create udev struct, exit monitor\n");
		mark_dead(monitor);
		return NULL;
	}

	_udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
	if(udev_monitor->type == UDEV_MONITOR_TYPE_POWER) {
		udev_monitor_filter_add_match_subsystem_devtype(_udev_monitor, "power_supply", NULL);
	} else if(udev_monitor->type == UDEV_MONITOR_TYPE_BLUETOOTH) {
		udev_monitor_filter_add_match_subsystem_devtype(_udev_monitor, "bluetooth", NULL);
	}
	udev_monitor_enable_receiving(_udev_monitor);

	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		log_error("can not create epoll fd, udev monitor exit\n");
		mark_dead(monitor);
		return NULL;
	}

	int udev_fd = udev_monitor_get_fd(_udev_monitor);
	struct epoll_event ep_udev = {0};
	ep_udev.events = EPOLLIN;
	ep_udev.data.fd = udev_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udev_fd, &ep_udev) < 0) {
		log_error("can not add event to epoll, udev monitor exit\n");
		mark_dead(monitor);
		return NULL;
	}

	while (1) {
		int events_count;
		struct epoll_event events_buffer[EVENTS_COUNT_BUFFER];

		events_count = epoll_wait(epoll_fd, events_buffer, EVENTS_COUNT_BUFFER, 500);
		while (events_count == 0) {
			pthread_mutex_lock(&(udev_monitor->state_mutex));
			if (monitor->state == MONITOR_STATE_DYING) {
				pthread_mutex_unlock(&(udev_monitor->state_mutex));
				mark_dead(monitor);
				close(epoll_fd);
				close(udev_fd);
				return NULL;
			}
			pthread_mutex_unlock(&(udev_monitor->state_mutex));
			events_count = epoll_wait(epoll_fd, events_buffer, EVENTS_COUNT_BUFFER, 500);
		}

		for (int i = 0; i < events_count; i++) {
			if (events_buffer[i].data.fd == udev_fd && events_buffer[i].events & EPOLLIN) {
				struct udev_device* power_device = udev_monitor_receive_device(_udev_monitor);
				if (power_device == NULL)
					continue;
				if(udev_monitor->type == UDEV_MONITOR_TYPE_POWER) {
					const char* status = udev_device_get_property_value(power_device, "POWER_SUPPLY_STATUS");
					if (status != NULL) printf("%s\n", status);
					if (status != NULL && strcmp(status, "Discharging") == 0) {
						log_info("power supply off");
					} else if (status != NULL) {
						log_info("power supply on");
					}
				} else if(udev_monitor->type == UDEV_MONITOR_TYPE_BLUETOOTH) {
					if (strcmp(udev_device_get_action(power_device), "add") == 0) {
						log_info("bluetooth on");
					} else if (strcmp(udev_device_get_action(power_device), "remove") == 0) {
						log_info("bluetooth off");
					}
				}

				udev_unref(udev);
			}
		}
	}
	return NULL;
}

static void mark_dead(monitor_t monitor) {
	pthread_mutex_lock(&(monitor->udev->state_mutex));
	monitor->state = MONITOR_STATE_DEAD;
	pthread_mutex_unlock(&(monitor->udev->state_mutex));
}
