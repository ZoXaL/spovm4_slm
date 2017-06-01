#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>
#include <errors.h>
#include <monitors/monitor.h>
#include <errno.h>
#include <fcntl.h>

#include "logging.h"
#include "glib.h"

#define COMMAND_BUFFER_SIZE 1024

static char* log_file_name = NULL;
static char* conf_file_name = NULL;
static char* error_file_name = NULL;
static char* pid_file_name = NULL;

static FILE* log_file = NULL;
static FILE* error_file = NULL;

static int running = 1;
static GArray* monitors_array = NULL;
static int monitors_array_size = 0;

int apply_configs() {
	FILE* conf_file = fopen(conf_file_name, "r");
	if (conf_file == NULL) {
		log_error("cannot open config file %", conf_file_name);
		return E_OPEN_CONFIGS;
	}
	int parsing_line = 1;

	char argv_string[COMMAND_BUFFER_SIZE];
	char argv_string_copy[COMMAND_BUFFER_SIZE];
	while(fgets(argv_string, COMMAND_BUFFER_SIZE, conf_file) != NULL) {
		if (argv_string[strlen(argv_string)-1] == '\n') {
			argv_string[strlen(argv_string)-1] = '\0';
		}
		strcpy(argv_string_copy, argv_string);
		char** argv = NULL;
		int argc = 0;
		char* token;
		token = strtok (argv_string, " ");
		if (token != NULL) {
			argc++;
			argv = (char**)malloc(sizeof(char*)*argc);
			if (argv == NULL) {
				fclose(conf_file);
				return E_OUT_OF_MEMORY;
			}
			argv[argc-1] = token;

			token = strtok (NULL, " ");
			while (token != NULL) {
				argc++;
				argv = (char**)realloc(argv, sizeof(char*)*argc);
				if (argv == NULL) {
					fclose(conf_file);
					return E_OUT_OF_MEMORY;
				}
				argv[argc-1] = token;
				token = strtok (NULL, " ");
			}
		}
		monitor_t new_monitor;
		log_info("Starting monitor %s", argv[0]);
		if (monitor_from_args(argc, argv, &new_monitor) == CALL_SUCCESS) {
			g_array_append_val(monitors_array, new_monitor);
			monitors_array_size++;
			start_monitor(new_monitor);
		} else {
			log_error("Cannot parse line %d: %s", parsing_line, argv_string_copy);
		}
		parsing_line++;
	}

	fclose(conf_file);

	return CALL_SUCCESS;
}

static void kill_all_monitors() {
	for (int i = 0; i < monitors_array_size; i++) {
		monitor_t monitor = g_array_index(monitors_array, monitor_t, i);
		stop_monitor(monitor);
	}
	for (int i = 0; i < monitors_array_size; i++) {
		monitor_t monitor = g_array_index(monitors_array, monitor_t, i);
		join_monitor(monitor);
		destroy_monitor(monitor);
	}
	g_array_free(monitors_array, TRUE);
	monitors_array_size = 0;
}

int reload_configs() {
	kill_all_monitors();
	return apply_configs();
}


void signal_handler(int signal) {
	if (signal == SIGINT) {
		kill_all_monitors();
		running = 0;
	} else if (signal == SIGHUP) {
		reload_configs();
	}
}

int daemonize() {
	pid_t pid, sid;
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		sleep(1);
		exit(EXIT_SUCCESS);
	}
	umask(0);
	sid = setsid();
	if (sid < 0) {
		printf("no sid:(: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if ((chdir("/")) < 0) {
		printf("cant go to /\n");
		exit(EXIT_FAILURE);
	}
	for (int fd = 0; fd <= sysconf(_SC_OPEN_MAX); fd++) {
		close(fd);
	}
	log_file = fopen(log_file_name, "a");
	if (log_file == NULL || dup2(fileno(log_file), fileno(stdout)) == -1) {
		printf("can not open logs\n");
		return E_OPEN_LOGS;
	}

	error_file = fopen(error_file_name, "a");
	if (error_file == NULL || dup2(fileno(error_file), fileno(stderr)) == -1) {
		log_error("cannot open error file %", error_file_name);
		error_file = log_file;
		dup2(fileno(stderr), fileno(error_file));
	}

	FILE* pid_file = fopen(pid_file_name, "w");
	if (pid_file == NULL) {
		exit(EXIT_FAILURE);
	}
	if (lockf(fileno(pid_file), F_TLOCK, 0) < 0) {
		exit(EXIT_FAILURE);
	}
	fprintf(pid_file, "%d", getpid());
	fclose(pid_file);

	fflush(stdout);
	fflush(stderr);
	return CALL_SUCCESS;
}

int main(int argc, char *argv[]) {
	static struct option options[] = {
		{"config-file", required_argument, 0, 'c'},
		{"log-file", required_argument, 0, 'l'},
		{"pid-file", required_argument, 0, 'p'},
		{"error-file", optional_argument, 0, 'e'},
		{NULL, 0, 0, 0}
	};

	int current_option = -1;
	int c;
	initialize_logging();
	while ((c = getopt_long(argc, argv, "+l:c:p:e", options, &current_option)) != -1) {
		switch (c) {
			case 'c': {
				conf_file_name = optarg;
				break;
			}
			case 'l': {
				log_file_name = optarg;
				break;
			}
			case 'e': {
				error_file_name = optarg;
				break;
			}
			case 'p': {
				log_info("pid file: %s", optarg);
				pid_file_name = optarg;
				break;
			}
			case '?':	{
				// unknown flag, nothing to do
				break;
			}
			case ':': 	// no required args
			default: {
				return EXIT_FAILURE;
			}
		}
	}
	log_info("pid file: %s", pid_file_name);
	if (conf_file_name == NULL || log_file_name == NULL) {
		return EXIT_FAILURE;
	}

	struct sigaction systemctl_sigaction;
	systemctl_sigaction.sa_handler = signal_handler;
	sigaction(SIGINT, &systemctl_sigaction, NULL);
	sigaction(SIGHUP, &systemctl_sigaction, NULL);

	monitors_array = g_array_new(FALSE, FALSE, sizeof(monitor_t));
	log_info("before daemonize");
	int call_result = daemonize();

	if (call_result != EXIT_SUCCESS) {
		return call_result;
	}
	log_info("after daemonize");
	if (call_result != CALL_SUCCESS) {
		return call_result;
	}

	call_result = apply_configs();
	if (call_result != EXIT_SUCCESS) {
		fclose(log_file);
		if (error_file != log_file) fclose(error_file);
		return call_result;
	}

	while(running) pause();

	log_info("daemon is dead");
	destroy_logging();
	return EXIT_SUCCESS;
}
