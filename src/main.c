#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "file.h"
#include "types.h"
#include "parser.h"
#include "semantic.h"
#include "symbols.h"
#include "codegen.h"
#include "errors.h"

#ifndef PATH_MAX
#define PATH_MAX 32768
#endif

extern struct nodo *raiz;
extern struct nodo *actual;
extern struct ErrorSemantico *cabeza_errores;
extern int contador_errores_semanticos;
extern Quadruple *codigo;

/**
 * Estructura de configuración del compilador.
 * Contiene todas las opciones procesadas de la línea de comandos.
 */
typedef struct
{
    const char *source_file;
    const char *output_name;
    int preserve_asm;
    int debug_mode;
    int execute_binary;
} CompilerOptions;

/**
 * Estructura para gestionar recursos abiertos.
 * Facilita la limpieza uniforme al finalizar la compilación.
 */
typedef struct
{
    FILE *source_fp;
    ASTNode *ast;
    TablaSimbolos *symbol_table;
    char asm_file[256];
    char obj_file[256];
} CompilationContext;

/**
 * Determina la arquitectura de enlace compatible con el sistema.
 * En macOS, traduce arm64 a x86_64 para mantener compatibilidad.
 *
 * @return Cadena con el nombre de arquitectura soportado.
 */
static const char *get_target_link_arch(void)
{
#if defined(__APPLE__)
    FILE *pipe = popen("uname -m", "r");
    static char arch[32] = "x86_64";

    if (pipe != NULL)
    {
        if (fgets(arch, sizeof(arch), pipe) != NULL)
        {
            arch[strcspn(arch, "\r\n")] = '\0';
        }
        pclose(pipe);
    }

    if (strcmp(arch, "arm64") == 0 || strcmp(arch, "aarch64") == 0)
    {
        return "x86_64";
    }

    return "x86_64";
#else
    return "x86_64";
#endif
}

/**
 * Valida la extensión del archivo fuente.
 *
 * @param filepath Ruta del archivo a validar.
 * @return 1 si es válido, 0 en caso contrario.
 */
static int is_valid_source_file(const char *filepath)
{
    if (filepath == NULL)
        return 0;

    const char *ext = strrchr(filepath, '.');
    return (ext != NULL && strcmp(ext, ".mx") == 0);
}

/**
 * Parsea los argumentos de la línea de comandos.
 * Valida las opciones y extrae el archivo fuente y nombre de salida.
 *
 * @param argc Cantidad de argumentos.
 * @param argv Vector de argumentos.
 * @param opts Estructura de opciones a llenar.
 * @return 0 si es exitoso, -1 en caso de error.
 */
static int parse_arguments(int argc, const char *argv[], CompilerOptions *opts)
{
    opts->source_file = NULL;
    opts->output_name = "program";
    opts->preserve_asm = 0;
    opts->debug_mode = 0;
    opts->execute_binary = 1;

    if (argc < 2)
    {
        return -1;
    }

    int positional_args = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (strcmp(arg, "-help") == 0 || strcmp(arg, "--help") == 0)
        {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(arg, "-version") == 0 || strcmp(arg, "--version") == 0)
        {
            print_version();
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(arg, "-asm") == 0)
        {
            opts->preserve_asm = 1;
        }
        else if (strcmp(arg, "-debug") == 0)
        {
            opts->debug_mode = 1;
        }
        else if (strcmp(arg, "-no-run") == 0)
        {
            opts->execute_binary = 0;
        }
        else if (arg[0] == '-')
        {
            log_error("arguments", "Opcion desconocida: %s", arg);
            return -1;
        }
        else
        {
            if (positional_args == 0)
            {
                opts->source_file = arg;
                positional_args++;
            }
            else if (positional_args == 1)
            {
                opts->output_name = arg;
                positional_args++;
            }
            else
            {
                log_error("arguments", "Argumentos posicionales excesivos: %s", arg);
                return -1;
            }
        }
    }

    if (opts->source_file == NULL)
    {
        log_error("arguments", "Archivo fuente requerido");
        return -1;
    }

    if (!is_valid_source_file(opts->source_file))
    {
        log_error("arguments", "Archivo debe tener extension .mx: %s", opts->source_file);
        return -1;
    }

    return 0;
}

