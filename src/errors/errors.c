#include "errors.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Diagnostic
{
    char code[32];
    char severity[16];
    char source[32];
    char message[512];
    int line;
    int column;
    struct Diagnostic *next;
} Diagnostic;

static Diagnostic *diagnostics_head;
static Diagnostic *diagnostics_tail;
static int json_diagnostics_enabled;
static const char *diagnostic_file;

void set_diagnostics_json(int enabled)
{
    json_diagnostics_enabled = enabled;
}

int diagnostics_json_enabled(void)
{
    return json_diagnostics_enabled;
}

void set_diagnostic_file(const char *path)
{
    diagnostic_file = path;
}

void report_diagnostic(const char *code, const char *severity, const char *source,
                       int line, int column, const char *message)
{
    if (!json_diagnostics_enabled)
        return;

    Diagnostic *diagnostic = calloc(1, sizeof(*diagnostic));
    if (!diagnostic)
    {
        perror("No se pudo reservar memoria para el diagnostico");
        exit(EXIT_FAILURE);
    }

    snprintf(diagnostic->code, sizeof(diagnostic->code), "%s", code ? code : "MX0000");
    snprintf(diagnostic->severity, sizeof(diagnostic->severity), "%s",
             severity ? severity : "error");
    snprintf(diagnostic->source, sizeof(diagnostic->source), "%s",
             source ? source : "compiler");
    snprintf(diagnostic->message, sizeof(diagnostic->message), "%s",
             message ? message : "Error sin descripcion");
    diagnostic->line = line > 0 ? line - 1 : 0;
    diagnostic->column = column > 0 ? column : 0;

    if (diagnostics_tail)
        diagnostics_tail->next = diagnostic;
    else
        diagnostics_head = diagnostic;
    diagnostics_tail = diagnostic;
}

void report_diagnosticf(const char *code, const char *severity, const char *source,
                        int line, int column, const char *fmt, ...)
{
    char message[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    report_diagnostic(code, severity, source, line, column, message);
}

static void json_string(const char *value)
{
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)(value ? value : ""); *p; ++p)
    {
        switch (*p)
        {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\b': fputs("\\b", stdout); break;
        case '\f': fputs("\\f", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20)
                fprintf(stdout, "\\u%04x", *p);
            else
                fputc(*p, stdout);
        }
    }
    fputc('"', stdout);
}

void emit_diagnostics_json(void)
{
    fputs("{\"file\":", stdout);
    json_string(diagnostic_file);
    fputs(",\"diagnostics\":[", stdout);

    int first = 1;
    for (Diagnostic *diagnostic = diagnostics_head; diagnostic; diagnostic = diagnostic->next)
    {
        if (!first)
            fputc(',', stdout);
        first = 0;
        int severity_number = strcmp(diagnostic->severity, "error") == 0 ? 1 :
                              strcmp(diagnostic->severity, "warning") == 0 ? 2 :
                              strcmp(diagnostic->severity, "information") == 0 ? 3 : 4;
        fprintf(stdout,
                "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                "\"end\":{\"line\":%d,\"character\":%d}},"
                "\"severity\":%d,\"code\":",
                diagnostic->line, diagnostic->column,
                diagnostic->line, diagnostic->column + 1,
                severity_number);
        json_string(diagnostic->code);
        fputs(",\"source\":", stdout);
        json_string(diagnostic->source);
        fputs(",\"message\":", stdout);
        json_string(diagnostic->message);
        fputs("}", stdout);
    }
    fputs("]}\n", stdout);
    fflush(stdout);

    Diagnostic *diagnostic = diagnostics_head;
    while (diagnostic)
    {
        Diagnostic *next = diagnostic->next;
        free(diagnostic);
        diagnostic = next;
    }
    diagnostics_head = diagnostics_tail = NULL;
}

/**
 * Muestra la ayuda de uso del compilador.
 *
 * @param program_name Nombre del ejecutable que se está invocando.
 */
void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Uso: %s <archivo.mx> [salida] [opciones]\n"
            "\n"
            "Opciones:\n"
            "  -asm          conserva el archivo .asm generado\n"
            "  -debug        muestra AST, tabla de símbolos, IR y ASM\n"
            "  -debugger     pausa en cada sentencia; comandos s, c, b LINEA, q\n"
            "  --diagnostics=json emite diagnosticos estructurados para IDEs\n"
            "  -no-run       compila sin ejecutar el ejecutable\n"
            "  -version      muestra la versión del compilador\n"
            "  -help         muestra esta ayuda\n",
            program_name ? program_name : "compilador");
}

/**
 * Imprime la versión del compilador y el modo de operación.
 */
void print_version(void)
{
    printf("Compilador Casero v1.0.0\n");
    printf("Modo: compilador multiplataforma\n");
}

/**
 * Genera el prefijo estándar para mensajes de depuración y diagnóstico.
 *
 * @param level Nivel del mensaje.
 * @param phase Fase o componente que emitió el mensaje.
 */
static void log_prefix(const char *level, const char *phase)
{
    fprintf(stderr, "[%s] %s: ", level, phase ? phase : "compiler");
}

/**
 * Registra un error con formato profesional.
 *
 * @param phase Etapa donde ocurrió el problema.
 * @param fmt Cadena de formato tipo printf.
 */
void log_error(const char *phase, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_prefix("ERROR", phase);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/**
 * Registra una advertencia del compilador.
 *
 * @param phase Etapa o módulo que emite la advertencia.
 * @param fmt Cadena de formato tipo printf.
 */
void log_warning(const char *phase, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_prefix("WARN", phase);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/**
 * Registra un mensaje informativo.
 *
 * @param phase Etapa o módulo asociado al mensaje.
 * @param fmt Cadena de formato tipo printf.
 */
void log_info(const char *phase, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_prefix("INFO", phase);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/**
 * Registra un mensaje de éxito.
 *
 * @param phase Etapa o módulo que concluyó correctamente.
 * @param fmt Cadena de formato tipo printf.
 */
void log_success(const char *phase, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_prefix("OK", phase);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
