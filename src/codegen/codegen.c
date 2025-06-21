#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "symbols.h"

int ir_current_size = 0;
int ir_capacity = 0;
Quadruple *ir_code;

int next_temp_number = 0;
int next_label_number = 0;

static TablaSimbolos *global_symbol_table_ref;
static TablaSimbolos *ambito_actual = NULL;

#define MAX_LOOP_NESTING 100

static char *break_labels_stack[MAX_LOOP_NESTING];
static char *continue_labels_stack[MAX_LOOP_NESTING];
static int loop_stack_top = -1;

static void push_loop_labels(char *break_label, char *continue_label)
{
    if (loop_stack_top >= MAX_LOOP_NESTING - 1)
    {
        fprintf(stderr, "Error at %d:%d: Error: Exceeded maximum loop nesting level (%d) for code generation.\n", -1, -1, MAX_LOOP_NESTING);
        exit(EXIT_FAILURE);
    }
    loop_stack_top++;
    break_labels_stack[loop_stack_top] = break_label;
    continue_labels_stack[loop_stack_top] = continue_label;
}

static void pop_loop_labels()
{
    if (loop_stack_top < 0)
    {

        fprintf(stderr, "Error at %d:%d: Error interno: Intento de sacar etiquetas de un stack de bucles vacío.\n", -1, -1);
        return;
    }

    loop_stack_top--;
}

static char *get_current_break_label()
{
    if (loop_stack_top < 0)
    {
        fprintf(stderr, "Error at %d:%d: Error semántico: 'Romper' fuera de un bucle.\n", -1, -1);
        return NULL;
    }
    return break_labels_stack[loop_stack_top];
}

static char *get_current_continue_label()
{
    if (loop_stack_top < 0)
    {
        fprintf(stderr, "Error at %d:%d: Error semántico: 'Continuar' fuera de un bucle.\n", -1, -1);
        return NULL;
    }
    return continue_labels_stack[loop_stack_top];
}

static void generate_code_for_node(ASTNode *node);
static char *generate_code_for_expression(ASTNode *expr_node);
static void generate_code_for_statement(ASTNode *stmt_node);
static void generate_code_for_declaration(ASTNode *decl_node);
static void generate_code_for_if_statement(ASTNode *if_node);
static void generate_code_for_while_statement(ASTNode *while_node);
static void generate_code_for_for_statement(ASTNode *for_node);

void generate_intermediate_code(ASTNode *root_ast_node, TablaSimbolos *global_sym_table)
{
    if (!root_ast_node)
    {
        return;
    }
    init_ir_generator();
    global_symbol_table_ref = global_sym_table;
    ambito_actual = global_sym_table;

    generate_code_for_node(root_ast_node);

    emit_quad(IR_HALT, NULL, NULL, NULL);
}

void init_ir_generator()
{
    ir_capacity = INITIAL_IR_CAPACITY;

    ir_code = malloc(ir_capacity * sizeof(Quadruple));

    if (ir_code == NULL)
    {
        fprintf(stderr, "Error: No se pudo asignar memoria para el código intermedio.\n");
        exit(EXIT_FAILURE);
    }
    ir_current_size = 0;
    next_temp_number = 0;
    next_label_number = 0;
}

void emit_quad(IROperation op, const char *arg1, const char *arg2, const char *result)
{
    if (ir_current_size >= ir_capacity)
    {
        ir_capacity *= 2;
        ir_code = (Quadruple *)realloc(ir_code, ir_capacity * sizeof(Quadruple));
        if (ir_code == NULL)
        {
            fprintf(stderr, "Error: No se pudo redimensionar la memoria para el código intermedio.\n");
            exit(EXIT_FAILURE);
        }
    }

    Quadruple *q = &ir_code[ir_current_size];
    q->op = op;
    q->arg1 = (arg1 != NULL) ? strdup(arg1) : NULL;
    q->arg2 = (arg2 != NULL) ? strdup(arg2) : NULL;
    q->result = (result != NULL) ? strdup(result) : NULL;

    ir_current_size++;
}

