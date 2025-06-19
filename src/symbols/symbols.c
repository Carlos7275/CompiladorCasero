#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"
#include "semantic.h"
static int siguiente_id_ambito = 0;



unsigned int calcular_hash(const char *nombre) {
    unsigned int valor_hash = 5381;
    int c;

    while ((c = *nombre++)) {
        valor_hash = ((valor_hash << 5) + valor_hash) + c;
    }
    return valor_hash % CUBETAS_TABLA_SIMBOLOS;
}

TablaSimbolos *crear_tabla_simbolos(TablaSimbolos *padre) {
    TablaSimbolos *nueva_tabla = (TablaSimbolos *)malloc(sizeof(TablaSimbolos));
    if (nueva_tabla == NULL) {
        perror("Error al asignar memoria para la tabla de simbolos");
        exit(EXIT_FAILURE);
    }

    // Ya que 'cubetas' es un arreglo estático dentro de TablaSimbolos,
    // solo necesitas inicializar sus elementos a NULL.
    // NO asignes memoria adicional con malloc/calloc para nueva_tabla->cubetas
    for (int i = 0; i < CUBETAS_TABLA_SIMBOLOS; i++) {
        nueva_tabla->cubetas[i] = NULL;
    }

    nueva_tabla->padre = padre;
    nueva_tabla->id_ambito = siguiente_id_ambito++;

    return nueva_tabla;
}


void destruir_tabla_simbolos(TablaSimbolos *tabla) {
    if (tabla == NULL) return;

    for (int i = 0; i < CUBETAS_TABLA_SIMBOLOS; i++) {
        EntradaSimbolo *entrada = tabla->cubetas[i];
        while (entrada != NULL) {
            EntradaSimbolo *temp = entrada;
            entrada = entrada->siguiente_en_cubeta;
            free(temp->nombre);
            free(temp);
        }
    }

    free(tabla);
}

EntradaSimbolo *agregar_simbolo(TablaSimbolos *tabla, const char *nombre, enum TipoDato tipo, int renglon, int columna) {

    if (tabla == NULL || nombre == NULL) {
        return NULL;
    }

    unsigned int indice = calcular_hash(nombre);
   
    // Verificar si el símbolo ya existe en el ámbito actual
    if (buscar_simbolo_en_ambito_actual(tabla, nombre) != NULL) {
        reportar_error_semantico(renglon, columna, "Símbolo '%s' ya declarado en este ámbito.", nombre);
        return NULL;
    }

    // Crear nueva entrada de símbolo
    EntradaSimbolo *nueva_entrada = (EntradaSimbolo *)malloc(sizeof(EntradaSimbolo));
    if (nueva_entrada == NULL) {
        perror("Error al asignar memoria para EntradaSimbolo");
        exit(EXIT_FAILURE); // O retorna NULL si prefieres manejarlo sin salir
    }

    nueva_entrada->nombre = strdup(nombre); // Duplicar el nombre
    if (nueva_entrada->nombre == NULL) {
        perror("Error al duplicar el nombre del simbolo");
        free(nueva_entrada);
        exit(EXIT_FAILURE); // O retorna NULL
    }
    nueva_entrada->tipo = tipo;

    // Insertar al principio de la lista enlazada en la cubeta
    nueva_entrada->siguiente_en_cubeta = tabla->cubetas[indice];
    tabla->cubetas[indice] = nueva_entrada;

  
    return nueva_entrada;
}
EntradaSimbolo *buscar_simbolo(TablaSimbolos *tabla, const char *nombre) {
    TablaSimbolos *tabla_actual = tabla;
    while (tabla_actual != NULL) {
        unsigned int indice = calcular_hash(nombre);
        EntradaSimbolo *entrada = tabla_actual->cubetas[indice];
        while (entrada != NULL) {
            if (strcmp(entrada->nombre, nombre) == 0) {
                return entrada;
            }
            entrada = entrada->siguiente_en_cubeta;
        }
        tabla_actual = tabla_actual->padre;
    }
    return NULL;
}

EntradaSimbolo *buscar_simbolo_en_ambito_actual(TablaSimbolos *tabla, const char *nombre) {
    if (tabla == NULL || nombre == NULL) return NULL;

    unsigned int indice = calcular_hash(nombre);

    EntradaSimbolo *entrada = tabla->cubetas[indice];
    while (entrada != NULL) {
        if (strcmp(entrada->nombre, nombre) == 0) {
            return entrada;
        }
        entrada = entrada->siguiente_en_cubeta;
    }
    return NULL;
}

void imprimir_tabla_simbolos(TablaSimbolos *tabla) {
    if (tabla == NULL) {
        printf("Tabla de simbolos vacia o NULL.\n");
        return;
    }

    // Imprimir el ámbito actual
    printf("\n--- Contenido del Ambito ID: %d (Padre ID: %d) ---\n",
           tabla->id_ambito, tabla->padre ? tabla->padre->id_ambito : -1);

    int simbolos_encontrados = 0;
    for (int i = 0; i < CUBETAS_TABLA_SIMBOLOS; i++) {
        EntradaSimbolo *entrada = tabla->cubetas[i];
        if (entrada != NULL) {
            printf("  Cubeta %d:\n", i);
            while (entrada != NULL) {
                printf("    - Nombre: '%s', Tipo: %s\n", entrada->nombre, tipoDatoToString(entrada->tipo));
                simbolos_encontrados++;
                entrada = entrada->siguiente_en_cubeta;
            }
        }
    }

    if (simbolos_encontrados == 0) {
        printf("  (Este ambito esta vacio)\n");
    }
    printf("------------------------------------------\n");

    // Recorrer e imprimir los ámbitos padre recursivamente
    if (tabla->padre != NULL) {
        imprimir_tabla_simbolos(tabla->padre);
    }
}