static int is_safe_command_path(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;

    for (const unsigned char *cursor = (const unsigned char *)path; *cursor; cursor++)
    {
        if (*cursor < 32 || strchr("\"'&|<>;$`!(){}[]^%", *cursor) != NULL)
            return 0;
    }
    return 1;
}

/**
 * Realiza el análisis léxico y sintáctico del archivo fuente.
 *
 * @param source_fp Archivo abierto con el código fuente.
 * @return Árbol sintáctico abstracto o NULL en caso de error.
 */
static ASTNode *parse_source_file(FILE *source_fp)
{
    reiniciar_tokens();
    analizar_archivo(source_fp);
    return parsePrograma();
}

static int path_in_stack(const char *path, const char **stack, int depth)
{
    for (int i = 0; i < depth; i++)
        if (strcmp(path, stack[i]) == 0) return 1;
    return 0;
}

static ASTNode *parse_module_path(const char *path)
{
    FILE *fp = leer_archivo(path);
    if (fp == NULL)
    {
        log_error("import", "No se pudo abrir biblioteca: %s", path);
        return NULL;
    }
    ASTNode *ast = parse_source_file(fp);
    return ast;
}

static int import_failed = 0;

static void cargar_dotenv(const char *source_path)
{
    char path[PATH_MAX];
    const char *slash = strrchr(source_path, '/');
    if (slash)
        snprintf(path, sizeof(path), "%.*s.env", (int)(slash - source_path + 1), source_path);
    else
        snprintf(path, sizeof(path), ".env");

    FILE *env = fopen(path, "r");
    if (!env) return;

    char line[1024];
    while (fgets(line, sizeof(line), env)) {
        char *key = line;
        while (isspace((unsigned char)*key)) key++;
        if (*key == '#' || *key == '\0') continue;
        if (strncmp(key, "export ", 7) == 0) key += 7;
        char *equals = strchr(key, '=');
        if (!equals) continue;
        *equals = '\0';
        char *value = equals + 1;
        char *end = key + strlen(key);
        while (end > key && isspace((unsigned char)end[-1])) *--end = '\0';
        value[strcspn(value, "\r\n")] = '\0';
        if (value[0] == '"' && value[strlen(value) - 1] == '"') {
            value[strlen(value) - 1] = '\0';
            value++;
        }
        if (*key != '\0' && getenv(key) == NULL)
        {
    #ifdef _WIN32
            _putenv_s(key, value);
    #else
            setenv(key, value, 0);
    #endif
        }
    }
    fclose(env);
}

/* Replaces import nodes with the declarations from the referenced module. */
static ASTNode *expand_imports(ASTNode *program, const char *source_path,
                               const char **stack, int depth)
{
    if (program == NULL) return NULL;
    if (depth >= 64)
    {
        log_error("import", "Se excedio la profundidad maxima de importaciones (64)");
        import_failed = 1;
        return NULL;
    }
    char canonical[PATH_MAX];
#ifdef _WIN32
    if (_fullpath(canonical, source_path, sizeof(canonical)) == NULL)
#else
    if (realpath(source_path, canonical) == NULL)
#endif
        snprintf(canonical, sizeof(canonical), "%s", source_path);
    if (path_in_stack(canonical, stack, depth))
    {
        log_error("import", "Ciclo de importacion detectado: %s", source_path);
        import_failed = 1;
        return NULL;
    }
#ifdef _WIN32
    stack[depth] = _strdup(canonical);
#else
    stack[depth] = strdup(canonical);
#endif

    ASTNode *head = NULL, *tail = NULL;
    ASTNode *node = program->hijo_izq;
    while (node != NULL)
    {
        ASTNode *next = node->siguiente_hermano;
        node->siguiente_hermano = NULL;
        if (node->type == AST_IMPORT)
        {
            char imported[PATH_MAX];
            const char *slash = strrchr(source_path, '/');
#ifdef _WIN32
            const char *backslash = strrchr(source_path, '\\');
            if (backslash != NULL && (slash == NULL || backslash > slash))
                slash = backslash;
#endif
            size_t dir_len = slash ? (size_t)(slash - source_path + 1) : 0;
            int absolute_path = node->valor.valor_cadena[0] == '/';
#ifdef _WIN32
            absolute_path = absolute_path || node->valor.valor_cadena[0] == '\\' ||
                            (isalpha((unsigned char)node->valor.valor_cadena[0]) &&
                             node->valor.valor_cadena[1] == ':');
#endif
            if (absolute_path)
                snprintf(imported, sizeof(imported), "%s", node->valor.valor_cadena);
            else
                snprintf(imported, sizeof(imported), "%.*s%s", (int)dir_len,
                         source_path, node->valor.valor_cadena);
            ASTNode *module = parse_module_path(imported);
            if (module == NULL) import_failed = 1;
            ASTNode *module_nodes = module ? expand_imports(module, imported, stack, depth + 1) : NULL;
            if (module) { module->hijo_izq = NULL; free(module); }
            free(node->valor.valor_cadena);
            free(node);
            while (module_nodes)
            {
                ASTNode *mn = module_nodes;
                module_nodes = mn->siguiente_hermano;
                mn->siguiente_hermano = NULL;
                if (!head) head = mn; else tail->siguiente_hermano = mn;
                tail = mn;
            }
        }
        else
        {
            if (!head) head = node; else tail->siguiente_hermano = node;
            tail = node;
        }
        node = next;
    }
    free((char *)stack[depth]);
    program->hijo_izq = head;
    return head;
}