void free_ir_code()
{
    if (ir_code == NULL)
        return;

    for (int i = 0; i < ir_current_size; i++)
    {
        if (ir_code[i].arg1)
            free(ir_code[i].arg1);
        if (ir_code[i].arg2)
            free(ir_code[i].arg2);
        if (ir_code[i].result)
            free(ir_code[i].result);
    }

    free(ir_code);
    ir_code = NULL;
    ir_current_size = 0;
    ir_capacity = 0;
}

char *new_temp()
{
    static char temp_name_buffer[32];
    sprintf(temp_name_buffer, "t%d", next_temp_number++);
    return strdup(temp_name_buffer);
}

char *new_label()
{
    static char label_name_buffer[32];
    sprintf(label_name_buffer, "L%d", next_label_number++);
    return strdup(label_name_buffer);
}

static void generate_code_for_node(ASTNode *node)
{
    if (!node)
    {
        return;
    }

    switch (node->type)
    {
    case AST_PROGRAMA:
    case AST_LISTA_SENTENCIAS:
    case AST_BLOQUE:
        TablaSimbolos *ambito_anterior = ambito_actual;

        if (ambito_actual->num_hijos > 0)
        {
            ambito_actual = ambito_actual->hijos[0];
        }
        else
        {
        }

        ASTNode *child = node->hijo_izq;
        while (child)
        {
            generate_code_for_node(child);
            child = child->siguiente_hermano;
        }

        ambito_actual = ambito_anterior;
        break;

    case AST_DECLARACION_VAR:
    case AST_DECLARACION_CONST:
        generate_code_for_declaration(node);
        break;

    case AST_ASIGNACION_STMT:
    case AST_MOSTRAR_STMT:
    case AST_LEER_STMT:
        generate_code_for_statement(node);
        break;

    case AST_SI_STMT:
        generate_code_for_if_statement(node);
        break;
    case AST_SINO_STMT:
        generate_code_for_node(node->hijo_izq);
        break;
    case AST_MIENTRAS_STMT:
        generate_code_for_while_statement(node);
        break;
    case AST_PARA_STMT:
        generate_code_for_for_statement(node);
        break;
    case AST_ROMPER_STMT:
    {
        char *break_label = get_current_break_label();
        if (break_label)
        {
            emit_quad(IR_GOTO, NULL, NULL, break_label);
        }
        break;
    }
    case AST_CONTINUAR_STMT:
    {
        char *continue_label = get_current_continue_label();
        if (continue_label)
        {
            emit_quad(IR_GOTO, NULL, NULL, continue_label);
        }
        break;
    }

    case AST_OR_EXPR:
    case AST_AND_EXPR:
    case AST_NOT_EXPR:
    case AST_IGUAL_EXPR:
    case AST_DIFERENTE_EXPR:
    case AST_MENOR_QUE_EXPR:
    case AST_MAYOR_QUE_EXPR:
    case AST_MENOR_IGUAL_EXPR:
    case AST_MAYOR_IGUAL_EXPR:
    case AST_SUMA_EXPR:
    case AST_RESTA_EXPR:
    case AST_MULT_EXPR:
    case AST_DIV_EXPR:
    case AST_MOD_EXPR:
    case AST_NEGACION_UNARIA_EXPR:
    case AST_IDENTIFICADOR:
    case AST_LITERAL_ENTERO:
    case AST_LITERAL_FLOTANTE:
    case AST_LITERAL_CADENA:
    case AST_LITERAL_BOOLEANO:
        fprintf(stderr, "Error at %d:%d: Error interno del compilador: Nodo de expresión procesado como sentencia directamente.\n", node->renglon, node->columna);
        break;

    default:
        fprintf(stderr, "Error at %d:%d: Error interno del compilador: Tipo de nodo AST no reconocido en generación de CI.\n", node->renglon, node->columna);
        break;
    }
}

