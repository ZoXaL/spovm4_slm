#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <sys/inotify.h>
#include <monitors/monitor.h>
#include <signal.h>
#include <sys/poll.h>
#include <logging/logging.h>
#include <errno.h>
#include "errors.h"

#define MODE_OPEN 'o'
#define MODE_WRITE 'w'
#define MODE_CLOSE 'c'
#define MODE_MOVE 'm'
#define MODE_DELETE 'd'
#define MODES_COUNT 5

#define MAX_FILE_NAME_LENGTH 256

#define EVENTS_COUNT_BUFFER 20
#define EVENTS_BYTE_BUFFER sizeof(struct inotify_event)*EVENTS_COUNT_BUFFER

static void* monitoring_thread(void* inotify_monitor_ptr);
static void mark_dead(monitor_t monitor);
static uint32_t mask_from_mode(char* mode);

int inotify_monitor_from_args(int argc, char* argv[], monitor_t* monitor) {
//	if (argc < 2) return E_INVALID_MONITOR_ARGUMENT;
	*monitor = (monitor_t)malloc(sizeof(struct monitor_t));
	if (*monitor == NULL) {
		log_error("malloc: %s", strerror(errno));
		return E_OUT_OF_MEMORY;
	}
	(*monitor)->type = MONITOR_TYPE_INOTIFY;

	(*monitor)->inotify = (inotify_monitor_t)malloc(sizeof(struct inotify_monitor));
	if ((*monitor)->inotify == NULL) {
		log_error("malloc: %s", strerror(errno));
		free(monitor);
		return E_OUT_OF_MEMORY;
	}
	inotify_monitor_t inotify_monitor = (*monitor)->inotify;
	inotify_monitor->mode = (char*)malloc(MODES_COUNT*sizeof(char));
	if (inotify_monitor->mode == NULL) {
		log_error("malloc: %s", strerror(errno));
		free(inotify_monitor);
		free((*monitor));
		return E_OUT_OF_MEMORY;
	}
	memset(inotify_monitor->mode, '\0', MODES_COUNT);
	if (pthread_mutex_init(&(inotify_monitor->state_mutex), NULL) != 0) {
		free(inotify_monitor->mode);
		free(inotify_monitor);
		free((*monitor));
		return CALL_FAILURE;
	};

	char argument_string[MODES_COUNT+1];
	argument_string[0] = '+';	// sets POSIX parsing mode: parse until first no-arg
	argument_string[1] = MODE_OPEN;
	argument_string[2] = MODE_WRITE;
	argument_string[3] = MODE_CLOSE;
	argument_string[4] = MODE_MOVE;
	argument_string[5] = MODE_DELETE;

	opterr = 0;
	optind = 1;
	int c;
	while ((c = getopt(argc, argv, argument_string)) != -1) {
		switch (c) {
			case MODE_OPEN: {
				inotify_monitor->mode[strlen(inotify_monitor->mode)] = MODE_OPEN;
				break;
			}
			case MODE_WRITE: {
				inotify_monitor->mode[strlen(inotify_monitor->mode)] = MODE_WRITE;
				break;
			}
			case MODE_CLOSE: {
				inotify_monitor->mode[strlen(inotify_monitor->mode)] = MODE_CLOSE;
				break;
			}
			case MODE_MOVE: {
				inotify_monitor->mode[strlen(inotify_monitor->mode)] = MODE_MOVE;
				break;
			}
			case MODE_DELETE: {
				inotify_monitor->mode[strlen(inotify_monitor->mode)] = MODE_DELETE;
				break;
			}
			case '?':
			default: {
				free(inotify_monitor->mode);
				free(inotify_monitor);
				free((*monitor));
				return E_INVALID_MONITOR_ARGUMENT;
			}
		}
	}
	if (argc != optind+1) {
		free(inotify_monitor->mode);
		free(inotify_monitor);
		free((*monitor));
		return E_INVALID_MONITOR_ARGUMENT;
	}
	inotify_monitor->inotify_file_descriptor = inotify_init();
	if (inotify_monitor->inotify_file_descriptor < 0) {
		log_error("inotify_init: %s", strerror(errno));
		free(inotify_monitor->mode);
		free(inotify_monitor);
		free((*monitor));
		return CALL_FAILURE;
	}
	inotify_monitor->file_path = (char*)malloc(sizeof(char)*MAX_FILE_NAME_LENGTH);
	if (inotify_monitor->file_path == NULL) {
		log_error("malloc: %s", strerror(errno));
		close(inotify_monitor->inotify_file_descriptor);
		free(inotify_monitor->mode);
		free(inotify_monitor);
		free((*monitor));
		return E_OUT_OF_MEMORY;
	}
	strcpy(inotify_monitor->file_path, argv[optind]);
	pthread_mutex_lock(&(inotify_monitor->state_mutex));
	(*monitor)->state = MONITOR_STATE_INITIALIZED;
	pthread_mutex_unlock(&(inotify_monitor->state_mutex));
	return CALL_SUCCESS;
}

