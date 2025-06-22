#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "file.h"
#include "types.h"
#include "parser.h"
#include "semantic.h"
#include "symbols.h"
#include "codegen.h"
extern struct nodo *raiz;
extern struct nodo *actual;

extern struct ErrorSemantico *cabeza_errores;
extern int contador_errores_semanticos;
extern Quadruple *codigo;

int main(int argc, const char *argv[])
{
    int asm_flag = 0;
    int debug_flag = 0;

    // Recorremos el resto de argumentos (si hay)
    for (int i = 3; i < argc; i++)
    {
        if (strcmp(argv[i], "-asm") == 0)
        {
            asm_flag = 1;
        }
        else if (strcmp(argv[i], "-debug") == 0)
        {
            debug_flag = 1;
        }
        else
        {
            printf("Opción desconocida: %s\n", argv[i]);
            return 1;
        }
    }

    if (argc > 1)
    {

        FILE *archivo = leer_archivo(argv[1]);
        if (!archivo)
        {
            char *cadena = strcat("Error en la lectura del archivo ", argv[1]);
            perror(cadena);
            exit(EXIT_FAILURE);
        }

        analizar_archivo(archivo);
        ASTNode *raiz_ast = parsePrograma();
        TablaSimbolos *tabla = realizar_analisis_semantico(raiz_ast);

        if (contador_errores_semanticos > 0)
        {
            imprimir_errores_semanticos();
            fprintf(stderr, "La compilacion aborto debido a errores semanticos.\n");
            liberar_ast(raiz_ast);
            return EXIT_FAILURE;
        }

        generar_codigo_intermedio(raiz_ast, tabla);

        char *base_name = argv[2] == NULL ? "program" : argv[2];
        char filename[256]; 

        snprintf(filename, sizeof(filename), "%s.asm", base_name);
        FILE *f = fopen(filename, "w");

        if (f == NULL)
        {
            perror("No se pudo crear el archivo");
        }

        generate_asm(f);

        fclose(f);

#ifdef _WIN32
        {
            char cmd[512];

            // NASM para Windows (win64)
            snprintf(cmd, sizeof(cmd), "nasm -f win64 %s.asm -o %s.obj", base_name, base_name);
            system(cmd);

            // Link para Windows (link.exe)
            snprintf(cmd, sizeof(cmd), "gcc -no-pie %s.o -o %s", base_name, base_name);
            system(cmd);

            printf("Compilacion terminada. Ejecutable: %s.exe\n", base_name);
        }
#else
        {
            char cmd[512];

            // NASM para Linux (elf64)
            snprintf(cmd, sizeof(cmd), "nasm -f elf64 %s.asm -o %s.o", base_name, base_name);
            system(cmd);

            // Link con ld para Linux
            snprintf(cmd, sizeof(cmd), "gcc -no-pie %s.o -o %s", base_name, base_name);
            system(cmd);

            printf("Compilacion terminada. Ejecutable: ./%s\n", base_name);
        }
#endif

        if (!asm_flag)
            remove("program.asm");

        if (debug_flag)
        {
            imprimir_lexico(raiz);
            imprimir_ast(raiz_ast, 0);
            imprimir_jerarquia_tablas_simbolos(tabla, 0);
            imprimir_codigo_intermedio();
        }
        liberar_ast(raiz_ast);

        destruir_jerarquia_tablas_simbolos(tabla);
    }

    else
        printf("Por favor ingrese la ruta del archivo a compilar");

    return 0;
}