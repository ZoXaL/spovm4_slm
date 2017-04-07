#include <stdio.h>

void showUsage() {
	printf("Usage: slm [command] [command_options]\n");
	printf("Available commands: \n");
	printf("\t --fs - monitor file system events\n");
	printf("Use slm [command] -h to get more info about command\n");
}