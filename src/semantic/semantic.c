#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "parser.h"
#include "symbols.h"
#include "semantic.h"

TablaSimbolos *ambito_actual;
struct ErrorSemantico *cabeza_errores = NULL; // Head of the linked list for errors
int contador_errores_semanticos = 0;   // Counter for the total number of semantic errors

void reportar_error_semantico(int renglon, int columna, const char *formato, ...) {
   struct  ErrorSemantico *nuevo_error = (struct ErrorSemantico *)malloc(sizeof(struct ErrorSemantico));
    if (nuevo_error == NULL) {
        perror("Error al asignar memoria para el error semantico");
        exit(EXIT_FAILURE);
    }

    nuevo_error->renglon = renglon;
    nuevo_error->columna = columna;

    va_list args;
    va_start(args, formato);
    vsnprintf(nuevo_error->mensaje, sizeof(nuevo_error->mensaje), formato, args);
    va_end(args);

    nuevo_error->sig = cabeza_errores; // Add to the beginning of the list
    cabeza_errores = nuevo_error;
    contador_errores_semanticos++;
}

void imprimir_errores_semanticos() {
    if (cabeza_errores == NULL) {
        printf("No se detectaron errores semanticos.\n");
        return;
    }

    printf("\n--- ERRORES SEMANTICOS DETECTADOS (%d) ---\n", contador_errores_semanticos);
    struct ErrorSemantico *actual = cabeza_errores;
    // Iterate and print errors (optionally in reverse order if you prefer oldest first)
    while (actual != NULL) {
        fprintf(stderr, "Error semántico (R%d, C%d): %s\n", actual->renglon, actual->columna, actual->mensaje);
        actual = actual->sig;
    }
    printf("-------------------------------------------\n");
    
    // Free the memory allocated for errors
    actual = cabeza_errores;
    while(actual != NULL) {
       struct ErrorSemantico *temp = actual;
        actual = actual->sig;
        free(temp);
    }
    cabeza_errores = NULL; // Reset head after freeing
}


const char *tipoDatoToString(enum TipoDato tipo) {
    switch (tipo) {
        case INT: return "Entero";
        case FLOAT: return "Flotante";
        case STRING: return "Cadena";
        case CHAR: return "Caracter";
        case BOOL: return "Booleano";
        case TIPO_VOID: return "Void";
        case TIPO_ERROR: return "Error";
        default: return "Desconocido";
    }
}

void verificar_asignacion(int renglon, int columna, enum TipoDato tipo_destino, enum TipoDato tipo_origen) {
    if (tipo_destino == TIPO_ERROR || tipo_origen == TIPO_ERROR) {
        return;
    }

    if (tipo_destino == tipo_origen) {
        return;
    }

    if (tipo_destino == FLOAT && tipo_origen == INT) {
        return;
    }

    reportar_error_semantico(renglon, columna,
                             "Tipos incompatibles en asignacion: no se puede asignar %s a %s.",
                             tipoDatoToString(tipo_origen), tipoDatoToString(tipo_destino));
}

