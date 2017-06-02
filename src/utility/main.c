#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <logging/logging.h>
#include "monitor.h"
#include "errors.h"


monitor_t main_monitor;

void killHandler(int signal) {
    if (stop_monitor(main_monitor) == CALL_SUCCESS) {
        join_monitor(main_monitor);
    }
    printf("slm was gracefully stopped\n");
}

void showUsage() {
	printf("Usage: slm [command] [command_options]\n");
	printf("Available commands: \n");
	printf("\t --file \t- monitors file events\n");
	printf("\t --network \t- monitors network events\n");
	printf("\t --disks \t- monitors disks events\n");
	printf("\t --power \t- monitors power supply events\n");
	printf("\t --bluetooth \t- monitors bluetooth events\n");
	printf("Use slm [command] -h to get more info about each command\n");
}

int main(int argc, char* argv[]) {
    struct sigaction kill_action;
    kill_action.sa_handler = killHandler;
    sigaction(SIGINT, &kill_action, NULL);

	int parse_result = monitor_from_args(argc-1, (argv+1), &main_monitor);
	if (parse_result == E_INVALID_INPUT) {
		showUsage();
		return EXIT_FAILURE;
	} else if (parse_result != CALL_SUCCESS) {
		return EXIT_FAILURE;
	}
	if (initialize_logging() != CALL_SUCCESS) {
		printf("can not initialize logging module, exit\n");
		destroy_monitor(main_monitor);
		return EXIT_FAILURE;
	}
	if (start_monitor(main_monitor) != CALL_SUCCESS) {
		printf("can not start monitor, exit\n");
		destroy_monitor(main_monitor);
        return EXIT_FAILURE;
    }
    join_monitor(main_monitor);
	destroy_monitor(main_monitor);
	destroy_logging();
	return EXIT_SUCCESS;
}