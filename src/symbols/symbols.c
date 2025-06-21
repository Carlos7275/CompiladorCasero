#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"
#include "types.h"
#include "semantic.h"


unsigned int calcular_hash(const char *nombre) {
    unsigned int hash = 0;
    for (int i = 0; nombre[i] != '\0'; i++) {
        hash = hash * 31 + nombre[i];
    }
    return hash % CUBETAS_TABLA_SIMBOLOS;
}

TablaSimbolos *crear_tabla_simbolos(TablaSimbolos *padre) {
    TablaSimbolos *nueva_tabla = (TablaSimbolos *)malloc(sizeof(TablaSimbolos));
    if (nueva_tabla == NULL) {
        perror("Error al asignar memoria para TablaSimbolos");
        exit(EXIT_FAILURE);
    }

    static int next_id = 0;
    nueva_tabla->id_ambito = next_id++;
    nueva_tabla->padre = padre;

    for (int i = 0; i < CUBETAS_TABLA_SIMBOLOS; i++) {
        nueva_tabla->cubetas[i] = NULL;
    }

    nueva_tabla->hijos = NULL;
    nueva_tabla->num_hijos = 0;
    nueva_tabla->capacidad_hijos = 0;

    if (padre != NULL) {
        if (padre->num_hijos == padre->capacidad_hijos) {
            padre->capacidad_hijos = (padre->capacidad_hijos == 0) ? 2 : padre->capacidad_hijos * 2;
            padre->hijos = (TablaSimbolos **)realloc(padre->hijos, sizeof(TablaSimbolos *) * padre->capacidad_hijos);
            if (padre->hijos == NULL) {
                perror("Error al reasignar memoria para hijos de TablaSimbolos");
                exit(EXIT_FAILURE);
            }
        }
        padre->hijos[padre->num_hijos++] = nueva_tabla;
    }

    return nueva_tabla;
}


void destruir_jerarquia_tablas_simbolos(TablaSimbolos *tabla) {
    if (tabla == NULL) {
        return;
    }

    for (int i = 0; i < tabla->num_hijos; i++) {
        destruir_jerarquia_tablas_simbolos(tabla->hijos[i]);
    }

    if (tabla->hijos != NULL) {
        free(tabla->hijos);
        tabla->hijos = NULL;
    }

    for (int i = 0; i < CUBETAS_TABLA_SIMBOLOS; i++) {
        EntradaSimbolo *actual = tabla->cubetas[i];
        while (actual != NULL) {
            EntradaSimbolo *temp = actual;
            actual = actual->siguiente_en_cubeta;
            free(temp->nombre);
            if (temp->es_constante && temp->tipo == STRING && temp->valor_constante.valor_cadena != NULL) {
                free(temp->valor_constante.valor_cadena);
            }
            free(temp);
        }
        tabla->cubetas[i] = NULL;
    }
    free(tabla);
}

EntradaSimbolo *agregar_simbolo(TablaSimbolos *tabla, const char *nombre, enum TipoDato tipo, int renglon, int columna) {
    if (tabla == NULL || nombre == NULL) {
        return NULL;
    }

    if (buscar_simbolo_en_ambito_actual(tabla, nombre) != NULL) {
        return NULL;
    }

    unsigned int indice = calcular_hash(nombre);
    EntradaSimbolo *nueva_entrada = (EntradaSimbolo *)malloc(sizeof(EntradaSimbolo));
    if (nueva_entrada == NULL) {
        perror("Error al asignar memoria para EntradaSimbolo");
        exit(EXIT_FAILURE);
    }

    nueva_entrada->nombre = strdup(nombre);
    nueva_entrada->tipo = tipo;
    nueva_entrada->es_constante = 0;
    nueva_entrada->valor_constante.valor_int = 0;
    nueva_entrada->valor_constante.valor_float = 0.0;
    nueva_entrada->valor_constante.valor_cadena = NULL;
    nueva_entrada->valor_constante.valor_bool = 0;
    nueva_entrada->siguiente_en_cubeta = tabla->cubetas[indice];
    tabla->cubetas[indice] = nueva_entrada;

    return nueva_entrada;
}

