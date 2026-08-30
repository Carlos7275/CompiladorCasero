#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>

void print_usage(const char *program_name);
void print_version(void);
void log_error(const char *phase, const char *fmt, ...);
void log_warning(const char *phase, const char *fmt, ...);
void log_info(const char *phase, const char *fmt, ...);
void log_success(const char *phase, const char *fmt, ...);

#endif