/**
 * Ejecuta el análisis semántico sobre el AST.
 *
 * @param ast Árbol sintáctico a validar.
 * @return Tabla de símbolos construida o NULL en error.
 */
static TablaSimbolos *analyze_semantics(ASTNode *ast)
{
    TablaSimbolos *table = realizar_analisis_semantico(ast);

    if (contador_errores_semanticos > 0)
    {
        imprimir_errores_semanticos();
        return NULL;
    }

    return table;
}

/**
 * Genera código intermedio y ensamblador.
 *
 * @param ast Árbol sintáctico.
 * @param table Tabla de símbolos.
 * @param asm_filepath Ruta donde guardar el archivo .asm
 * @return 0 si es exitoso, -1 en error.
 */
static int generate_code(ASTNode *ast, TablaSimbolos *table, const char *asm_filepath)
{
    generar_codigo_intermedio(ast, table);

    FILE *asm_fp = fopen(asm_filepath, "w");
    if (asm_fp == NULL)
    {
        log_error("io", "No se pudo crear archivo de ensamblador: %s", asm_filepath);
        return -1;
    }

    generate_asm(asm_fp);
    fclose(asm_fp);

    return 0;
}

/**
 * Ejecuta el ensamblador para generar código objeto.
 *
 * @param asm_file Archivo fuente de ensamblador.
 * @param obj_file Archivo destino de código objeto.
 * @return 0 si es exitoso, -1 en error.
 */
static int assemble_code(const char *asm_file, const char *obj_file)
{
    char cmd[512];
    int written;

    if (!is_safe_command_path(asm_file) || !is_safe_command_path(obj_file))
    {
        log_error("assembly", "Ruta de archivo no permitida");
        return -1;
    }

#if defined(_WIN32)
    written = snprintf(cmd, sizeof(cmd), "nasm -f win64 \"%s\" -o \"%s\"", asm_file, obj_file);
#elif defined(__APPLE__)
    written = snprintf(cmd, sizeof(cmd), "nasm -f macho64 \"%s\" -o \"%s\"", asm_file, obj_file);
#else
    written = snprintf(cmd, sizeof(cmd), "nasm -f elf64 \"%s\" -o \"%s\"", asm_file, obj_file);
#endif

    if (written < 0 || (size_t)written >= sizeof(cmd))
    {
        log_error("assembly", "Comando de ensamblado demasiado largo");
        return -1;
    }

    if (system(cmd) != 0)
    {
        log_error("assembly", "El ensamblador fallo procesando: %s", asm_file);
        return -1;
    }

    return 0;
}

/**
 * Enlaza el código objeto para generar el ejecutable final.
 *
 * @param obj_file Archivo de código objeto.
 * @param output_name Nombre base del ejecutable.
 * @return 0 si es exitoso, -1 en error.
 */
