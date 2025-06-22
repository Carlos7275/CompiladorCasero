#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "symbols.h"
#include "ctype.h"

#define MAX_BUFFER 1024
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

void generar_codigo_intermedio(ASTNode *root_ast_node, TablaSimbolos *global_sym_table)
{
    if (!root_ast_node)
    {
        return;
    }
    init_ir_generator();
    global_symbol_table_ref = global_sym_table;
    ambito_actual = global_sym_table;

    generate_code_for_node(root_ast_node);
    optimize_ir_code();

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
        ASTNode *current = stmt_node->hijo_izq;
        while (current)
        {
            char *print_arg = generate_code_for_expression(current);
            emit_quad(IR_PRINT, print_arg, NULL, NULL);
            current = current->siguiente_hermano;
        }
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
        return;

    ASTNode *for_params_node = for_node->hijo_izq;
    if (!for_params_node || for_params_node->type != AST_PARA_PARAMS)
    {
        fprintf(stderr, "Error at %d:%d: Estructura AST inesperada para el bucle 'Para'.\n",
                for_node->renglon, for_node->columna);
        return;
    }

    ASTNode *init_node = for_params_node->hijo_izq;
    ASTNode *condition_node = NULL;
    ASTNode *increment_node = NULL;

    if (init_node)
    {
        condition_node = init_node->siguiente_hermano;
        if (condition_node)
            increment_node = condition_node->siguiente_hermano;
    }

    ASTNode *body_node = for_node->hijo_der;

    char *loop_condition_label = new_label();
    char *loop_increment_label = new_label();
    char *loop_end_label = new_label();

    push_loop_labels(loop_end_label, loop_increment_label);

    if (init_node)
        generate_code_for_node(init_node);

    emit_quad(IR_LABEL, NULL, NULL, loop_condition_label);

    char *condition_result = NULL;
    if (condition_node)
        condition_result = generate_code_for_expression(condition_node);
    else
    {
        condition_result = new_temp();
        emit_quad(IR_ASSIGN, "1", NULL, condition_result);
    }

    emit_quad(IR_IF_FALSE_GOTO, condition_result, NULL, loop_end_label);

    generate_code_for_node(body_node);

    emit_quad(IR_LABEL, NULL, NULL, loop_increment_label);

    if (increment_node)
        generate_code_for_node(increment_node);

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

void imprimir_codigo_intermedio()
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

static int es_entero(const char *s)
{
    if (!s)
        return 0;
    for (int i = 0; s[i]; i++)
    {
        if (!isdigit(s[i]) && !(i == 0 && s[i] == '-'))
            return 0;
    }
    return 1;
}

static int es_flotante(const char *s)
{
    if (!s)
        return 0;
    int punto = 0;
    for (int i = 0; s[i]; i++)
    {
        if (s[i] == '.')
            punto++;
        else if (!isdigit(s[i]) && !(i == 0 && s[i] == '-'))
            return 0;
    }
    return punto == 1;
}

static int usar_temp(const char *temp, int desde)
{
    for (int i = desde; i < ir_current_size; i++)
    {
        if ((ir_code[i].arg1 && strcmp(ir_code[i].arg1, temp) == 0) ||
            (ir_code[i].arg2 && strcmp(ir_code[i].arg2, temp) == 0))
        {
            return 1;
        }
    }
    return 0;
}

int es_literal(const char *s)
{
    if (!s)
        return 0;

    char *endptr;
    strtod(s, &endptr);
    return *endptr == '\0';
}
void optimize_ir_code()
{
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];

        if (q->arg1 && q->arg2 && q->result &&
            es_literal(q->arg1) && es_literal(q->arg2))
        {
            double a = atof(q->arg1);
            double b = atof(q->arg2);
            double r;
            int valido = 1;

            switch (q->op)
            {
            case IR_ADD:
                r = a + b;
                break;
            case IR_SUB:
                r = a - b;
                break;
            case IR_MUL:
                r = a * b;
                break;
            case IR_DIV:
                if (b != 0)
                    r = a / b;
                else
                    valido = 0;
                break;
            case IR_MOD:
                if ((int)b != 0)
                    r = (int)a % (int)b;
                else
                    valido = 0;
                break;
            default:
                valido = 0;
                break;
            }

            if (valido)
            {
                char buffer[64];
                if (r == (int)r)
                    sprintf(buffer, "%d", (int)r);
                else
                    sprintf(buffer, "%f", r);

                q->op = IR_ASSIGN;
                free(q->arg1);
                free(q->arg2);
                q->arg1 = strdup(buffer);
                q->arg2 = NULL;
            }
        }

        if (q->op == IR_ASSIGN && q->arg1 && q->result && q->result[0] == 't')
        {
            const char *src = q->arg1;
            const char *dest = q->result;

            for (int j = i + 1; j < ir_current_size; j++)
            {
                Quadruple *q2 = &ir_code[j];

                if (q2->arg1 && strcmp(q2->arg1, dest) == 0)
                {
                    free(q2->arg1);
                    q2->arg1 = strdup(src);
                }
                if (q2->arg2 && strcmp(q2->arg2, dest) == 0)
                {
                    free(q2->arg2);
                    q2->arg2 = strdup(src);
                }

                if (q2->result && strcmp(q2->result, dest) == 0)
                    break;
            }
        }
    }

    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        if (q->result && q->result[0] == 't')
        {
            if (!usar_temp(q->result, i + 1))
            {
                free(q->arg1);
                free(q->arg2);
                free(q->result);
                q->arg1 = q->arg2 = q->result = NULL;
                q->op = -1;
            }
        }
    }

    int nueva_pos = 0;
    for (int i = 0; i < ir_current_size; i++)
    {
        if (ir_code[i].op != -1)
        {
            if (i != nueva_pos)
                ir_code[nueva_pos] = ir_code[i];
            nueva_pos++;
        }
    }
    ir_current_size = nueva_pos;
}

