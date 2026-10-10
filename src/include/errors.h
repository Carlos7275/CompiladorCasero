#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>

void print_usage(const char *program_name);
void print_version(void);
void log_error(const char *phase, const char *fmt, ...);
void log_warning(const char *phase, const char *fmt, ...);
void log_info(const char *phase, const char *fmt, ...);
void log_success(const char *phase, const char *fmt, ...);
void set_diagnostics_json(int enabled);
int diagnostics_json_enabled(void);
void set_diagnostic_file(const char *path);
void report_diagnostic(const char *code, const char *severity, const char *source,
                       int line, int column, const char *message);
void report_diagnosticf(const char *code, const char *severity, const char *source,
                        int line, int column, const char *fmt, ...);
void emit_diagnostics_json(void);

#endif
