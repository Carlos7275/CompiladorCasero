#include "file.h"
#include "errors.h"
#include "lexer.h"
#include "types.h"
#include <stdlib.h>
#include <ctype.h>
int Renglon = 1, Col = 0;
extern struct nodo *actual;

/**
 * Abre un archivo fuente en modo lectura.
 *
 * @param archivo Ruta del archivo a abrir.
 * @return Puntero al flujo de archivo abierto, o NULL si falla.
 */
FILE *leer_archivo(const char *archivo)
{
    return fopen(archivo, "r");
}

void analizar_archivo(FILE *archivo)
{
    Renglon = 1;
    Col = 0;
    int car;
    while ((car = fgetc(archivo)) != EOF)
    {
        int startColForToken = Col;
        int startLineForToken = Renglon;
        struct nodo *last_token = actual;
        int tokenized = 0;

        if (isalpha(car))
        {
            EsID(archivo, car, startColForToken, Renglon);
            tokenized = 1;
        }
        else if (car == '/')
        {
            int next_car = fgetc(archivo);
            if (next_car == EOF)
            {
                EsSimbolo(archivo, car, startColForToken, Renglon);
                tokenized = 1;
            }
            else if (next_car == '/')
            {
                Col += 2;
                while ((car = fgetc(archivo)) != EOF && car != '\n')
                {
                    Col++;
                }
                if (car == '\n')
                {
                    Renglon++;
                    Col = 0;
                }
                continue;
            }
            else if (next_car == '*')
            {
                Col += 2;
                int in_comment = 1;
                int prev_car = 0;
                while (in_comment && (car = fgetc(archivo)) != EOF)
                {
                    Col++;
                    if (car == '\n')
                    {
                        Renglon++;
                        Col = 0;
                    }
                    if (prev_car == '*' && car == '/')
                    {
                        in_comment = 0;
                    }
                    prev_car = car;
                }
                if (in_comment)
                {
                    if (diagnostics_json_enabled())
                    {
                        report_diagnostic("MX1004", "error", "lexer", Renglon, Col,
                                          "El comentario de bloque no tiene cierre '*/'.");
                        emit_diagnostics_json();
                    }
                    else
                        fprintf(stderr, "Error (R%d, C%d): Comentario multi-línea no cerrado.\n", Renglon, Col);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            else
            {
                ungetc(next_car, archivo);
                EsSimbolo(archivo, car, startColForToken, Renglon);
                tokenized = 1;
            }
        }
        else if (isdigit(car))
        {
            EsNum(archivo, car, startColForToken, Renglon);
            tokenized = 1;
        }
        else if (car == '"')
        {
            EsCadena(archivo, car, startColForToken, Renglon);
            tokenized = 1;
        }
        else if (isascii(car) && car != ' ' && car != '\n' && car != '\r' && car != '\t')
        {
            EsSimbolo(archivo, car, startColForToken, Renglon);
            tokenized = 1;
        }

        if (tokenized && actual != last_token)
        {
            actual->info.Renglon = startLineForToken;
            actual->info.Columna = startColForToken;
            for (const unsigned char *cursor =
                     (const unsigned char *)actual->info.Lexema;
                 *cursor; ++cursor)
            {
                if (*cursor == '\n')
                {
                    Renglon++;
                    Col = 0;
                }
                else
                    Col++;
            }
        }
        else if (car == '\n')
        {
            Renglon++;
            Col = 0;
        }
        else if (isspace(car))
        {
            Col++;
        }
    }

    fclose(archivo);
}