int is_number(const char *s)
{
    if (!s)
        return 0;
    int i = 0;
    if (s[0] == '-' || s[0] == '+')
        i = 1;
    int has_digit = 0, has_dot = 0;
    for (; s[i]; i++)
    {
        if (isdigit(s[i]))
            has_digit = 1;
        else if (s[i] == '.' && !has_dot)
            has_dot = 1;
        else
            return 0;
    }
    return has_digit;
}

int is_string_literal(const char *s)
{
    if (!s)
        return 0;
    int len = strlen(s);
    return (len >= 2 && s[0] == '"' && s[len - 1] == '"');
}

int is_valid_varname(const char *s)
{
    if (!s || !s[0])
        return 0;
    if (!(isalpha(s[0]) || s[0] == '_'))
        return 0;
    for (int i = 1; s[i]; i++)
    {
        if (!(isalnum(s[i]) || s[i] == '_'))
            return 0;
    }
    return 1;
}

int var_declared(const char vars[][64], int count, const char *name)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(vars[i], name) == 0)
            return 1;
    }
    return 0;
}

int string_declared(const char labels[][64], int count, const char *str)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(labels[i], str) == 0)
            return 1;
    }
    return 0;
}

void sanitize_label(const char *input, char *output, int max_len)
{

    int j = 0;
    output[j++] = 's';
    output[j++] = 't';
    output[j++] = 'r';
    output[j++] = '_';
    for (int i = 1; input[i] != '\0' && input[i] != '"' && j < max_len - 1; i++)
    {
        if (isalnum(input[i]) || input[i] == '_')
            output[j++] = input[i];
        else
            output[j++] = '_';
    }
    output[j] = '\0';
}

void print_asm_string_literal(FILE *f, const char *str)
{
    int in_quotes = 0;
    int first = 1;

    while (*str)
    {
        if (*str == '\\')
        {
            str++;
            if (*str == '\0')
                break;

            // Cerrar cadena si está abierta para imprimir byte numérico
            if (in_quotes)
            {
                fprintf(f, "\"");
                in_quotes = 0;
            }

            if (!first)
                fprintf(f, ", ");

            switch (*str)
            {
            case 'n':
                fprintf(f, "10");
                break;
            case 't':
                fprintf(f, "9");
                break;
            case 'r':
                fprintf(f, "13");
                break;
            case 'b':
                fprintf(f, "8");
                break;
            case 'f':
                fprintf(f, "12");
                break;
            case 'v':
                fprintf(f, "11");
                break;
            case '0':
                fprintf(f, "0");
                break;
            case '\\':
                if (!first)
                    fprintf(f, ", ");
                fprintf(f, "\"\\\\\"");
                first = 0;
                break;
            case '"':
                if (!first)
                    fprintf(f, ", ");
                fprintf(f, "\"\\\"\"");
                first = 0;
                break;
            default:
                if (!first)
                    fprintf(f, ", ");
                fprintf(f, "\"\\%c\"", *str);
                first = 0;
                break;
            }
            first = 0;
        }
        else
        {
            // Abrir comillas si no están abiertas
            if (!in_quotes)
            {
                if (!first)
                    fprintf(f, ", ");
                fputc('"', f);
                in_quotes = 1;
            }
            fputc(*str, f);
            first = 0;
        }
        str++;
    }

    if (in_quotes)
        fprintf(f, "\"");
}

