#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "parser.h"
#include "symbols.h"
#include "semantic.h"
extern const char *DataTypeNames[];

TablaSimbolos *ambito_actual;
struct ErrorSemantico *cabeza_errores = NULL;
int profundidad_loop = 0;

int contador_errores_semanticos = 0;

void reportar_error_semantico(int renglon, int columna, const char *formato, ...)
{
    struct ErrorSemantico *nuevo_error = (struct ErrorSemantico *)malloc(sizeof(struct ErrorSemantico));
    if (nuevo_error == NULL)
    {
        perror("Error al asignar memoria para el error semantico");
        exit(EXIT_FAILURE);
    }

    nuevo_error->renglon = renglon;
    nuevo_error->columna = columna;

    va_list args;
    va_start(args, formato);
    vsnprintf(nuevo_error->mensaje, sizeof(nuevo_error->mensaje), formato, args);
    va_end(args);

    nuevo_error->sig = cabeza_errores;
    cabeza_errores = nuevo_error;
    contador_errores_semanticos++;
}

void imprimir_errores_semanticos()
{
    if (cabeza_errores == NULL)
    {
        printf("No se detectaron errores semanticos.\n");
        return;
    }

    if (contador_errores_semanticos == 0)
    {
        printf("No se detectaron errores semanticos.\n");
        printf("-------------------------------------------\n");
        return;
    }

    struct ErrorSemantico **errores_array = (struct ErrorSemantico **)malloc(
        sizeof(struct ErrorSemantico *) * contador_errores_semanticos);

    if (errores_array == NULL)
    {
        perror("Error: No se pudo asignar memoria para ordenar los errores semanticos. Imprimiendo sin ordenar.");
        struct ErrorSemantico *actual_fallback = cabeza_errores;
        while (actual_fallback != NULL)
        {
            fprintf(stderr, "Error (R%d, C%d): %s\n", actual_fallback->renglon, actual_fallback->columna, actual_fallback->mensaje);
            actual_fallback = actual_fallback->sig;
        }
        printf("-------------------------------------------\n");
        actual_fallback = cabeza_errores;
        while (actual_fallback != NULL)
        {
            struct ErrorSemantico *temp = actual_fallback;
            actual_fallback = actual_fallback->sig;
            free(temp);
        }
        cabeza_errores = NULL;
        return;
    }

    struct ErrorSemantico *actual = cabeza_errores;
    int i = 0;
    while (actual != NULL && i < contador_errores_semanticos)
    {
        errores_array[i++] = actual;
        actual = actual->sig;
    }

    int n = contador_errores_semanticos;
    for (int k = 0; k < n - 1; k++)
    {
        for (int j = 0; j < n - k - 1; j++)
        {
            if (errores_array[j]->renglon > errores_array[j + 1]->renglon ||
                (errores_array[j]->renglon == errores_array[j + 1]->renglon &&
                 errores_array[j]->columna > errores_array[j + 1]->columna))
            {
                struct ErrorSemantico *temp = errores_array[j];
                errores_array[j] = errores_array[j + 1];
                errores_array[j + 1] = temp;
            }
        }
    }

    for (i = 0; i < contador_errores_semanticos; i++)
    {
        fprintf(stderr, "Error (R%d, C%d): %s\n",
                errores_array[i]->renglon, errores_array[i]->columna, errores_array[i]->mensaje);
    }

    printf("-------------------------------------------\n");

    free(errores_array);

    actual = cabeza_errores;
    while (actual != NULL)
    {
        struct ErrorSemantico *temp = actual;
        actual = actual->sig;
        free(temp);
    }
    cabeza_errores = NULL;
}

const char *tipoDatoToString(enum TipoDato tipo)
{
    switch (tipo)
    {
    case INT:
        return "Entero";
    case FLOAT:
        return "Flotante";
    case STRING:
        return "Cadena";
    case CHAR:
        return "Caracter";
    case BOOL:
        return "Booleano";
    case TIPO_VOID:
        return "Void";
    case TIPO_ERROR:
        return "Error";
    default:
        return "Desconocido";
    }
}