enum TipoDato verificar_expresion_aritmetica(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2) {
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR) {
        return TIPO_ERROR;
    }

    if ((tipo1 == INT || tipo1 == INT) &&
        (tipo2 == INT || tipo2 == INT)) {

        if (tipo1 == FLOAT || tipo2 == FLOAT) {
            return FLOAT;
        } else {
            return INT;
        }
    }

    reportar_error_semantico(renglon, columna,
                             "Operacion aritmetica con tipos incompatibles: %s y %s.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

enum TipoDato verificar_expresion_comparacion(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2) {
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR) {
        return TIPO_ERROR;
    }

    if ((tipo1 == INT || tipo1 == FLOAT) &&
        (tipo2 == INT || tipo2 == FLOAT)) {
        return BOOL;
    }

    if ((tipo1 == STRING && tipo2 == STRING) ||
        (tipo1 == CHAR && tipo2 == CHAR)) {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Comparacion de tipos incompatibles: %s y %s.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

enum TipoDato verificar_expresion_logica(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2) {
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR) {
        return TIPO_ERROR;
    }

    if (tipo1 == BOOL && tipo2 == BOOL) {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Operacion logica con tipos incompatibles: %s y %s. Se esperaban Booleanos.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

enum TipoDato verificar_negacion_unaria(int renglon, int columna, enum TipoDato tipo) {
    if (tipo == TIPO_ERROR) {
        return TIPO_ERROR;
    }

    if (tipo == INT || tipo == FLOAT) {
        return tipo;
    }

    reportar_error_semantico(renglon, columna,
                             "Operador de negacion unaria (-) aplicado a tipo no numerico: %s.",
                             tipoDatoToString(tipo));
    return TIPO_ERROR;
}

enum TipoDato verificar_negacion_logica(int renglon, int columna, enum TipoDato tipo) {
    if (tipo == TIPO_ERROR) {
        return TIPO_ERROR;
    }

    if (tipo == BOOL) {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Operador de negacion logica (NOT) aplicado a tipo no booleano: %s.",
                             tipoDatoToString(tipo));
    return TIPO_ERROR;
}

void realizar_analisis_semantico(ASTNode *raiz_ast) {
    ambito_actual = crear_tabla_simbolos(NULL);

    visit_ast_semantic(raiz_ast);

    destruir_tabla_simbolos(ambito_actual);
}

void visit_ast_semantic(ASTNode *node) {
    if (node == NULL) {
        return;
    }

    TablaSimbolos *ambito_padre_temp;

    switch (node->type) {
        case AST_PROGRAMA:
        case AST_LISTA_SENTENCIAS:
             ASTNode *current_stmt = node->hijo_izq;
            while (current_stmt != NULL) {
                visit_ast_semantic(current_stmt); // Visita la sentencia/declaración actual
                current_stmt = current_stmt->siguiente_hermano; // Pasa al siguiente hermano
            }
            break; // ¡Ya procesamos todos los hijos!
            break;

        case AST_BLOQUE:
        case AST_SI_STMT:
        case AST_SINO_STMT:
        case AST_MIENTRAS_STMT:
        case AST_PARA_STMT: {
            ambito_padre_temp = ambito_actual;
            ambito_actual = crear_tabla_simbolos(ambito_actual);
         
            visit_ast_semantic(node->hijo_izq);
            visit_ast_semantic(node->hijo_der);

            TablaSimbolos *ambito_a_destruir = ambito_actual;
            ambito_actual = ambito_padre_temp;
            destruir_tabla_simbolos(ambito_a_destruir);
            break;
        }

        case AST_DECLARACION_VAR: {
            if (node->hijo_izq == NULL || node->hijo_izq->type != AST_IDENTIFICADOR) {
                reportar_error_semantico(node->renglon, node->columna, "Declaracion de variable sin identificador.");
                break;
            }
            const char *nombre_var = node->hijo_izq->valor.nombre_id;
            enum TipoDato tipo_declarado = node->declared_type_info;

            if (agregar_simbolo(ambito_actual, nombre_var, tipo_declarado,node->renglon,node->columna) == NULL) {
                // The error is already reported inside agregar_simbolo if the symbol already exists
                break;
            }
           

            if (node->hijo_der != NULL) {
                visit_ast_semantic(node->hijo_der);
                verificar_asignacion(node->renglon, node->columna, tipo_declarado, node->hijo_der->resolved_type);
            }
            break;
        }

        case AST_ASIGNACION_STMT: {
            if (node->hijo_izq == NULL || node->hijo_izq->type != AST_IDENTIFICADOR) {
                reportar_error_semantico(node->renglon, node->columna, "Asignacion a un elemento no identificador.");
                break;
            }
            const char *nombre_var_asignacion = node->hijo_izq->valor.nombre_id;

            EntradaSimbolo *entrada_var = buscar_simbolo(ambito_actual, nombre_var_asignacion);
            if (entrada_var == NULL) {
                reportar_error_semantico(node->renglon, node->columna,
                                        "Uso de variable no declarada: '%s'", nombre_var_asignacion);
                node->resolved_type = TIPO_ERROR;
                break;
            }

            visit_ast_semantic(node->hijo_der);

            verificar_asignacion(node->renglon, node->columna, entrada_var->tipo, node->hijo_der->resolved_type);
            
            node->resolved_type = entrada_var->tipo;
            break;
        }

        case AST_MOSTRAR_STMT: {
            ASTNode *arg = node->hijo_izq;
            while (arg != NULL) {
                visit_ast_semantic(arg);
                arg = arg->siguiente_hermano;
            }
            break;
        }
        case AST_LEER_STMT: {
            if (node->hijo_izq == NULL || node->hijo_izq->type != AST_IDENTIFICADOR) {
                reportar_error_semantico(node->renglon, node->columna, "Leer requiere un identificador para almacenar el valor.");
                break;
            }
            const char *nombre_var_leer = node->hijo_izq->valor.nombre_id;
            EntradaSimbolo *entrada_var = buscar_simbolo(ambito_actual, nombre_var_leer);
            if (entrada_var == NULL) {
                reportar_error_semantico(node->renglon, node->columna,
                                        "Uso de variable no declarada en Leer: '%s'", nombre_var_leer);
                break;
            }
           
            break;
        }

        case AST_SUMA_EXPR:
        case AST_RESTA_EXPR:
        case AST_MULT_EXPR:
        case AST_DIV_EXPR: {
            visit_ast_semantic(node->hijo_izq);
            visit_ast_semantic(node->hijo_der);

            node->resolved_type = verificar_expresion_aritmetica(
                node->renglon, node->columna,
                node->hijo_izq->resolved_type,
                node->hijo_der->resolved_type
            );
            
            break;
        }
        case AST_NEGACION_UNARIA_EXPR: {
            visit_ast_semantic(node->hijo_izq);
            node->resolved_type = verificar_negacion_unaria(node->renglon, node->columna, node->hijo_izq->resolved_type);
            break;
        }

        case AST_OR_EXPR:
        case AST_AND_EXPR: {
            visit_ast_semantic(node->hijo_izq);
            visit_ast_semantic(node->hijo_der);
            node->resolved_type = verificar_expresion_logica(
                node->renglon, node->columna,
                node->hijo_izq->resolved_type,
                node->hijo_der->resolved_type
            );
                 
            break;
        }
        case AST_NOT_EXPR: {
            visit_ast_semantic(node->hijo_izq);
            node->resolved_type = verificar_negacion_logica(node->renglon, node->columna, node->hijo_izq->resolved_type);
            break;
        }
        case AST_IGUAL_EXPR:
        case AST_DIFERENTE_EXPR:
        case AST_MENOR_QUE_EXPR:
        case AST_MAYOR_QUE_EXPR:
        case AST_MENOR_IGUAL_EXPR:
        case AST_MAYOR_IGUAL_EXPR: {
            visit_ast_semantic(node->hijo_izq);
            visit_ast_semantic(node->hijo_der);
            node->resolved_type = verificar_expresion_comparacion(
                node->renglon, node->columna,
                node->hijo_izq->resolved_type,
                node->hijo_der->resolved_type
            );
                   
            break;
        }

        case AST_IDENTIFICADOR: {
            EntradaSimbolo *entrada = buscar_simbolo(ambito_actual, node->valor.nombre_id);
            if (entrada == NULL) {
                reportar_error_semantico(node->renglon, node->columna,
                                        "Uso de variable no declarada: '%s'", node->valor.nombre_id);
                node->resolved_type = TIPO_ERROR;
            } else {
                node->resolved_type = entrada->tipo;
            }
            break;
        }
        case AST_LITERAL_ENTERO:
            node->resolved_type = INT;
            break;
        case AST_LITERAL_FLOTANTE:
            node->resolved_type = FLOAT;
            break;
        case AST_LITERAL_CADENA:
            node->resolved_type = STRING;
            break;

        default:
            visit_ast_semantic(node->hijo_izq);
            visit_ast_semantic(node->hijo_der);
            break;
    }
}