int inotify_start(monitor_t monitor) {
	pthread_mutex_lock(&(monitor->inotify->state_mutex));
	if (monitor->state != MONITOR_STATE_INITIALIZED) {
		log_error("cannot start monitor wich is not in \'initialized\' state");
		pthread_mutex_unlock(&(monitor->inotify->state_mutex));
		return E_MONITOR_INVALID_STATE;
	}
	if (pthread_create(&(monitor->inotify->thread),
		NULL, monitoring_thread, monitor) != 0) {
		log_error("inotify pthread_create: %s", strerror(errno));
		pthread_mutex_unlock(&(monitor->inotify->state_mutex));
		return CALL_FAILURE;
	}
	monitor->state = MONITOR_STATE_RUNNING;
	pthread_mutex_unlock(&(monitor->inotify->state_mutex));
	return CALL_SUCCESS;
}

int inotify_stop(monitor_t monitor) {
	pthread_mutex_lock(&(monitor->inotify->state_mutex));
	if (monitor->state != MONITOR_STATE_RUNNING) {
		pthread_mutex_unlock(&(monitor->inotify->state_mutex));
		return E_MONITOR_INVALID_STATE;
	}
	monitor->state = MONITOR_STATE_DYING;
	pthread_mutex_unlock(&(monitor->inotify->state_mutex));
	return CALL_SUCCESS;
}

void inotify_join(monitor_t monitor) {
	pthread_join(monitor->inotify->thread, NULL);
	log_info("inotify monitor was stopped");
}

void inotify_print_usage() {
	printf("%s%s%s%s%s%s%s%s%s",
		"Aimed to monitors file system events\n",
		"Usage: slm --file [watch_options] [path_to_file]\n",
		"\t path_to_file - full path to monitoring file\n",
		"\t watch_options: \n",
		"\t\t -o - file opened \n",
		"\t\t -w - file changed \n",
		"\t\t -c - file closed \n",
		"\t\t -d - file deleted \n",
		"\t\t -m - file moved \n");
}

int inotify_monitor_destroy(monitor_t monitor) {
	if (monitor->state != MONITOR_STATE_DEAD) {
		return E_MONITOR_INVALID_STATE;
	}
	log_info("inotify monitor %s was killed", monitor->inotify->file_path);
	inotify_monitor_t inotify_monitor = monitor->inotify;
	close(inotify_monitor->inotify_file_descriptor);
	free(inotify_monitor->mode);
	free(inotify_monitor);
	free(monitor);
	return CALL_SUCCESS;
}