static char *generate_code_for_expression(ASTNode *expr_node)
{
    if (!expr_node)
    {
        return NULL;
    }

    if (expr_node->ir_result_name != NULL)
    {
        return expr_node->ir_result_name;
    }

    char *result_name = NULL;
    char buffer[256];

    switch (expr_node->type)
    {
    case AST_LITERAL_ENTERO:
        sprintf(buffer, "%d", (int)expr_node->valor.valor_numero);
        result_name = strdup(buffer);
        break;
    case AST_LITERAL_FLOTANTE:
        sprintf(buffer, "%f", expr_node->valor.valor_numero);
        result_name = strdup(buffer);
        break;
    case AST_LITERAL_CADENA:

        result_name = strdup(expr_node->valor.valor_cadena);
        break;
    case AST_LITERAL_BOOLEANO:
        sprintf(buffer, "%d", expr_node->valor.valor_booleano ? 1 : 0);
        result_name = strdup(buffer);
        break;
    case AST_IDENTIFICADOR:
    {

        EntradaSimbolo *symbol = buscar_simbolo_ambitos(ambito_actual, expr_node->valor.nombre_id);
        if (!symbol)
        {

            fprintf(stderr, "Error at %d:%d: Error interno: Identificador '%s' no encontrado en la tabla de símbolos durante la generación de CI.\n", expr_node->renglon, expr_node->columna, expr_node->valor.nombre_id);
            result_name = strdup("ERROR_VAR");
        }
        else if (symbol->es_constante)
        {

            switch (symbol->tipo)
            {
            case INT:
                sprintf(buffer, "%d", symbol->valor_constante.valor_int);
                break;
            case FLOAT:
                sprintf(buffer, "%f", symbol->valor_constante.valor_float);
                break;
            case STRING:
                return strdup(symbol->valor_constante.valor_cadena);
            case BOOL:
                sprintf(buffer, "%d", symbol->valor_constante.valor_bool ? 1 : 0);
                break;
            default:
                fprintf(stderr, "Error at %d:%d: Error interno: Tipo de constante no soportado para generación de CI.\n", expr_node->renglon, expr_node->columna);
                result_name = strdup("ERROR_CONST_TYPE");
                break;
            }
            result_name = strdup(buffer);
        }
        else
        {

            result_name = strdup(expr_node->valor.nombre_id);
        }
        break;
    }

    case AST_NEGACION_UNARIA_EXPR:
    {
        char *operand_name = generate_code_for_expression(expr_node->hijo_izq);
        char *temp = new_temp();
        emit_quad(IR_NEG, operand_name, NULL, temp);
        result_name = temp;
        break;
    }
    case AST_NOT_EXPR:
    {
        char *operand_name = generate_code_for_expression(expr_node->hijo_izq);
        char *temp = new_temp();
        emit_quad(IR_NOT, operand_name, NULL, temp);
        result_name = temp;
        break;
    }

    case AST_SUMA_EXPR:
    case AST_RESTA_EXPR:
    case AST_MULT_EXPR:
    case AST_DIV_EXPR:
    case AST_MOD_EXPR:
    case AST_OR_EXPR:
    case AST_AND_EXPR:
    case AST_IGUAL_EXPR:
    case AST_DIFERENTE_EXPR:
    case AST_MENOR_QUE_EXPR:
    case AST_MAYOR_QUE_EXPR:
    case AST_MENOR_IGUAL_EXPR:
    case AST_MAYOR_IGUAL_EXPR:
    {
        char *left_operand = generate_code_for_expression(expr_node->hijo_izq);
        char *right_operand = generate_code_for_expression(expr_node->hijo_der);
        char *temp = new_temp();

        IROperation op_code;
        switch (expr_node->type)
        {
        case AST_SUMA_EXPR:
            op_code = IR_ADD;
            break;
        case AST_RESTA_EXPR:
            op_code = IR_SUB;
            break;
        case AST_MULT_EXPR:
            op_code = IR_MUL;
            break;
        case AST_DIV_EXPR:
            op_code = IR_DIV;
            break;
        case AST_MOD_EXPR:
            op_code = IR_MOD;
            break;
        case AST_OR_EXPR:
            op_code = IR_OR;
            break;
        case AST_AND_EXPR:
            op_code = IR_AND;
            break;
        case AST_IGUAL_EXPR:
            op_code = IR_EQ;
            break;
        case AST_DIFERENTE_EXPR:
            op_code = IR_NE;
            break;
        case AST_MENOR_QUE_EXPR:
            op_code = IR_LT;
            break;
        case AST_MAYOR_QUE_EXPR:
            op_code = IR_GT;
            break;
        case AST_MENOR_IGUAL_EXPR:
            op_code = IR_LE;
            break;
        case AST_MAYOR_IGUAL_EXPR:
            op_code = IR_GE;
            break;
        default:
            fprintf(stderr, "Error at %d:%d: Error interno: Operador binario desconocido en expresión.\n", expr_node->renglon, expr_node->columna);
            op_code = (IROperation)-1;
            break;
        }
        emit_quad(op_code, left_operand, right_operand, temp);
        result_name = temp;
        break;
    }

    default:
        fprintf(stderr, "Error at %d:%d: Error interno: Tipo de expresión no manejado para generación de CI.\n", expr_node->renglon, expr_node->columna);
        result_name = strdup("ERROR_EXPR");
        break;
    }

    expr_node->ir_result_name = result_name;
    return result_name;
}

