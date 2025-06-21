#ifndef SYMBOLS_H
#define SYMBOLS_H

#define CUBETAS_TABLA_SIMBOLOS 100

#include "types.h"

typedef struct EntradaSimbolo
{
    char *nombre;
    enum TipoDato tipo;
    int es_constante;
    union
    {
        int valor_int;
        double valor_float;
        char *valor_cadena;
        int valor_bool;
    } valor_constante;
    struct EntradaSimbolo *siguiente_en_cubeta;
} EntradaSimbolo;

typedef struct TablaSimbolos
{
    EntradaSimbolo *cubetas[CUBETAS_TABLA_SIMBOLOS];
    struct TablaSimbolos *padre;
    int id_ambito;
    struct TablaSimbolos **hijos;
    int num_hijos;
    int capacidad_hijos;
} TablaSimbolos;

unsigned int calcular_hash(const char *nombre);

TablaSimbolos *crear_tabla_simbolos(TablaSimbolos *padre);

void destruir_jerarquia_tablas_simbolos(TablaSimbolos *tabla);

EntradaSimbolo *agregar_simbolo(TablaSimbolos *tabla, const char *nombre, enum TipoDato tipo, int renglon, int columna);

EntradaSimbolo *buscar_simbolo(TablaSimbolos *tabla, const char *nombre);

EntradaSimbolo *buscar_simbolo_en_ambito_actual(TablaSimbolos *tabla, const char *nombre);

void imprimir_jerarquia_tablas_simbolos(TablaSimbolos *tabla, int nivel);

EntradaSimbolo *buscar_simbolo_ambitos(TablaSimbolos *ambito, const char *nombre);
#endif