static void* monitoring_thread(void* monitor_ptr) {
	monitor_t monitor = (monitor_t)monitor_ptr;
	inotify_monitor_t inotify_monitor = monitor->inotify;

	log_info("inotify monitor %s created", monitor->inotify->file_path);

	sigset_t blocking_mask;
	if (sigfillset(&blocking_mask) != 0) {
		log_error("cannot create sigmask for inotify thread, exit");
		pthread_mutex_lock(&(inotify_monitor->state_mutex));
		monitor->state = MONITOR_STATE_DEAD;
		pthread_mutex_unlock(&(inotify_monitor->state_mutex));
		return NULL;
	}
	if (pthread_sigmask(SIG_BLOCK, &blocking_mask, NULL) != 0) {
		log_error("cannot mask inotify thread signals, exit");
		pthread_mutex_lock(&(inotify_monitor->state_mutex));
		monitor->state = MONITOR_STATE_DEAD;
		pthread_mutex_unlock(&(inotify_monitor->state_mutex));
		return NULL;
	}
	if(inotify_add_watch(inotify_monitor->inotify_file_descriptor,
						 inotify_monitor->file_path,
						mask_from_mode(inotify_monitor->mode)) == -1) {
		log_error("inotify add watch for %s: %s",
				  inotify_monitor->file_path, strerror(errno));
		monitor->state = MONITOR_STATE_DEAD;
		return NULL;
	}

	char buf[EVENTS_BYTE_BUFFER];
	char* eventPtr;
	struct inotify_event* event;
	struct pollfd inotify_poll_fd;
	inotify_poll_fd.fd = inotify_monitor->inotify_file_descriptor;
	inotify_poll_fd.events = POLLIN;
	inotify_poll_fd.revents = 0;

	while(1) {
		do {
			pthread_mutex_lock(&(inotify_monitor->state_mutex));
			if (monitor->state == MONITOR_STATE_DYING) {
				pthread_mutex_unlock(&(inotify_monitor->state_mutex));
				mark_dead(monitor);
				return NULL;
			}
			pthread_mutex_unlock(&(inotify_monitor->state_mutex));
		    poll(&inotify_poll_fd, 1, 500);
		} while (inotify_poll_fd.revents != POLLIN);

		ssize_t bytesRead = read(inotify_monitor->inotify_file_descriptor, buf, EVENTS_BYTE_BUFFER);

		for (eventPtr = buf; eventPtr < buf + bytesRead;
			eventPtr += sizeof(struct inotify_event) + event->len) {

			event = (struct inotify_event*)eventPtr;

			if (event->mask & IN_OPEN) {
				log_info("file %s was opened", inotify_monitor->file_path);
			}
			if (event->mask & IN_CLOSE) {
				log_info("file %s was closed", inotify_monitor->file_path);
			}
			if (event->mask & IN_CLOSE_WRITE) {
				log_info("file %s was changed", inotify_monitor->file_path);
			}
			if (event->mask & IN_MOVE_SELF) {
				log_info("file %s was moved", inotify_monitor->file_path);
				mark_dead(monitor);
				return NULL;
			}
			if (event->mask & IN_DELETE_SELF) {
				log_info("file %s was deleted", inotify_monitor->file_path);
				mark_dead(monitor);
				return NULL;
			}
		}
	}
	return NULL;
}

static void mark_dead(monitor_t monitor) {
	close(monitor->inotify->inotify_file_descriptor);
	pthread_mutex_lock(&(monitor->inotify->state_mutex));
	monitor->state = MONITOR_STATE_DEAD;
	pthread_mutex_unlock(&(monitor->inotify->state_mutex));
	pthread_mutex_destroy(&(monitor->inotify->state_mutex));
}

static uint32_t mask_from_mode(char* mode) {
	uint32_t mask = 0;
	if(strchr(mode, MODE_OPEN) != NULL) {
		mask |= IN_OPEN;
	}
	if(strchr(mode, MODE_WRITE) != NULL) {
		mask |= IN_MODIFY;
	}
	if(strchr(mode, MODE_CLOSE) != NULL) {
		mask |= IN_CLOSE;
	}
	if(strchr(mode, MODE_DELETE) != NULL) {
		mask |= IN_DELETE_SELF;
	}
	if(strchr(mode, MODE_MOVE) != NULL) {
		mask |= IN_MOVE_SELF;
	}
	return mask;
}