EntradaSimbolo *buscar_simbolo_en_ambito_actual(TablaSimbolos *tabla, const char *nombre) {
    if (tabla == NULL || nombre == NULL) {
        return NULL;
    }
    unsigned int indice = calcular_hash(nombre);
    EntradaSimbolo *actual = tabla->cubetas[indice];
    while (actual != NULL) {
        if (strcmp(actual->nombre, nombre) == 0) {
            return actual;
        }
        actual = actual->siguiente_en_cubeta;
    }
    return NULL;
}

EntradaSimbolo *buscar_simbolo(TablaSimbolos *tabla, const char *nombre) {
    TablaSimbolos *actual_ambito = tabla;
    while (actual_ambito != NULL) {
        EntradaSimbolo *encontrado = buscar_simbolo_en_ambito_actual(actual_ambito, nombre);
        if (encontrado != NULL) {
            return encontrado;
        }
        actual_ambito = actual_ambito->padre;
    }
    return NULL;
}

void imprimir_indentacion(int nivel) {
    for (int i = 0; i < nivel * 4; i++) {
        printf(" ");
    }
}

void imprimir_jerarquia_tablas_simbolos(TablaSimbolos *tabla, int nivel) {
    if (tabla == NULL) {
        return;
    }

    imprimir_indentacion(nivel);
    printf("--- Contenido del Ambito ID: %d (Padre ID: %d) ---\n",
           tabla->id_ambito, tabla->padre ? tabla->padre->id_ambito : -1);

    int simbolos_encontrados = 0;
    for (int i = 0; i < CUBETAS_TABLA_SIMBOLOS; i++) {
        EntradaSimbolo *entrada = tabla->cubetas[i];
        if (entrada != NULL) {
            imprimir_indentacion(nivel);
            printf("  Cubeta %d:\n", i);
            while (entrada != NULL) {
                imprimir_indentacion(nivel);
                printf("    - Nombre: '%s'", entrada->nombre);
                printf(", Tipo: %s", tipoDatoToString(entrada->tipo));
                printf(", Rol: %s", entrada->es_constante ? "CONSTANTE" : "VARIABLE");

                if (entrada->es_constante) {
                    if (entrada->tipo == INT) {
                        printf(", Valor: %d", entrada->valor_constante.valor_int);
                    } else if (entrada->tipo == FLOAT) {
                        printf(", Valor: %.2f", entrada->valor_constante.valor_float);
                    } else if (entrada->tipo == STRING) {
                        printf(", Valor: \"%s\"", entrada->valor_constante.valor_cadena);
                    } else if (entrada->tipo == BOOL) {
                        printf(", Valor: %s", entrada->valor_constante.valor_bool ? "Verdadero" : "Falso");
                    }
                }
                printf("\n");
                simbolos_encontrados++;
                entrada = entrada->siguiente_en_cubeta;
            }
        }
    }

    if (simbolos_encontrados == 0) {
        imprimir_indentacion(nivel);
        printf("  (Este ambito esta vacio)\n");
    }
    imprimir_indentacion(nivel);
    printf("------------------------------------------\n");

    for (int i = 0; i < tabla->num_hijos; i++) {
        imprimir_jerarquia_tablas_simbolos(tabla->hijos[i], nivel + 1);
    }
}

EntradaSimbolo *buscar_simbolo_ambitos(TablaSimbolos *ambito, const char *nombre)
{
    TablaSimbolos *actual = ambito;
    EntradaSimbolo *simbolo = NULL;

    while (actual != NULL)
    {
        simbolo = buscar_simbolo(actual, nombre);
        if (simbolo != NULL)
        {
            return simbolo;
        }
        actual = actual->padre;  // sube al ámbito padre
    }
    return NULL;
}
