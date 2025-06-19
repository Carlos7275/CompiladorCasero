#ifndef SYMBOLS_H
#define SYMBOLS_H
#define CUBETAS_TABLA_SIMBOLOS 100
#include "types.h";
typedef struct EntradaSimbolo
{
    char *nombre;
    enum TipoDato tipo;

    struct EntradaSimbolo *siguiente_en_cubeta;
} EntradaSimbolo;

typedef struct TablaSimbolos
{
    EntradaSimbolo *cubetas[CUBETAS_TABLA_SIMBOLOS];
    struct TablaSimbolos *padre;
    int id_ambito;
} TablaSimbolos;

unsigned int calcular_hash(const char *nombre);

TablaSimbolos *crear_tabla_simbolos(TablaSimbolos *padre);

void destruir_tabla_simbolos(TablaSimbolos *tabla);

EntradaSimbolo *agregar_simbolo(TablaSimbolos *tabla, const char *nombre, enum TipoDato tipo,int renglon,int columna);

EntradaSimbolo *buscar_simbolo(TablaSimbolos *tabla, const char *nombre);

EntradaSimbolo *buscar_simbolo_en_ambito_actual(TablaSimbolos *tabla, const char *nombre);
void imprimir_tabla_simbolos(TablaSimbolos *tabla);

#endif