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
extern Quadruple* codigo;

int main(int argc, const char *argv[])
{
    printf("Bienvenido al compilador casero\n");

    if (argc > 1)
    {
        FILE *archivo = LeerArchivo(argv[1]);
        if (!archivo)
        {
            perror("Error en la lectura del archivo");
            exit(EXIT_FAILURE);
        }

        /**Analisis Lexico */
        AnalizarArchivo(archivo);
         Imprimir(raiz);
        //  Analisis Sintactico
        ASTNode *ast_root = parsePrograma();
         ImprimirAST(ast_root, 0);

        TablaSimbolos *tabla = realizar_analisis_semantico(ast_root);
         imprimir_jerarquia_tablas_simbolos(tabla, 0);

        if (contador_errores_semanticos > 0)
        {
            imprimir_errores_semanticos();
            fprintf(stderr, "La compilacion aborto debido a errores semanticos.\n");
            liberarAST(ast_root);
            return EXIT_FAILURE;
        }

        generate_intermediate_code(ast_root, tabla);

        print_intermediate_code();

        liberarAST(ast_root);
        destruir_jerarquia_tablas_simbolos(tabla);
    }

    else
        printf("Por favor ingrese la ruta del archivo a compilar");

    return 0;
}