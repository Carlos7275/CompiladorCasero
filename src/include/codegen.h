#ifndef CODEGEN_H
#define CODEGEN_H

#include "parser.h"
#include "symbols.h"
#define INITIAL_IR_CAPACITY 128 // Capacidad inicial, puedes ajustarla


typedef enum
{
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    IR_NEG,
    IR_LT,
    IR_GT,
    IR_LE,
    IR_GE,
    IR_EQ,
    IR_NE,
    IR_AND,
    IR_OR,
    IR_NOT,
    IR_ASSIGN,
    IR_LABEL,
    IR_GOTO,
    IR_IF_FALSE_GOTO,

    IR_PRINT,
    IR_READ,
    IR_HALT
} IROperation;

typedef struct
{
    IROperation op;
    char *arg1;
    char *arg2;
    char *result;
} Quadruple;

void init_ir_generator();
/**
 * @brief Inicia el proceso de generación de código intermedio.
 *
 * Esta función es el punto de entrada para la fase de CodeGen.
 * Recorre el Árbol de Sintaxis Abstracta (AST) y produce una secuencia de cuádruplos.
 *
 * @param root_ast_node Un puntero al nodo raíz del AST (típicamente AST_PROGRAMA).
 * @param global_sym_table Un puntero a la tabla de símbolos global, que permite
 * consultar información sobre variables y constantes.
 */
void generate_intermediate_code(ASTNode *root_ast_node, TablaSimbolos *global_sym_table);

/**
 * @brief Emite un cuádruplo y lo añade a la secuencia de código intermedio.
 *
 * Esta función es la interfaz principal para añadir instrucciones al IR.
 *
 * @param op La operación del cuádruplo (ej., IR_ADD, IR_ASSIGN).
 * @param arg1 El primer argumento (nombre de variable, literal, temporal, etc.). Puede ser NULL.
 * @param arg2 El segundo argumento. Puede ser NULL.
 * @param result El resultado de la operación (donde se almacena). Puede ser NULL.
 */
void emit_quad(IROperation op, const char *arg1, const char *arg2, const char *result);

/**
 * @brief Genera y devuelve un nuevo nombre temporal único (ej., "t0", "t1").
 *
 * Cada llamada a esta función garantiza un nombre diferente para un temporal.
 * La memoria para el nombre debe ser liberada por el llamador.
 *
 * @return Un puntero a una cadena de caracteres que representa el nuevo temporal.
 */
char *new_temp();

/**
 * @brief Genera y devuelve un nuevo nombre de etiqueta único (ej., "L0", "L1").
 *
 * Útil para la generación de saltos condicionales e incondicionales.
 * La memoria para el nombre debe ser liberada por el llamador.
 *
 * @return Un puntero a una cadena de caracteres que representa la nueva etiqueta.
 */
char *new_label();

/**
 * @brief Devuelve un puntero al arreglo global de cuádruplos generados.
 * @return Un puntero al inicio del arreglo de Quadruple.
 */
Quadruple *get_ir_code();

/**
 * @brief Devuelve el número total de cuádruplos generados hasta el momento.
 * @return El número de cuádruplos.
 */
int get_ir_code_size();

/**
 * @brief Imprime el código intermedio generado a la salida estándar.
 */
void print_intermediate_code();


#endif