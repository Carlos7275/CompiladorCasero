#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "file.h"
#include "types.h"
#include "parser.h"

extern struct nodo *raiz;
extern struct nodo *actual;
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
        //Analisis Sintactico
        ASTNode* ast_root = parsePrograma();
        ImprimirAST(ast_root,0);

    }
    else
        printf("Por favor ingrese la ruta del archivo a compilar");

    return 0;
}