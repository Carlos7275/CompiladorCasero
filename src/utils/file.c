#include "file.h"
#include "lexer.h"
#include "types.h"
#include <stdlib.h>
#include <ctype.h>
int Renglon = 1, Col = 0;

// Lee un archivo en modo lectura y lo regresa
FILE *LeerArchivo(const char *archivo)
{
    return fopen(archivo, "r");
}

void AnalizarArchivo(FILE *archivo)
{
    int car;
    while ((car = fgetc(archivo)) != EOF)
    {
        int startColForToken = Col; 

        if (isalpha(car))
            EsID(archivo, car, startColForToken, Renglon);
        else if (car == '/')
        {
            int next_car = fgetc(archivo);
            if (next_car == EOF)
            {
                EsSimbolo(archivo, car, startColForToken, Renglon);
            }
            else if (next_car == '/')
            {
                Col++;
                while ((car = fgetc(archivo)) != EOF && car != '\n')
                {
                    Col++;
                }
                if (car == '\n')
                {
                    Renglon++;
                    Col = 1;
                }
                continue;
            }
            else if (next_car == '*')
            {
                Col++;
                int in_comment = 1;
                int prev_car = 0;
                while (in_comment && (car = fgetc(archivo)) != EOF)
                {
                    Col++;
                    if (car == '\n')
                    {
                        Renglon++;
                        Col = 1;
                    }
                    if (prev_car == '*' && car == '/')
                    {
                        in_comment = 0;
                    }
                    prev_car = car;
                }
                if (in_comment)
                {
                    fprintf(stderr, "Error (R%d, C%d): Comentario multi-línea no cerrado.\n", Renglon, Col);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            else
            {
                ungetc(next_car, archivo);
                EsSimbolo(archivo, car, startColForToken, Renglon);
            }
        }
        else if (isdigit(car))
            EsNum(archivo, car, startColForToken, Renglon);
        else if (car == '"')
            EsCadena(archivo, car, startColForToken, Renglon);
        else if (isascii(car) && car != ' ' && car != '\n' && car != '\r' && car != '\t')
            EsSimbolo(archivo, car, startColForToken, Renglon);
        
        if (car == '\n') {
            Renglon++;
            Col = 1;
        } else if (isspace(car)) {
            Col++;
        }
    }

    fclose(archivo);
}