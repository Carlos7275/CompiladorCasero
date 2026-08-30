#include "errors.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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