const char *strip_quotes(const char *s)
{
    size_t len = strlen(s);
    if (len >= 2 && s[0] == '"' && s[len - 1] == '"')
    {
        static char buffer[1024];
        strncpy(buffer, s + 1, len - 2);
        buffer[len - 2] = '\0';
        return buffer;
    }
    return s;
}
void generate_asm(FILE *f)
{
    char declared_vars[MAX_BUFFER][64];
    int declared_vars_count = 0;

    // Sección .data con formatos
    fprintf(f, "section .data\n");
    fprintf(f, "fmt_int db \"%%ld\", 0\n");
    fprintf(f, "fmt_float db \"%%lf\", 10, 0\n");
    fprintf(f, "fmt_str db \"%%s\", 0\n");
    fprintf(f, "fmt_read_int db \"%%ld\", 0\n");
    fprintf(f, "fmt_read_float db \"%%lf\", 0\n");
    fprintf(f, "fmt_read_str db \"%%255s\", 0\n");

    // Literales string para impresión
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        if (q->op == IR_PRINT && is_string_literal(q->arg1))
        {
            fprintf(f, "str_%d db ", i);
            print_asm_string_literal(f, strip_quotes(q->arg1));
            fprintf(f, ", 0\n");
        }
    }

    // Variables en .bss
    fprintf(f, "section .bss\n");
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        const char *args[] = {q->arg1, q->arg2, q->result};
        for (int j = 0; j < 3; j++)
        {
            const char *var = args[j];
            if (var && is_valid_varname(var) && !var_declared(declared_vars, declared_vars_count, var))
            {
                if (!(var[0] == 'L' && isdigit((unsigned char)var[1])))
                {
                    strcpy(declared_vars[declared_vars_count++], var);
                    EntradaSimbolo *entry = buscar_simbolo(ambito_actual, var);
                    if (entry != NULL && entry->tipo == STRING)
                        fprintf(f, "    %s resb 256\n", var);
                    else
                        fprintf(f, "    %s resq 1\n", var);
                }
            }
        }
    }

    // Código principal
    fprintf(f, "section .text\n");
    fprintf(f, "global main\n");
    fprintf(f, "extern printf\n");
    fprintf(f, "extern scanf\n");

    fprintf(f, "main:\n");
    fprintf(f, "    push rbp\n");
    fprintf(f, "    mov rbp, rsp\n");

    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];

        if (q->op == IR_LABEL)
        {
            fprintf(f, "%s:\n", q->result);
            continue;
        }

        switch (q->op)
        {
        case IR_ASSIGN:
            if (is_number(q->arg1))
                fprintf(f, "    mov rax, %s\n    mov [rel %s], rax\n", q->arg1, q->result);
            else
                fprintf(f, "    mov rax, [rel %s]\n    mov [rel %s], rax\n", q->arg1, q->result);
            break;

        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
        case IR_DIV:
        case IR_MOD:
        {
            const char *op;
            if (q->op == IR_ADD) op = "add";
            else if (q->op == IR_SUB) op = "sub";
            else if (q->op == IR_MUL) op = "imul";
            else if (q->op == IR_DIV || q->op == IR_MOD) op = "idiv";
            else op = "";

            if (q->op == IR_DIV || q->op == IR_MOD)
            {
                if (es_literal(q->arg1))
                    fprintf(f, "    mov rax, %s\n", q->arg1);
                else
                    fprintf(f, "    mov rax, [rel %s]\n", q->arg1);
                fprintf(f, "    cqo\n");

                if (es_literal(q->arg2))
                    fprintf(f, "    mov rbx, %s\n", q->arg2);
                else
                    fprintf(f, "    mov rbx, [rel %s]\n", q->arg2);

                fprintf(f, "    idiv rbx\n");

                if (q->op == IR_DIV)
                    fprintf(f, "    mov [rel %s], rax\n", q->result);
                else
                    fprintf(f, "    mov [rel %s], rdx\n", q->result);
            }
            else
            {
                if (es_literal(q->arg1))
                    fprintf(f, "    mov rax, %s\n", q->arg1);
                else
                    fprintf(f, "    mov rax, [rel %s]\n", q->arg1);

                if (es_literal(q->arg2))
                    fprintf(f, "    %s rax, %s\n", op, q->arg2);
                else
                    fprintf(f, "    %s rax, [rel %s]\n", op, q->arg2);

                fprintf(f, "    mov [rel %s], rax\n", q->result);
            }
            break;
        }

        case IR_NEG:
            if (es_literal(q->arg1))
                fprintf(f, "    mov rax, %s\n", q->arg1);
            else
                fprintf(f, "    mov rax, [rel %s]\n", q->arg1);
            fprintf(f, "    neg rax\n");
            fprintf(f, "    mov [rel %s], rax\n", q->result);
            break;

        case IR_LT:
        case IR_GT:
        case IR_LE:
        case IR_GE:
        case IR_EQ:
        case IR_NE:
        {
            const char *cond;
            switch (q->op)
            {
            case IR_LT: cond = "l"; break;
            case IR_GT: cond = "g"; break;
            case IR_LE: cond = "le"; break;
            case IR_GE: cond = "ge"; break;
            case IR_EQ: cond = "e"; break;
            case IR_NE: cond = "ne"; break;
            default: cond = "e"; break;
            }

            if (is_number(q->arg2))
            {
                fprintf(f, "    mov rax, [rel %s]\n", q->arg1);
                fprintf(f, "    cmp rax, %s\n", q->arg2);
            }
            else
            {
                fprintf(f, "    mov rax, [rel %s]\n", q->arg1);
                fprintf(f, "    cmp rax, [rel %s]\n", q->arg2);
            }
            fprintf(f, "    set%s al\n", cond);
            fprintf(f, "    movzx rax, al\n");
            fprintf(f, "    mov [rel %s], rax\n", q->result);
            break;
        }

        case IR_AND:
            fprintf(f, "    mov rax, [rel %s]\n", q->arg1);
            fprintf(f, "    and rax, [rel %s]\n", q->arg2);
            fprintf(f, "    mov [rel %s], rax\n", q->result);
            break;

        case IR_OR:
            fprintf(f, "    mov rax, [rel %s]\n", q->arg1);
            fprintf(f, "    or rax, [rel %s]\n", q->arg2);
            fprintf(f, "    mov [rel %s], rax\n", q->result);
            break;

        case IR_NOT:
            fprintf(f, "    mov rax, [rel %s]\n", q->arg1);
            fprintf(f, "    cmp rax, 0\n");
            fprintf(f, "    sete al\n");
            fprintf(f, "    movzx rax, al\n");
            fprintf(f, "    mov [rel %s], rax\n", q->result);
            break;

        case IR_PRINT:
        {
            EntradaSimbolo *entry = buscar_simbolo(ambito_actual, q->arg1);

            fprintf(f,
                "    %%ifdef WINDOWS\n"
                "        "); 
            if (is_string_literal(q->arg1))
            {
                fprintf(f,
                    "lea rcx, [rel fmt_str]\n"
                    "        lea rdx, [rel str_%d]\n"
                    "        xor eax, eax\n"
                    "        call printf\n",
                    i);
            }
            else if (is_number(q->arg1))
            {
                fprintf(f,
                    "lea rcx, [rel fmt_int]\n"
                    "        mov rdx, %s\n"
                    "        xor eax, eax\n"
                    "        call printf\n",
                    q->arg1);
            }
            else if (entry != NULL)
            {
                if (entry->tipo == STRING)
                {
                    fprintf(f,
                        "lea rcx, [rel fmt_str]\n"
                        "        lea rdx, [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call printf\n",
                        q->arg1);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                        "lea rcx, [rel fmt_float]\n"
                        "        movsd xmm0, qword [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call printf\n",
                        q->arg1);
                }
                else
                {
                    fprintf(f,
                        "lea rcx, [rel fmt_int]\n"
                        "        mov rdx, [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call printf\n",
                        q->arg1);
                }
            }
            fprintf(f, "    %%else\n        ");
            if (is_string_literal(q->arg1))
            {
                fprintf(f,
                    "lea rdi, [rel str_%d]\n"
                    "        lea rsi, [rel fmt_str]\n"
                    "        xor eax, eax\n"
                    "        call printf\n",
                    i);
            }
            else if (is_number(q->arg1))
            {
                fprintf(f,
                    "mov rsi, %s\n"
                    "        lea rdi, [rel fmt_int]\n"
                    "        xor eax, eax\n"
                    "        call printf\n",
                    q->arg1);
            }
            else if (entry != NULL)
            {
                if (entry->tipo == STRING)
                {
                    fprintf(f,
                        "lea rdi, [rel %s]\n"
                        "        lea rsi, [rel fmt_str]\n"
                        "        xor eax, eax\n"
                        "        call printf\n",
                        q->arg1);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                        "movsd xmm0, qword [rel %s]\n"
                        "        lea rdi, [rel fmt_float]\n"
                        "        mov eax, 1\n"
                        "        call printf\n",
                        q->arg1);
                }
                else
                {
                    fprintf(f,
                        "mov rsi, [rel %s]\n"
                        "        lea rdi, [rel fmt_int]\n"
                        "        xor eax, eax\n"
                        "        call printf\n",
                        q->arg1);
                }
            }
            fprintf(f, "    %%endif\n");
            break;
        }

        case IR_READ:
        {
            EntradaSimbolo *entry = buscar_simbolo(ambito_actual, q->result);

            fprintf(f,
                "    %%ifdef WINDOWS\n"
                "        ");
            if (entry != NULL)
            {
                if (entry->tipo == STRING)
                {
                    fprintf(f,
                        "lea rcx, [rel fmt_read_str]\n"
                        "        lea rdx, [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call scanf\n",
                        q->result);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                        "lea rcx, [rel fmt_read_float]\n"
                        "        lea rdx, [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call scanf\n",
                        q->result);
                }
                else
                {
                    fprintf(f,
                        "lea rcx, [rel fmt_read_int]\n"
                        "        lea rdx, [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call scanf\n",
                        q->result);
                }
            }
            fprintf(f,
                "    %%else\n        ");
            if (entry != NULL)
            {
                if (entry->tipo == STRING)
                {
                    fprintf(f,
                        "lea rdi, [rel fmt_read_str]\n"
                        "        lea rsi, [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call scanf\n",
                        q->result);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                        "lea rdi, [rel fmt_read_float]\n"
                        "        lea rsi, [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call scanf\n",
                        q->result);
                }
                else
                {
                    fprintf(f,
                        "lea rdi, [rel fmt_read_int]\n"
                        "        lea rsi, [rel %s]\n"
                        "        xor eax, eax\n"
                        "        call scanf\n",
                        q->result);
                }
            }
            fprintf(f, "    %%endif\n");
            break;
        }

        case IR_GOTO:
            fprintf(f, "    jmp %s\n", q->result);
            break;

        case IR_IF_FALSE_GOTO:
            if (is_number(q->arg1))
            {
                fprintf(f,
                    "    mov rax, %s\n"
                    "    cmp rax, 0\n"
                    "    je %s\n",
                    q->arg1, q->result);
            }
            else
            {
                fprintf(f,
                    "    mov rax, [rel %s]\n"
                    "    cmp rax, 0\n"
                    "    je %s\n",
                    q->arg1, q->result);
            }
            break;

        case IR_HALT:
            fprintf(f, "    mov eax, 0\n");
            break;

        default:
            fprintf(f, "    ; Operación no implementada: %d\n", q->op);
            break;
        }
    }

    fprintf(f, "    pop rbp\n");

    fprintf(f, "%%ifdef WINDOWS\n");
    fprintf(f, "    extern ExitProcess\n");
    fprintf(f, "    mov ecx, 0\n");
    fprintf(f, "    call ExitProcess\n");
    fprintf(f, "%%else\n");
    fprintf(f, "    ret\n");
    fprintf(f, "%%endif\n");

    fprintf(f, "section .note.GNU-stack noalloc noexec nowrite progbits\n");
}
