#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <memory.h>
#include "errors.h"
#include "logging.h"

#define ERROR_TYPE_LABEL 	"ERROR"
#define INFO_TYPE_LABEL 	"INFO"

static pthread_mutex_t log_mutex;

enum log_type {
	INFO,
	ERROR
};

static void log_common(const char* format, enum log_type, va_list args);

int initialize_logging() {
	if (pthread_mutex_init(&log_mutex, NULL) != 0) return CALL_FAILURE;
	return CALL_SUCCESS;
}

int destroy_logging() {
	if (pthread_mutex_destroy(&log_mutex) != 0) return CALL_FAILURE;
	return CALL_SUCCESS;
}

void log_info(const char* format, ...) {
	va_list args;
	va_start(args, format);
	log_common(format, INFO, args);
	va_end(args);
}

void log_error(const char* format, ...) {
	va_list args;
	va_start(args, format);
	log_common(format, ERROR, args);
	va_end(args);
}

static void log_common(const char* format, enum log_type log_type, va_list args) {
	FILE* log_file = stdout;
	char* log_label;
	switch (log_type) {
		case INFO: {
			log_file = stdout;
			log_label = INFO_TYPE_LABEL;
			break;
		}
		default:
		case ERROR: {
			log_file = stderr;
			log_label = ERROR_TYPE_LABEL;
		}
	}
	time_t rawtime;
	struct tm* timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	pthread_mutex_lock(&log_mutex);
	fprintf(log_file, "%s [%s]: ", strtok(asctime(timeinfo), "\n"), log_label);
	vfprintf(log_file, format, args);
	fprintf(log_file, "\n");
	fflush(log_file);
	pthread_mutex_unlock(&log_mutex);
}