static void generate_code_for_statement(ASTNode *stmt_node)
{
    if (!stmt_node)
    {
        return;
    }

    switch (stmt_node->type)
    {
    case AST_ASIGNACION_STMT:
    {

        char *var_name = stmt_node->hijo_izq->valor.nombre_id;
        char *expr_result = generate_code_for_expression(stmt_node->hijo_der);

        emit_quad(IR_ASSIGN, expr_result, NULL, var_name);
        break;
    }
    case AST_MOSTRAR_STMT:
    {

        char *print_arg = generate_code_for_expression(stmt_node->hijo_izq);
        emit_quad(IR_PRINT, print_arg, NULL, NULL);
        break;
    }
    case AST_LEER_STMT:
    {

        char *read_target = stmt_node->hijo_izq->valor.nombre_id;
        emit_quad(IR_READ, NULL, NULL, read_target);
        break;
    }
    default:
        fprintf(stderr, "Error at %d:%d: Error interno: Tipo de sentencia simple no manejado para CI.\n", stmt_node->renglon, stmt_node->columna);
        break;
    }
}

static void generate_code_for_declaration(ASTNode *decl_node)
{
    if (!decl_node)
    {
        return;
    }

    if (decl_node->hijo_der != NULL)
    {

        char *var_name = decl_node->hijo_izq->valor.nombre_id;
        char *expr_result = generate_code_for_expression(decl_node->hijo_der);
        emit_quad(IR_ASSIGN, expr_result, NULL, var_name);
    }
}

static void generate_code_for_if_statement(ASTNode *if_node)
{
    if (!if_node)
    {
        return;
    }

    char *condition_result = generate_code_for_expression(if_node->hijo_izq);

    char *else_label = new_label();
    char *end_if_label = new_label();

    emit_quad(IR_IF_FALSE_GOTO, condition_result, NULL, else_label);

    generate_code_for_node(if_node->hijo_der);

    if (if_node->hijo_der->siguiente_hermano)
    {
        emit_quad(IR_GOTO, NULL, NULL, end_if_label);
    }

    emit_quad(IR_LABEL, NULL, NULL, else_label);

    if (if_node->hijo_der->siguiente_hermano)
    {
        generate_code_for_node(if_node->hijo_der->siguiente_hermano);

        emit_quad(IR_LABEL, NULL, NULL, end_if_label);
    }

    free(condition_result);
    free(else_label);
    free(end_if_label);
}

static void generate_code_for_while_statement(ASTNode *while_node)
{
    if (!while_node)
    {
        return;
    }

    char *loop_start_label = new_label();
    char *loop_end_label = new_label();

    push_loop_labels(loop_end_label, loop_start_label);

    emit_quad(IR_LABEL, NULL, NULL, loop_start_label);

    char *condition_result = generate_code_for_expression(while_node->hijo_izq);

    emit_quad(IR_IF_FALSE_GOTO, condition_result, NULL, loop_end_label);

    generate_code_for_node(while_node->hijo_der);

    emit_quad(IR_GOTO, NULL, NULL, loop_start_label);

    emit_quad(IR_LABEL, NULL, NULL, loop_end_label);

    pop_loop_labels();
}