void verificar_asignacion(int renglon, int columna, enum TipoDato tipo_destino, enum TipoDato tipo_origen)
{
    if (tipo_destino == TIPO_ERROR || tipo_origen == TIPO_ERROR)
    {
        return;
    }

    if (tipo_destino == tipo_origen)
    {
        return;
    }

    if (tipo_destino == FLOAT && tipo_origen == INT)
    {
        return;
    }

    if (tipo_destino == INT && tipo_origen == FLOAT)
    {
        return;
    }

    reportar_error_semantico(renglon, columna,
                             "Tipos incompatibles en asignacion: no se puede asignar %s a %s.",
                             tipoDatoToString(tipo_origen), tipoDatoToString(tipo_destino));
}

enum TipoDato verificar_expresion_aritmetica(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2)
{
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if (tipo1 == FLOAT || tipo2 == FLOAT)
    {
        if ((tipo1 == INT || tipo1 == FLOAT) && (tipo2 == INT || tipo2 == FLOAT))
        {
            return FLOAT;
        }
    }

    if (tipo1 == INT && tipo2 == INT)
    {
        return INT;
    }

    if (tipo1 == STRING && tipo2 == STRING)
    {
        return STRING;
    }

    reportar_error_semantico(renglon, columna,
                             "Operacion aritmetica con tipos incompatibles: %s y %s.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

enum TipoDato verificar_expresion_comparacion(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2)
{
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if ((tipo1 == INT || tipo1 == FLOAT) &&
        (tipo2 == INT || tipo2 == FLOAT))
    {
        return BOOL;
    }

    if ((tipo1 == STRING && tipo1 == STRING) ||
        (tipo1 == CHAR && tipo2 == CHAR))
    {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Comparacion de tipos incompatibles: %s y %s.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

enum TipoDato verificar_expresion_logica(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2)
{
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if (tipo1 == BOOL && tipo2 == BOOL)
    {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Operacion logica con tipos incompatibles: %s y %s. Se esperaban Booleanos.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

enum TipoDato verificar_negacion_unaria(int renglon, int columna, enum TipoDato tipo)
{
    if (tipo == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if (tipo == INT || tipo == FLOAT)
    {
        return tipo;
    }

    reportar_error_semantico(renglon, columna,
                             "Operador de negacion unaria (-) aplicado a tipo no numerico: %s.",
                             tipoDatoToString(tipo));
    return TIPO_ERROR;
}

enum TipoDato verificar_negacion_logica(int renglon, int columna, enum TipoDato tipo)
{
    if (tipo == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if (tipo == BOOL)
    {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Operador de negacion logica (NOT) aplicado a tipo no booleano: %s.",
                             tipoDatoToString(tipo));
    return TIPO_ERROR;
}

TablaSimbolos *realizar_analisis_semantico(ASTNode *raiz_ast)
{
    ambito_actual = crear_tabla_simbolos(NULL);
    inicializarTablaSimbolos();
    visit_ast_semantic(raiz_ast);
    return ambito_actual;
}

void inicializarTablaSimbolos()
{
    EntradaSimbolo *entrada;

    if (ambito_actual == NULL)
    {

        fprintf(stderr, "Error: La tabla de símbolos global (ambito_actual) no ha sido inicializada.\n");
        exit(EXIT_FAILURE);
    }

    entrada = agregar_simbolo(ambito_actual, "PI", FLOAT, 0, 0);
    if (entrada)
    {
        entrada->es_constante = 1;
        entrada->valor_constante.valor_float = 3.14159265358979323846;
    }
    else
    {
        fprintf(stderr, "Error al insertar 'PI' en la tabla de símbolos.\n");
    }

    entrada = agregar_simbolo(ambito_actual, "E", FLOAT, 0, 0);
    if (entrada)
    {
        entrada->es_constante = 1;
        entrada->valor_constante.valor_float = 2.71828182845904523536;
    }
    else
    {
        fprintf(stderr, "Error al insertar 'E' en la tabla de símbolos.\n");
    }

    entrada = agregar_simbolo(ambito_actual, "MAX_ENTERO", INT, 0, 0);
    if (entrada)
    {
        entrada->es_constante = 1;
        entrada->valor_constante.valor_int = 2147483647;
    }
    else
    {
        fprintf(stderr, "Error al insertar 'MAX_ENTERO' en la tabla de símbolos.\n");
    }

    entrada = agregar_simbolo(ambito_actual, "MAX_FLOTANTE", FLOAT, 0, 0);
    if (entrada)
    {
        entrada->es_constante = 1;
        entrada->valor_constante.valor_float = 3.40282347E+38;
    }
    else
    {
        fprintf(stderr, "Error al insertar 'MAX_FLOTANTE' en la tabla de símbolos.\n");
    }
}

void visit_ast_semantic(ASTNode *node)
{
    if (node == NULL)
    {
        return;
    }

    switch (node->type)
    {
    case AST_PROGRAMA:
    {
        ASTNode *current_child = node->hijo_izq;
        while (current_child != NULL)
        {
            visit_ast_semantic(current_child);
            current_child = current_child->siguiente_hermano;
        }
        break;
    }

    case AST_LISTA_SENTENCIAS:
    case AST_BLOQUE:
    {
        TablaSimbolos *nuevo_ambito = crear_tabla_simbolos(ambito_actual);
        ambito_actual = nuevo_ambito;

        ASTNode *current = node->hijo_izq;
        while (current != NULL)
        {
            visit_ast_semantic(current);
            current = current->siguiente_hermano;
        }

        ambito_actual = ambito_actual->padre;
        break;
    }

    case AST_DECLARACION_VAR:
    {
        const char *nombre_var = node->hijo_izq->valor.nombre_id;
        enum TipoDato tipo_declarado = node->declared_type_info;

        EntradaSimbolo *simbolo_agregado = agregar_simbolo(ambito_actual, nombre_var, tipo_declarado, node->renglon, node->columna);
        if (simbolo_agregado != NULL)
        {
            simbolo_agregado->es_constante = 0;
        }

        if (node->hijo_der != NULL)
        {
            visit_ast_semantic(node->hijo_der);
            enum TipoDato tipo_expr = node->hijo_der->resolved_type;
            verificar_asignacion(node->renglon, node->columna, tipo_declarado, tipo_expr);
        }
        break;
    }

    case AST_DECLARACION_CONST:
    {
        const char *nombre_const = node->hijo_izq->valor.nombre_id;
        enum TipoDato tipo_declarado = node->declared_type_info;

        if (node->hijo_der == NULL)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "La constante '%s' debe ser inicializada.", nombre_const);
            node->resolved_type = TIPO_ERROR;
            break;
        }

        visit_ast_semantic(node->hijo_der);
        enum TipoDato tipo_expr = node->hijo_der->resolved_type;

        if (tipo_expr == TIPO_ERROR)
        {
            node->resolved_type = TIPO_ERROR;
            break;
        }

        verificar_asignacion(node->renglon, node->columna, tipo_declarado, tipo_expr);

        EntradaSimbolo *simbolo_agregado = agregar_simbolo(
            ambito_actual,
            nombre_const,
            tipo_declarado,
            node->renglon,
            node->columna);

        if (simbolo_agregado != NULL)
        {
            simbolo_agregado->es_constante = 1;
            if (node->hijo_der->tipoconstante == CONSTANTE_LITERAL)
            {
                if (tipo_declarado == INT)
                {
                    simbolo_agregado->valor_constante.valor_int = (int)node->hijo_der->valor.valor_numero;
                }
                else if (tipo_declarado == FLOAT)
                {
                    simbolo_agregado->valor_constante.valor_float = node->hijo_der->valor.valor_numero;
                }
                else if (tipo_declarado == STRING)
                {
                    if (node->hijo_der->valor.valor_cadena != NULL)
                    {
                        simbolo_agregado->valor_constante.valor_cadena = strdup(node->hijo_der->valor.valor_cadena);
                    }
                    else
                    {
                        simbolo_agregado->valor_constante.valor_cadena = NULL;
                    }
                }
                else if (tipo_declarado == BOOL)
                {
                    simbolo_agregado->valor_constante.valor_bool = node->hijo_der->valor.valor_booleano;
                }
            }
        }
        node->resolved_type = tipo_declarado;
        break;
    }

    case AST_ASIGNACION_STMT:
    {
        visit_ast_semantic(node->hijo_izq);

        if (node->hijo_izq->type != AST_IDENTIFICADOR)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "El lado izquierdo de la asignacion debe ser un identificador.");
            node->resolved_type = TIPO_ERROR;
        }
        else
        {
            const char *nombre_var = node->hijo_izq->valor.nombre_id;
            EntradaSimbolo *entrada = buscar_simbolo(ambito_actual, nombre_var);

            if (entrada == NULL)
            {
                reportar_error_semantico(node->renglon, node->columna,
                                         "Uso de variable/constante no declarada: '%s'", nombre_var);
                node->resolved_type = TIPO_ERROR;
            }
            else
            {
                if (entrada->es_constante)
                {
                    reportar_error_semantico(node->renglon, node->columna,
                                             "No se puede asignar a la constante '%s'.", nombre_var);
                    node->resolved_type = TIPO_ERROR;
                }

                node->hijo_izq->resolved_type = entrada->tipo;
            }
        }

        visit_ast_semantic(node->hijo_der);

        enum TipoDato tipo_destino = node->hijo_izq->resolved_type;
        enum TipoDato tipo_origen = node->hijo_der->resolved_type;

        if (tipo_destino != TIPO_ERROR && tipo_origen != TIPO_ERROR)
        {
            verificar_asignacion(node->renglon, node->columna, tipo_destino, tipo_origen);
        }

        if (node->resolved_type != TIPO_ERROR && tipo_destino != TIPO_ERROR && tipo_origen != TIPO_ERROR)
        {
            node->resolved_type = tipo_destino;
        }
        else
        {
            node->resolved_type = TIPO_ERROR;
        }

        break;
    }

    case AST_MOSTRAR_STMT:
    case AST_LEER_STMT:
    {
        ASTNode *current_arg = node->hijo_izq;
        while (current_arg != NULL)
        {
            visit_ast_semantic(current_arg);
            current_arg = current_arg->siguiente_hermano;
        }
        break;
    }

    case AST_SI_STMT:
    {
        visit_ast_semantic(node->hijo_izq);
        if (node->hijo_izq->resolved_type != BOOL && node->hijo_izq->resolved_type != TIPO_ERROR)
        {
            reportar_error_semantico(node->hijo_izq->renglon, node->hijo_izq->columna,
                                     "La condicion de la sentencia 'Si' debe ser booleana, se encontro %s.",
                                     tipoDatoToString(node->hijo_izq->resolved_type));
        }
        visit_ast_semantic(node->hijo_der);
        if (node->siguiente_hermano != NULL && node->siguiente_hermano->type == AST_SINO_STMT)
        {
            visit_ast_semantic(node->siguiente_hermano);
        }
        break;
    }

    case AST_SINO_STMT:
        visit_ast_semantic(node->hijo_izq);
        break;

    case AST_MIENTRAS_STMT:
    {

        visit_ast_semantic(node->hijo_izq);
        if (node->hijo_izq->resolved_type != BOOL && node->hijo_izq->resolved_type != TIPO_ERROR)
        {
            reportar_error_semantico(node->hijo_izq->renglon, node->hijo_izq->columna,
                                     "La condicion del bucle 'Mientras' debe ser booleana, se encontro %s.",
                                     tipoDatoToString(node->hijo_izq->resolved_type));
        }
        profundidad_loop++;
        visit_ast_semantic(node->hijo_der);
        profundidad_loop--;
        break;
    }

    case AST_PARA_STMT:
    {
        TablaSimbolos *nuevo_ambito_para = crear_tabla_simbolos(ambito_actual);
        ambito_actual = nuevo_ambito_para;

        ASTNode *for_params_node = node->hijo_izq;
        ASTNode *inicializacion_node = NULL;
        ASTNode *condicion_node = NULL;
        ASTNode *incremento_node = NULL;
        ASTNode *cuerpo_bucle_node = node->hijo_der;

        if (for_params_node != NULL)
        {
            inicializacion_node = for_params_node->hijo_izq;
            if (inicializacion_node != NULL)
            {
                visit_ast_semantic(inicializacion_node);

                condicion_node = inicializacion_node->siguiente_hermano;
                if (condicion_node != NULL)
                {
                    visit_ast_semantic(condicion_node);

                    if (condicion_node->resolved_type != BOOL && condicion_node->resolved_type != TIPO_ERROR)
                    {
                        reportar_error_semantico(condicion_node->renglon, condicion_node->columna,
                                                 "La condicion del bucle 'Para' debe ser booleana, se encontro %s.",
                                                 DataTypeNames[condicion_node->resolved_type]);
                    }

                    incremento_node = condicion_node->siguiente_hermano;
                    if (incremento_node != NULL)
                    {
                        visit_ast_semantic(incremento_node);
                    }
                    else
                    {
                        reportar_error_semantico(node->renglon, node->columna, "El bucle 'Para' requiere una expresion de incremento.");
                    }
                }
                else
                {
                    reportar_error_semantico(node->renglon, node->columna, "La condicion del bucle 'Para' no esta definida.");
                }
            }
            else
            {
                reportar_error_semantico(node->renglon, node->columna, "El bucle 'Para' requiere una inicializacion.");
            }
        }
        else
        {
            reportar_error_semantico(node->renglon, node->columna, "El bucle 'Para' requiere parametros.");
        }

        profundidad_loop++;

        if (cuerpo_bucle_node != NULL)
        {
            visit_ast_semantic(cuerpo_bucle_node);
        }
        else
        {
            reportar_error_semantico(node->renglon, node->columna, "El bucle 'Para' debe tener un cuerpo.");
        }

        profundidad_loop--;

        ambito_actual = ambito_actual->padre;

        break;
    }

    case AST_SUMA_EXPR:
    case AST_RESTA_EXPR:
    case AST_MULT_EXPR:
    case AST_DIV_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        node->resolved_type = verificar_expresion_aritmetica(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type,
            node->hijo_der->resolved_type);
        break;
    }
    case AST_OR_EXPR:
    case AST_AND_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        node->resolved_type = verificar_expresion_logica(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type,
            node->hijo_der->resolved_type);
        break;
    }
    case AST_IGUAL_EXPR:
    case AST_DIFERENTE_EXPR:
    case AST_MENOR_QUE_EXPR:
    case AST_MAYOR_QUE_EXPR:
    case AST_MENOR_IGUAL_EXPR:
    case AST_MAYOR_IGUAL_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        node->resolved_type = verificar_expresion_comparacion(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type,
            node->hijo_der->resolved_type);
        break;
    }

    case AST_NOT_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        node->resolved_type = verificar_negacion_logica(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type);
        break;
    }
    case AST_NEGACION_UNARIA_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        node->resolved_type = verificar_negacion_unaria(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type);
        break;
    }

