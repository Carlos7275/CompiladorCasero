#include "types.h"
#include<stdio.h>
#include<string.h>
#include<stdlib.h>

struct nodo *raiz = NULL;
struct nodo *actual = NULL;

void Insertar(struct Token token)
{
    struct nodo *nuevo;
    nuevo = malloc(sizeof(struct nodo));
    nuevo->info = token;
    nuevo->izq = NULL;
    nuevo->der = NULL;

    if (raiz == NULL)
    {
        raiz = nuevo;
        actual = nuevo;
    }
    else
    {
        nuevo->izq = actual;
        actual->der = nuevo;
        actual = nuevo;
    }
}

void imprimir_lexico(struct nodo *Nodo)
{
    if (Nodo != NULL)
    {
        printf("Lexema:%s\tTipoToken:%d\tTipoDato:%d\tColumna:%d\tRenglon:%d",
               Nodo->info.Lexema,
               Nodo->info.TipoToken,
               Nodo->info.tipoDato,
               Nodo->info.Columna,
               Nodo->info.Renglon);
        if (Nodo->info.valor)
            printf("\tValor:%lf", Nodo->info.valor);
        printf("\n----------------------------------------------------------------------------------------------\n");

        imprimir_lexico(Nodo->der);
    }
}

void generarToken(int tipoToken, const char *lexema, int tipoDato, int Col, int Renglon)
{
    struct Token token;
    token.tipoDato = tipoDato;
    token.TipoToken = tipoToken;
    token.Columna = Col;
    token.Renglon = Renglon;
    token.Lexema = malloc(strlen(lexema) + 1);
    strcpy(token.Lexema, lexema);

    if (tipoDato == INT || tipoDato == FLOAT)
    {
        double var = 0.0;
        sscanf(lexema, "%lf", &var);
        token.valor = var;
    }
    else
        token.valor = 0.0;
    Insertar(token);
}