static void generate_code_for_for_statement(ASTNode *for_node)
{
    if (!for_node)
    {
        return;
    }

    ASTNode *for_params_node = for_node->hijo_izq;
    if (!for_params_node || for_params_node->type != AST_PARA_PARAMS)
    {
        fprintf(stderr, "Error at %d:%d: Error interno: Estructura AST inesperada para el bucle 'Para'.\n", for_node->renglon, for_node->columna);
        return;
    }

    ASTNode *init_node = for_params_node->hijo_izq;
    ASTNode *condition_node = for_params_node->hijo_der;
    ASTNode *increment_node = NULL;
    if (condition_node != NULL)
    {
        increment_node = condition_node->siguiente_hermano;
    }
    ASTNode *body_node = for_node->hijo_der;

    char *loop_condition_label = new_label();
    char *loop_increment_label = new_label();
    char *loop_end_label = new_label();

    push_loop_labels(loop_end_label, loop_increment_label);

    if (init_node)
    {
        generate_code_for_node(init_node);
    }

    emit_quad(IR_LABEL, NULL, NULL, loop_condition_label);

    char *condition_result = NULL;
    if (condition_node != NULL)
    {
        condition_result = generate_code_for_expression(condition_node);
    }
    else
    {
        condition_result = strdup("1"); // condición siempre verdadera
    }

    emit_quad(IR_IF_FALSE_GOTO, condition_result, NULL, loop_end_label);

    generate_code_for_node(body_node);

    emit_quad(IR_LABEL, NULL, NULL, loop_increment_label);
    if (increment_node)
    {
        generate_code_for_expression(increment_node);
    }

    emit_quad(IR_GOTO, NULL, NULL, loop_condition_label);

    emit_quad(IR_LABEL, NULL, NULL, loop_end_label);

    if (condition_result)
        free(condition_result);

    pop_loop_labels();
}

Quadruple *get_ir_code()
{
    return ir_code;
}

int get_ir_code_size()
{
    return ir_current_size;
}

void print_intermediate_code()
{
    printf("\n--- Código Intermedio (Cuádruplos) ---\n");
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple q = ir_code[i];
        printf("%d: (", i);

        switch (q.op)
        {
        case IR_ADD:
            printf("ADD");
            break;
        case IR_SUB:
            printf("SUB");
            break;
        case IR_MUL:
            printf("MUL");
            break;
        case IR_DIV:
            printf("DIV");
            break;
        case IR_MOD:
            printf("MOD");
            break;
        case IR_NEG:
            printf("NEG");
            break;
        case IR_LT:
            printf("LT");
            break;
        case IR_GT:
            printf("GT");
            break;
        case IR_LE:
            printf("LE");
            break;
        case IR_GE:
            printf("GE");
            break;
        case IR_EQ:
            printf("EQ");
            break;
        case IR_NE:
            printf("NE");
            break;
        case IR_AND:
            printf("AND");
            break;
        case IR_OR:
            printf("OR");
            break;
        case IR_NOT:
            printf("NOT");
            break;
        case IR_ASSIGN:
            printf("ASSIGN");
            break;
        case IR_LABEL:
            printf("LABEL");
            break;
        case IR_GOTO:
            printf("GOTO");
            break;
        case IR_IF_FALSE_GOTO:
            printf("IF_FALSE_GOTO");
            break;
        case IR_PRINT:
            printf("PRINT");
            break;
        case IR_READ:
            printf("READ");
            break;
        case IR_HALT:
            printf("HALT");
            break;
        default:
            printf("UNKNOWN_OP");
            break;
        }
        printf(", ");

        printf("%s, ", q.arg1 ? q.arg1 : "NULL");

        printf("%s, ", q.arg2 ? q.arg2 : "NULL");

        printf("%s)\n", q.result ? q.result : "NULL");
    }
    printf("---------------------------------------\n");
}