    case AST_IDENTIFICADOR:
    {
        EntradaSimbolo *entrada = buscar_simbolo(ambito_actual, node->valor.nombre_id);
        if (entrada == NULL)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "Uso de variable/constante no declarada: '%s'", node->valor.nombre_id);
            node->resolved_type = TIPO_ERROR;
        }
        else
        {
            node->resolved_type = entrada->tipo;
        }
        break;
    }
    case AST_LITERAL_ENTERO:
        node->resolved_type = INT;
        node->tipoconstante = CONSTANTE_LITERAL;
        break;
    case AST_LITERAL_FLOTANTE:
        node->resolved_type = FLOAT;
        node->tipoconstante = CONSTANTE_LITERAL;
        break;
    case AST_LITERAL_CADENA:
        node->resolved_type = STRING;
        node->tipoconstante = CONSTANTE_LITERAL;
        break;
    case AST_LITERAL_BOOLEANO:
        node->resolved_type = BOOL;
        node->tipoconstante = CONSTANTE_LITERAL;
        break;

    case AST_ROMPER_STMT:
        if (profundidad_loop <= 0)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "'Romper' debe ser utilizado dentro de un bucle.");
        }
        break;

    case AST_CONTINUAR_STMT:
        if (profundidad_loop <= 0)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "'Continuar' debe ser utilizado dentro de un bucle.");
        }
        break;
    default:
        if (node->hijo_izq != NULL)
        {
            visit_ast_semantic(node->hijo_izq);
        }
        if (node->hijo_der != NULL)
        {
            visit_ast_semantic(node->hijo_der);
        }
        if (node->siguiente_hermano != NULL)
        {
            visit_ast_semantic(node->siguiente_hermano);
        }
        break;
    }
}