static int link_executable(const char *obj_file, const char *output_name)
{
    char cmd[512];
    int written;

    if (!is_safe_command_path(obj_file) || !is_safe_command_path(output_name))
    {
        log_error("link", "Ruta de archivo no permitida");
        return -1;
    }

#if defined(_WIN32)
    written = snprintf(cmd, sizeof(cmd), "gcc \"%s\" \"runtime/mx_runtime.c\" -o \"%s.exe\" -lm -lws2_32", obj_file, output_name);
#elif defined(__APPLE__)
    const char *link_arch = get_target_link_arch();
    written = snprintf(cmd, sizeof(cmd), "clang -arch %s -Wl,-w \"%s\" \"runtime/mx_runtime.c\" -o \"%s\" -lm", link_arch, obj_file, output_name);

#else
    written = snprintf(cmd, sizeof(cmd), "gcc \"%s\" \"runtime/mx_runtime.c\" -o \"%s\" -lm", obj_file, output_name);
#endif

    if (written < 0 || (size_t)written >= sizeof(cmd))
    {
        log_error("link", "Comando de enlazado demasiado largo");
        return -1;
    }

    if (system(cmd) != 0)
    {
        log_error("link", "El enlazador fallo");
        return -1;
    }

#if defined(_WIN32)
    log_success("build", "Compilacion completada: %s.exe", output_name);
#elif defined(__APPLE__)
    log_success("build", "Compilacion completada: ./%s (%s)", output_name, link_arch);
#else
    log_success("build", "Compilacion completada: ./%s", output_name);
#endif

    return 0;
}

/**
 * Ejecuta el binario generado.
 *
 * @param output_name Nombre base del ejecutable.
 * @return Código de salida del programa, o -1 en error fatal.
 */
static int run_executable(const char *output_name)
{
    char cmd[512];
    int written;

    if (!is_safe_command_path(output_name))
    {
        log_error("runtime", "Ruta de ejecutable no permitida");
        return -1;
    }

    log_info("runtime", "Ejecutando %s...", output_name);

#ifdef _WIN32
    written = snprintf(cmd, sizeof(cmd), "\"%s.exe\"", output_name);
#else
    written = snprintf(cmd, sizeof(cmd), "\"./%s\"", output_name);
#endif

    if (written < 0 || (size_t)written >= sizeof(cmd))
        return -1;

    int status = system(cmd);
    return status;
}

/**
 * Limpia archivos intermedios generados durante la compilación.
 *
 * @param asm_file Archivo de ensamblador a limpiar.
 * @param obj_file Archivo de código objeto a limpiar.
 * @param preserve_asm Si es 1, preserva el archivo .asm.
 */
static void cleanup_intermediates(const char *asm_file, const char *obj_file, int preserve_asm)
{
    if (remove(obj_file) != 0 && errno != ENOENT)
    {
        log_warning("cleanup", "No se pudo eliminar archivo temporal: %s", obj_file);
    }

    if (!preserve_asm)
    {
        if (remove(asm_file) != 0 && errno != ENOENT)
        {
            log_warning("cleanup", "No se pudo eliminar archivo temporal: %s", asm_file);
        }
    }
}

/**
 * Libera todos los recursos asociados con la compilación.
 *
 * @param ctx Contexto de compilación.
 */
static void free_compilation_context(CompilationContext *ctx)
{
    if (ctx->source_fp != NULL)
    {
        fclose(ctx->source_fp);
        ctx->source_fp = NULL;
    }

    if (ctx->ast != NULL)
    {
        liberar_ast(ctx->ast);
        ctx->ast = NULL;
    }

    if (ctx->symbol_table != NULL)
    {
        destruir_jerarquia_tablas_simbolos(ctx->symbol_table);
        ctx->symbol_table = NULL;
    }
}

/**
 * Muestra información de depuración del compilador.
 *
 * @param ctx Contexto de compilación.
 */
