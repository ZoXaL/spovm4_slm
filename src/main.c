#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include "filesystem.h" 
#include "help.h"

int main(int argc, char* argv[]) {
	if (argc < 2) {
		showUsage();
		return EINVAL;
	} 
	if (strcmp(argv[1], "--fs") == 0) {
		struct sigaction intAction;
		intAction.sa_handler = FS.gracefullShutdown;
		sigaction(SIGINT, &intAction, NULL);

		return FS.proceed(argc-2, (argv+2));
	} else if (strcmp(argv[1], "-h") == 0) {
		showUsage();
		return 0;
	} else {		
		return EINVAL;
	}
	return 0;
}