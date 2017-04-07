#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/inotify.h>
#include "filesystem.h"

#define MAX_NAME_SIZE 200
#define EVENTS_COUNT_BUFFER 20
#define EVENTS_BYTE_BUFFER sizeof(struct inotify_event)*EVENTS_COUNT_BUFFER

// is static good or namespace prefix is better?

static int inotifyFileDescriptor;

static void gracefullShutdown(int signal) {
	close(inotifyFileDescriptor);
	printf("slm was gracefully closed\n");
	exit(0);
}

static int watchEvents();

static void test() {
	printf("FS test\n");
}

static void showUsage() {
	printf("Aimed to monitor file system events\n");
	printf("Usage: slm --fs [command_options] [path]\n");
	printf("\t command_options - auxiliary options\n");
	printf("\t path - full path to dir of file to monitor\n");
	printf("Available --fs options: \n");
	printf("\t -r - watch dir recursive (in development)\n");
	printf("\t -s - specify script to call on event (in development)\n");
}

static int proceed(int argc, char* argv[]) {
	if (argc < 1) {
		printf("Illegal usage, use 'slm --fs -h' for help\n");
		return EINVAL;
	}
	if (strcmp(argv[0], "-h") == 0) {
		showUsage();
		return 0;
	}
	return watchEvents(argc, argv);
}

static int watchEvents(int argc, char* argv[]) {
	int lastError, i;

	inotifyFileDescriptor = inotify_init();	
	if (inotifyFileDescriptor < 0) {
		lastError = errno;
		perror("inotify_init");
		return lastError;
	}
	int* watchDescriptors = malloc(argc*sizeof(int));
	if (watchDescriptors == NULL) {
		lastError = errno;
		perror("malloc");
		return lastError;
	}
	for (i = 0; i < argc; i++) {
		watchDescriptors[i] = inotify_add_watch(inotifyFileDescriptor, 
											argv[i], IN_ALL_EVENTS);
        if (watchDescriptors[i] == -1) {
        	lastError = errno;
            fprintf(stderr, "Cannot watch '%s'\n", argv[i]);
            perror("inotify_add_watch");
            exit(lastError);
        }
	}
	char buf[EVENTS_BYTE_BUFFER];
	char* eventPtr;
	struct inotify_event* event;
	char fullName[MAX_NAME_SIZE];
	char* type;
	// infinite watch loop
	while(1) {
		memset(fullName, '\0', MAX_NAME_SIZE);
		ssize_t bytesRead = read(inotifyFileDescriptor, 
								buf, EVENTS_BYTE_BUFFER);
		if (errno == EINTR) {
			printf("Interrupter during waiting for file events\n");
			gracefullShutdown(EINTR);
		}
		for (eventPtr = buf; eventPtr < buf + bytesRead;
			eventPtr += sizeof(struct inotify_event) + event->len) {

			event = (struct inotify_event *) eventPtr;
			
			if (event->mask & IN_ISDIR) {
				type = "directory";
			} else {
				type = "file";
			}
			for (i = 0; i < argc; ++i) {
				if (watchDescriptors[i] == event->wd) {
					strcpy(fullName, argv[i]);
					strcat(fullName, "/");
				}
			}
			if (event->len > 0) {
				strcat(fullName, event->name);
			}

			if (event->mask & IN_OPEN) 
				printf("%s %s was opened\n", type, fullName);
			if (event->mask & IN_CLOSE)
				printf("%s %s was closed\n", type, fullName);
			if (event->mask & IN_CREATE) 
				printf("%s %s was created\n", type, fullName);
			if (event->mask & IN_MODIFY) 
				printf("%s %s was modified\n", type, fullName);
			if (event->mask & IN_DELETE) 
				printf("%s %s was deleted\n", type, fullName);
		}
	}
	return 0;
}



const struct FileSystem FS = {
	.test = test,
	.showUsage = showUsage,
	.proceed = proceed,
	.gracefullShutdown = gracefullShutdown
};  