static void print_debug_info(CompilationContext *ctx)
{
    fprintf(stderr, "\n=== DEBUG INFO ===\n\n");

    fprintf(stderr, "--- Tokens Léxicos ---\n");
    imprimir_lexico(raiz);

    fprintf(stderr, "\n--- Árbol Sintáctico Abstracto ---\n");
    imprimir_ast(ctx->ast, 0);

    fprintf(stderr, "\n--- Tabla de Símbolos ---\n");
    imprimir_jerarquia_tablas_simbolos(ctx->symbol_table, 0);

    fprintf(stderr, "\n--- Código Intermedio ---\n");
    imprimir_codigo_intermedio();

    fprintf(stderr, "\n--- Ensamblador Generado ---\n");
    FILE *asm_fp = fopen(ctx->asm_file, "r");
    if (asm_fp != NULL)
    {
        char line[512];
        while (fgets(line, sizeof(line), asm_fp) != NULL)
        {
            fprintf(stderr, "%s", line);
        }
        fclose(asm_fp);
    }
    fprintf(stderr, "\n=== FIN DEBUG ===\n\n");
}

/**
 * Punto de entrada del compilador.
 *
 * Orquesta el flujo completo de compilación: parseado de argumentos,
 * análisis léxico/sintáctico, validación semántica, generación de código,
 * ensamblado, enlazado y ejecución (si corresponde).
 *
 * @param argc Cantidad de argumentos.
 * @param argv Vector de argumentos.
 * @return Código de salida: EXIT_SUCCESS (0) o EXIT_FAILURE (1).
 */
int main(int argc, const char *argv[])
{
    CompilerOptions opts;
    CompilationContext ctx = {0};

    if (parse_arguments(argc, argv, &opts) != 0)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    ctx.source_fp = leer_archivo(opts.source_file);
    if (ctx.source_fp == NULL)
    {
        log_error("io", "No se pudo abrir archivo: %s", opts.source_file);
        return EXIT_FAILURE;
    }

    cargar_dotenv(opts.source_file);
    ctx.ast = parse_source_file(ctx.source_fp);
    if (ctx.ast == NULL)
    {
        log_error("parser", "Fallo el analisis sintactico");
        free_compilation_context(&ctx);
        return EXIT_FAILURE;
    }

    const char *import_stack[64] = {0};
    import_failed = 0;
    expand_imports(ctx.ast, opts.source_file, import_stack, 0);
    if (import_failed)
    {
        free_compilation_context(&ctx);
        return EXIT_FAILURE;
    }

    ctx.symbol_table = analyze_semantics(ctx.ast);
    if (ctx.symbol_table == NULL)
    {
        log_error("semantic", "Fallo el analisis semantico");
        free_compilation_context(&ctx);
        return EXIT_FAILURE;
    }

    snprintf(ctx.asm_file, sizeof(ctx.asm_file), "%s.asm", opts.output_name);
    snprintf(ctx.obj_file, sizeof(ctx.obj_file), "%s.o", opts.output_name);

#if defined(_WIN32)
    snprintf(ctx.obj_file, sizeof(ctx.obj_file), "%s.obj", opts.output_name);
#endif

    if (generate_code(ctx.ast, ctx.symbol_table, ctx.asm_file) != 0)
    {
        cleanup_intermediates(ctx.asm_file, ctx.obj_file, opts.preserve_asm);
        free_compilation_context(&ctx);
        return EXIT_FAILURE;
    }

    if (assemble_code(ctx.asm_file, ctx.obj_file) != 0)
    {
        cleanup_intermediates(ctx.asm_file, ctx.obj_file, opts.preserve_asm);
        free_compilation_context(&ctx);
        return EXIT_FAILURE;
    }

    if (link_executable(ctx.obj_file, opts.output_name) != 0)
    {
        cleanup_intermediates(ctx.asm_file, ctx.obj_file, opts.preserve_asm);
        free_compilation_context(&ctx);
        return EXIT_FAILURE;
    }

    if (opts.execute_binary)
    {
        int run_status = run_executable(opts.output_name);
        if (run_status != 0)
        {
#ifdef _WIN32
            log_error("runtime", "El programa retorno con codigo: %d", run_status);
#else
            log_error("runtime", "El programa retorno con codigo: %d", WEXITSTATUS(run_status));
#endif
            cleanup_intermediates(ctx.asm_file, ctx.obj_file, opts.preserve_asm);
            free_compilation_context(&ctx);
            return EXIT_FAILURE;
        }
    }

    if (opts.debug_mode)
    {
        print_debug_info(&ctx);
    }

    cleanup_intermediates(ctx.asm_file, ctx.obj_file, opts.preserve_asm);
    free_compilation_context(&ctx);

    return EXIT_SUCCESS;
}