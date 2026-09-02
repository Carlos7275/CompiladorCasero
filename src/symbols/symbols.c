#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"
#include "types.h"
#include "semantic.h"


/**
 * Calcula el índice hash para una entrada de símbolo dentro del ámbito actual.
 *
 * @param nombre Nombre del símbolo a indexar.
 * @return Índice de cubeta calculado.
 */
unsigned int calcular_hash(const char *nombre) {
    unsigned int hash = 0;
    for (int i = 0; nombre[i] != '\0'; i++) {
        hash = hash * 31 + nombre[i];
    }
    return hash % CUBETAS_TABLA_SIMBOLOS;
}

/**
 * Crea una nueva tabla de símbolos y la registra como hijo del ámbito padre.
 *
 * @param padre Ámbito padre que contendrá la nueva tabla.
 * @return Puntero a la nueva tabla creada.
 */
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


/**
 * Libera toda la jerarquía de tablas de símbolos y sus entradas asociadas.
 *
 * @param tabla Raíz del árbol de ámbitos a liberar.
 */
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

/**
 * Registra un símbolo dentro del ámbito indicado.
 *
 * @param tabla Ámbito donde se agregará el símbolo.
 * @param nombre Nombre del símbolo.
 * @param tipo Tipo de dato asociado.
 * @param renglon Línea donde aparece la declaración.
 * @param columna Columna donde aparece la declaración.
 * @return Entrada del símbolo creada o NULL si ya existe en el mismo ámbito.
 */
EntradaSimbolo *agregar_simbolo(TablaSimbolos *tabla, const char *nombre, enum TipoDato tipo, int renglon, int columna) {
    (void)renglon;
    (void)columna;

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

/**
 * Busca un símbolo únicamente dentro del ámbito actual.
 *
 * @param tabla Tabla de símbolos del ámbito consultado.
 * @param nombre Nombre del símbolo a localizar.
 * @return Entrada del símbolo si existe; de lo contrario NULL.
 */
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

/**
 * Busca un símbolo recorriendo la jerarquía de ámbitos desde el actual hacia los padres.
 *
 * @param tabla Tabla del ámbito actual.
 * @param nombre Nombre del símbolo a localizar.
 * @return Entrada del símbolo encontrado o NULL si no existe.
 */
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

/**
 * Imprime espacios de indentación para la jerarquía de tablas de símbolos.
 *
 * @param nivel Nivel de profundidad del ámbito.
 */
void imprimir_indentacion(int nivel) {
    for (int i = 0; i < nivel * 4; i++) {
        printf(" ");
    }
}

/**
 * Muestra la estructura completa de ámbitos y símbolos en formato legible.
 *
 * @param tabla Tabla raíz a imprimir.
 * @param nivel Profundidad actual en la jerarquía.
 */
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
                        printf(", Valor: %lld", (long long)entrada->valor_constante.valor_int);
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

/**
 * Busca un símbolo revisando el ámbito actual y los ancestros.
 *
 * @param ambito Ámbito de inicio de búsqueda.
 * @param nombre Nombre del símbolo.
 * @return Entrada del símbolo encontrado o NULL.
 */
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
