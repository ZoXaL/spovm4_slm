#ifndef LOGGING_H
#define LOGGING_H

int initialize_logging();
int destroy_logging();
void log_info(const char* format, ...);
void log_error(const char* format, ...);

#endif