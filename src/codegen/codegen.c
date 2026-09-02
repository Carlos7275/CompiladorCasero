#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>

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
static ASTNode *program_root = NULL;
static char *current_return_value = NULL;
static enum TipoDato current_return_type = TIPO_ERROR;

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

static void pop_loop_labels(void)
{
    if (loop_stack_top < 0)
    {

        fprintf(stderr, "Error at %d:%d: Error interno: Intento de sacar etiquetas de un stack de bucles vacío.\n", -1, -1);
        return;
    }

    loop_stack_top--;
}

static char *get_current_break_label(void)
{
    if (loop_stack_top < 0)
    {
        fprintf(stderr, "Error at %d:%d: Error semántico: 'Romper' fuera de un bucle.\n", -1, -1);
        return NULL;
    }
    return break_labels_stack[loop_stack_top];
}

static char *get_current_continue_label(void)
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
static char *new_float_temp(void);
static int es_operando_flotante(const char *s);
static void cargar_float_en_xmm(FILE *f, const char *operando, const char *registro);
static int usar_temp(const char *temp, int desde);
int es_literal(const char *s);
int is_number(const char *s);
int is_string_literal(const char *s);
static int es_funcion_nativa(const char *nombre);
static char *resolver_entorno(const char *operando);
int is_valid_varname(const char *s);

void generar_codigo_intermedio(ASTNode *root_ast_node, TablaSimbolos *global_sym_table)
{
    if (!root_ast_node)
    {
        return;
    }
    init_ir_generator();
    global_symbol_table_ref = global_sym_table;
    ambito_actual = global_sym_table;
    program_root = root_ast_node;

    generate_code_for_node(root_ast_node);
    optimize_ir_code();

    emit_quad(IR_HALT, NULL, NULL, NULL);
}

/**
 * Inicializa la estructura del generador de código intermedio.
 */
void init_ir_generator(void)
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

/**
 * Agrega una cuádrupla al buffer del IR.
 *
 * @param op Operación a registrar.
 * @param arg1 Primer operando.
 * @param arg2 Segundo operando.
 * @param result Destino o resultado de la operación.
 */
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

/**
 * Libera la memoria del código intermedio generado.
 */
void free_ir_code(void)
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

/**
 * Crea un nombre temporal único para un valor intermedio.
 *
 * @return Nombre temporal asignado.
 */
char *new_temp(void)
{
    static char temp_name_buffer[32];
    sprintf(temp_name_buffer, "t%d", next_temp_number++);
    return strdup(temp_name_buffer);
}

static char *new_float_temp(void)
{
    static char temp_name_buffer[32];
    sprintf(temp_name_buffer, "f%d", next_temp_number++);
    return strdup(temp_name_buffer);
}

static char *new_string_temp(void)
{
    static char temp_name_buffer[32];
    sprintf(temp_name_buffer, "s%d", next_temp_number++);
    return strdup(temp_name_buffer);
}

/**
 * Genera una etiqueta única para saltos y bifurcaciones del IR.
 *
 * @return Nombre de etiqueta generado.
 */
char *new_label(void)
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
    {
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
    }

    case AST_DECLARACION_VAR:
    case AST_DECLARACION_CONST:
        generate_code_for_declaration(node);
        break;
    case AST_DECLARACION_TIPO:
        break;
    case AST_FUNCION:
        /* Functions are expanded at call sites by the compact backend. */
        break;
    case AST_RETORNAR_STMT:
        if (node->hijo_izq) {
            char *v = generate_code_for_expression(node->hijo_izq);
            if (node->hijo_izq->resolved_type == STRING)
                current_return_value = v;
            else
            {
                emit_quad(IR_ASSIGN, v, NULL, "__return");
                current_return_value = "__return";
            }
        }
        break;
    case AST_LLAMADA:
        (void)generate_code_for_expression(node);
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
        snprintf(buffer, sizeof(buffer), "%" PRId64, expr_node->valor.valor_entero);
        result_name = strdup(buffer);
        break;
    case AST_LITERAL_FLOTANTE:
    {
        uint64_t bits;
        memcpy(&bits, &expr_node->valor.valor_numero, sizeof(bits));
        sprintf(buffer, "__float_%llx", (unsigned long long)bits);
        result_name = strdup(buffer);
        break;
    }
    case AST_LITERAL_CADENA:

        result_name = strdup(expr_node->valor.valor_cadena);
        break;
    case AST_LITERAL_BOOLEANO:
        sprintf(buffer, "%d", expr_node->valor.valor_booleano ? 1 : 0);
        result_name = strdup(buffer);
        break;
    case AST_LLAMADA:
    {
        const char *native = expr_node->hijo_izq->valor.nombre_id;
        if (strcmp(native, "Entorno") == 0) {
            result_name = resolver_entorno(expr_node->hijo_der
                                               ? generate_code_for_expression(expr_node->hijo_der)
                                               : "\"\"");
            break;
        }
        if (strcmp(native, "ExisteEntorno") == 0) {
            char *clave = expr_node->hijo_der
                              ? generate_code_for_expression(expr_node->hijo_der)
                              : strdup("\"\"");
            char *valor = resolver_entorno(clave);
            result_name = new_temp();
            emit_quad(IR_ASSIGN, valor && strcmp(valor, "\"\"") != 0 ? "1" : "0", NULL, result_name);
            free(clave);
            free(valor);
            break;
        }
        if (es_funcion_nativa(native)) {
            char *values[4] = {0};
            size_t len = 1;
            int count = 0;
            for (ASTNode *a = expr_node->hijo_der; a && count < 4; a = a->siguiente_hermano) {
                values[count] = generate_code_for_expression(a);
                len += strlen(values[count]) + 1;
                count++;
            }
            char *args = calloc(len, 1);
            for (int n = 0; n < count; n++) {
                if (n > 0) strcat(args, "|");
                strcat(args, values[n]);
            }
            result_name = (expr_node->resolved_type == FLOAT) ? new_float_temp() :
                          (expr_node->resolved_type == STRING ? new_string_temp() : new_temp());
            emit_quad(IR_NATIVE_CALL, native, args, result_name);
            for (int n = 0; n < count; n++) free(values[n]);
            free(args);
            break;
        }
        ASTNode *fn = NULL, *c = program_root ? program_root->hijo_izq : NULL;
        while (c) {
            if (c->type == AST_FUNCION && c->hijo_izq &&
                strcmp(c->hijo_izq->valor.nombre_id, expr_node->hijo_izq->valor.nombre_id) == 0) { fn = c; break; }
            c = c->siguiente_hermano;
        }
        if (!fn) { result_name = strdup("0"); break; }
        ASTNode *p = fn->parametros, *a = expr_node->hijo_der;
        while (p && a) {
            emit_quad(IR_ASSIGN, generate_code_for_expression(a), NULL, p->hijo_izq->valor.nombre_id);
            p = p->siguiente_hermano; a = a->siguiente_hermano;
        }
        current_return_value = NULL;
        enum TipoDato previous_return_type = current_return_type;
        current_return_type = fn->return_type;
        generate_code_for_node(fn->hijo_der);
        current_return_type = previous_return_type;
        if (current_return_value) {
            if (fn->return_type == STRING)
                result_name = strdup(current_return_value);
            else
            {
                result_name = (fn->return_type == FLOAT) ? new_float_temp() : new_temp();
                emit_quad(IR_ASSIGN, current_return_value, NULL, result_name);
            }
        } else result_name = strdup("0");
        break;
    }
    case AST_IDENTIFICADOR:
    {

        EntradaSimbolo *symbol = buscar_simbolo_ambitos(ambito_actual, expr_node->valor.nombre_id);
        if (!symbol)
        {

            result_name = strdup(expr_node->valor.nombre_id);
        }
        else if (symbol->es_constante)
        {

            switch (symbol->tipo)
            {
            case INT:
                snprintf(buffer, sizeof(buffer), "%" PRId64, symbol->valor_constante.valor_int);
                break;
            case FLOAT:
            {
                uint64_t bits;
                memcpy(&bits, &symbol->valor_constante.valor_float, sizeof(bits));
                sprintf(buffer, "__float_%llx", (unsigned long long)bits);
                break;
            }
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
        char *temp = (expr_node->resolved_type == FLOAT) ? new_float_temp() : new_temp();
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
        char *temp = (expr_node->resolved_type == FLOAT ||
                      es_operando_flotante(left_operand) ||
                      es_operando_flotante(right_operand))
                         ? new_float_temp()
                         : new_temp();

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

static int es_funcion_nativa(const char *nombre)
{
    static const char *nombres[] = {
        "Abs", "Absoluto", "Min", "Max", "Potencia", "RaizCuadrada",
        "Seno", "Coseno", "Tangente", "Logaritmo", "Exponencial",
        "Piso", "Techo", "Redondear", "Longitud", "Comparar", "Contiene"
        , "Aleatorio", "AleatorioEntre", "Entorno", "ExisteEntorno",
        "Http", "HttpCuerpo", "HttpCabeceras"
    };
    for (size_t i = 0; i < sizeof(nombres) / sizeof(nombres[0]); i++)
        if (strcmp(nombre, nombres[i]) == 0) return 1;
    return 0;
}

static char *resolver_entorno(const char *operando)
{
    if (!operando || strlen(operando) < 2 ||
        operando[0] != '"' || operando[strlen(operando) - 1] != '"')
        return strdup("\"\"");
    size_t key_len = strlen(operando) - 2;
    char *key = malloc(key_len + 1);
    memcpy(key, operando + 1, key_len);
    key[key_len] = '\0';
    const char *value = getenv(key);
    free(key);
    if (!value) return strdup("\"\"");

    char *result = malloc(strlen(value) * 2 + 3);
    size_t j = 0;
    result[j++] = '"';
    for (size_t i = 0; value[i] != '\0'; i++) {
        if (value[i] == '"' || value[i] == '\\') result[j++] = '\\';
        result[j++] = value[i];
    }
    result[j++] = '"';
    result[j] = '\0';
    return result;
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

Quadruple *get_ir_code(void)
{
    return ir_code;
}

int get_ir_code_size(void)
{
    return ir_current_size;
}

void imprimir_codigo_intermedio(void)
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
        case IR_NATIVE_CALL:
            printf("NATIVE_CALL");
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

static int es_operando_flotante(const char *s)
{
    if (!s)
        return 0;
    if (strncmp(s, "__float_", 8) == 0 || s[0] == 'f' ||
        (strcmp(s, "__return") == 0 && current_return_type == FLOAT))
        return 1;
    EntradaSimbolo *entry = buscar_simbolo_ambitos(ambito_actual, s);
    return entry != NULL && entry->tipo == FLOAT;
}

static void cargar_float_en_xmm(FILE *f, const char *operando, const char *registro)
{
    if (strncmp(operando, "__float_", 8) == 0)
    {
        unsigned long long bits = 0;
        sscanf(operando + 8, "%llx", &bits);
        fprintf(f, "    mov rax, 0x%llx\n    movq %s, rax\n",
                bits, registro);
    }
    else
    {
        fprintf(f, "    movsd %s, qword [rel %s]\n", registro, operando);
    }
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
static int es_temporal(const char *s)
{
    return s && s[0] == 't' && s[1] != '\0';
}

static void sustituir_uso_temporal(char *dst, const char *src)
{
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        if (q->arg1 && strcmp(q->arg1, dst) == 0)
        {
            free(q->arg1);
            q->arg1 = strdup(src);
        }
        if (q->arg2 && strcmp(q->arg2, dst) == 0)
        {
            free(q->arg2);
            q->arg2 = strdup(src);
        }
    }
}

void optimize_ir_code(void)
{
    int cambio = 1;
    while (cambio)
    {
        cambio = 0;

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
                    if (b != 0.0)
                        r = a / b;
                    else
                        valido = 0;
                    break;
                case IR_MOD:
                    if (b != 0.0)
                        r = (double)((long long)a % (long long)b);
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
                    if (fabs(r - round(r)) < 1e-9)
                        snprintf(buffer, sizeof(buffer), "%lld", (long long)llround(r));
                    else
                        snprintf(buffer, sizeof(buffer), "%0.10f", r);

                    q->op = IR_ASSIGN;
                    free(q->arg1);
                    free(q->arg2);
                    q->arg1 = strdup(buffer);
                    q->arg2 = NULL;
                    cambio = 1;
                }
            }

            if (q->op == IR_ASSIGN && q->arg1 && q->result && es_temporal(q->result))
            {
                const char *src = q->arg1;
                int usado = 0;

                for (int j = i + 1; j < ir_current_size; j++)
                {
                    Quadruple *q2 = &ir_code[j];
                    if ((q2->arg1 && strcmp(q2->arg1, q->result) == 0) ||
                        (q2->arg2 && strcmp(q2->arg2, q->result) == 0) ||
                        (q2->result && strcmp(q2->result, q->result) == 0))
                    {
                        usado = 1;
                        break;
                    }
                }

                if (!usado)
                {
                    free(q->result);
                    q->result = NULL;
                    q->op = -1;
                    cambio = 1;
                    continue;
                }

                if (es_temporal(src) || is_valid_varname(src) || is_number(src) || is_string_literal(src) || es_literal(src))
                {
                    sustituir_uso_temporal(q->result, src);
                    q->op = -1;
                    cambio = 1;
                }
            }
        }

        int nueva_pos = 0;
        for (int i = 0; i < ir_current_size; i++)
        {
            if (ir_code[i].op != (IROperation)-1)
            {
                if (i != nueva_pos)
                    ir_code[nueva_pos] = ir_code[i];
                nueva_pos++;
            }
        }
        ir_current_size = nueva_pos;
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
        if (ir_code[i].op != (IROperation)-1)
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
    else if (first)
        fprintf(f, "\"\"");
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
    int uses_print = 0;
    int uses_read = 0;
    int uses_float_print = 0;
    int uses_string_print = 0;
    int uses_int_print = 0;
    int uses_float_read = 0;
    int uses_string_read = 0;
    int uses_int_read = 0;
    int uses_strlen = 0;
    int uses_strcmp = 0;
    int uses_strstr = 0;
    int uses_sqrt = 0;
    int uses_sin = 0;
    int uses_cos = 0;
    int uses_tan = 0;
    int uses_log = 0;
    int uses_exp = 0;
    int uses_floor = 0;
    int uses_ceil = 0;
    int uses_pow = 0;
    int uses_round = 0;
    int uses_rand = 0;
    int uses_http_request = 0;
    int uses_http_body = 0;
    int uses_http_headers = 0;
    const char *data_section = "section .data\n";
    const char *bss_section = "section .bss\n";
    const char *text_section = "section .text\n";
    const char *printf_sym = "printf";
    const char *scanf_sym = "scanf";
    const char *main_sym = "main";

    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        if (q->op == IR_PRINT)
        {
            uses_print = 1;
            if (is_string_literal(q->arg1))
                uses_string_print = 1;
            else if (es_operando_flotante(q->arg1))
                uses_float_print = 1;
            else
                uses_int_print = 1;
        }
        else if (q->op == IR_READ)
        {
            uses_read = 1;
            EntradaSimbolo *entry = buscar_simbolo_ambitos(ambito_actual, q->result);
            if (entry != NULL && entry->tipo == STRING)
                uses_string_read = 1;
            else if (entry != NULL && entry->tipo == FLOAT)
                uses_float_read = 1;
            else
                uses_int_read = 1;
        }
        else if (q->op == IR_NATIVE_CALL)
        {
            const char *native = q->arg1;
            if (strcmp(native, "Longitud") == 0)
                uses_strlen = 1;
            else if (strcmp(native, "Comparar") == 0)
                uses_strcmp = 1;
            else if (strcmp(native, "Contiene") == 0)
                uses_strstr = 1;
            else if (strcmp(native, "Potencia") == 0)
                uses_pow = 1;
            else if (strcmp(native, "RaizCuadrada") == 0)
                uses_sqrt = 1;
            else if (strcmp(native, "Seno") == 0)
                uses_sin = 1;
            else if (strcmp(native, "Coseno") == 0)
                uses_cos = 1;
            else if (strcmp(native, "Tangente") == 0)
                uses_tan = 1;
            else if (strcmp(native, "Logaritmo") == 0)
                uses_log = 1;
            else if (strcmp(native, "Exponencial") == 0)
                uses_exp = 1;
            else if (strcmp(native, "Piso") == 0)
                uses_floor = 1;
            else if (strcmp(native, "Techo") == 0)
                uses_ceil = 1;
            else if (strcmp(native, "Redondear") == 0)
                uses_round = 1;
            else if (strcmp(native, "Aleatorio") == 0 ||
                     strcmp(native, "AleatorioEntre") == 0)
                uses_rand = 1;
            else if (strcmp(native, "Http") == 0)
                uses_http_request = 1;
            else if (strcmp(native, "HttpCuerpo") == 0)
                uses_http_body = 1;
            else if (strcmp(native, "HttpCabeceras") == 0)
                uses_http_headers = 1;
        }
    }

#if defined(__APPLE__)
    data_section = "section __DATA,__data\n";
    bss_section = "section __DATA,__bss\n";
    text_section = "section __TEXT,__text\n";
    printf_sym = "_printf";
    scanf_sym = "_scanf";
    main_sym = "_main";
#endif

    // Sección .data con formatos
    fprintf(f, "%s", data_section);
    if (uses_int_print) fprintf(f, "fmt_int db \"%%lld\", 0\n");
    if (uses_float_print) fprintf(f, "fmt_float db \"%%f\", 0\n");
    if (uses_string_print) fprintf(f, "fmt_str db \"%%s\", 0\n");
    if (uses_int_read) fprintf(f, "fmt_read_int db \"%%lld\", 0\n");
    if (uses_float_read) fprintf(f, "fmt_read_float db \"%%lf\", 0\n");
    if (uses_string_read) fprintf(f, "fmt_read_str db \"%%255s\", 0\n");

    // Literales string (buscar en todas las operaciones)
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        // Buscar cadenas en arg1 y arg2
        if (is_string_literal(q->arg1))
        {
            fprintf(f, "str_%d db ", i);
            print_asm_string_literal(f, strip_quotes(q->arg1));
            fprintf(f, ", 0\n");
        }
        if (is_string_literal(q->arg2))
        {
            fprintf(f, "str_%d_2 db ", i);
            print_asm_string_literal(f, strip_quotes(q->arg2));
            fprintf(f, ", 0\n");
        }
        if (q->op == IR_NATIVE_CALL && q->arg2) {
            char args[512];
            snprintf(args, sizeof(args), "%s", q->arg2);
            char *arg = strtok(args, "|");
            int arg_index = 0;
            while (arg) {
                if (is_string_literal(arg)) {
                    fprintf(f, "str_native_%d_%d db ", i, arg_index);
                    print_asm_string_literal(f, strip_quotes(arg));
                    fprintf(f, ", 0\n");
                }
                arg = strtok(NULL, "|");
                arg_index++;
            }
        }
    }

    // Variables en .bss
    fprintf(f, "%s", bss_section);
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        const char *args[] = {
            q->op == IR_NATIVE_CALL ? NULL : q->arg1,
            q->arg2, q->result
        };
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
        if (q->op == IR_NATIVE_CALL && q->arg2) {
            char native_args[512];
            snprintf(native_args, sizeof(native_args), "%s", q->arg2);
            char *arg = strtok(native_args, "|");
            while (arg) {
                if (is_valid_varname(arg) &&
                    !var_declared(declared_vars, declared_vars_count, arg)) {
                    strcpy(declared_vars[declared_vars_count++], arg);
                    EntradaSimbolo *entry = buscar_simbolo(ambito_actual, arg);
                    if (entry && entry->tipo == STRING)
                        fprintf(f, "    %s resb 256\n", arg);
                    else
                        fprintf(f, "    %s resq 1\n", arg);
                }
                arg = strtok(NULL, "|");
            }
        }
    }

    // Código principal
    fprintf(f, "%s", text_section);
    fprintf(f, "global %s\n", main_sym);
    if (uses_print) fprintf(f, "extern %s\n", printf_sym);
    if (uses_read) fprintf(f, "extern %s\n", scanf_sym);
    {
        const char *native_prefix =
#if defined(__APPLE__)
            "_";
#else
            "";
#endif
        if (uses_strlen) fprintf(f, "extern %sstrlen\n", native_prefix);
        if (uses_strcmp) fprintf(f, "extern %sstrcmp\n", native_prefix);
        if (uses_strstr) fprintf(f, "extern %sstrstr\n", native_prefix);
        if (uses_sqrt) fprintf(f, "extern %ssqrt\n", native_prefix);
        if (uses_sin) fprintf(f, "extern %ssin\n", native_prefix);
        if (uses_cos) fprintf(f, "extern %scos\n", native_prefix);
        if (uses_tan) fprintf(f, "extern %stan\n", native_prefix);
        if (uses_log) fprintf(f, "extern %slog\n", native_prefix);
        if (uses_exp) fprintf(f, "extern %sexp\n", native_prefix);
        if (uses_floor) fprintf(f, "extern %sfloor\n", native_prefix);
        if (uses_ceil) fprintf(f, "extern %sceil\n", native_prefix);
        if (uses_pow) fprintf(f, "extern %spow\n", native_prefix);
        if (uses_round) fprintf(f, "extern %sround\n", native_prefix);
        if (uses_rand) fprintf(f, "extern %srand\n", native_prefix);
        if (uses_http_request)
            fprintf(f, "extern %smx_http_request\n", native_prefix);
        if (uses_http_body)
            fprintf(f, "extern %smx_http_body\n", native_prefix);
        if (uses_http_headers)
            fprintf(f, "extern %smx_http_headers\n", native_prefix);
    }

    fprintf(f, "%s:\n", main_sym);
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
            if (es_operando_flotante(q->arg1))
            {
                cargar_float_en_xmm(f, q->arg1, "xmm0");
                fprintf(f, "    movsd [rel %s], xmm0\n", q->result);
            }
            else if (is_number(q->arg1))
            {
                fprintf(f, "    mov rax, %s\n    mov [rel %s], rax\n", q->arg1, q->result);
            }
            else if (is_string_literal(q->arg1))
            {
                // Copiar cadena byte a byte
                EntradaSimbolo *entry_result = buscar_simbolo(ambito_actual, q->result);
                if (entry_result != NULL && entry_result->tipo == STRING)
                {
                    // Generar código para copiar la cadena
                    fprintf(f,
                        "    lea rsi, [rel str_%d]\n"
                        "    lea rdi, [rel %s]\n"
                        "    xor rcx, rcx\n"
                        ".copy_str_%d:\n"
                        "    lodsb\n"
                        "    stosb\n"
                        "    test al, al\n"
                        "    jnz .copy_str_%d\n",
                        i, q->result, i, i);
                }
                else
                {
                    // Si no es STRING, copiar la dirección (para compatibilidad)
                    fprintf(f, "    lea rax, [rel str_%d]\n    mov [rel %s], rax\n", i, q->result);
                }
            }
            else if (q->arg1[0] == 's' && isdigit((unsigned char)q->arg1[1]) &&
                     buscar_simbolo(ambito_actual, q->result) != NULL &&
                     buscar_simbolo(ambito_actual, q->result)->tipo == STRING)
            {
                fprintf(f,
                    "    mov rsi, [rel %s]\n"
                    "    lea rdi, [rel %s]\n"
                    "    xor rcx, rcx\n"
                    ".copy_native_str_%d:\n"
                    "    mov al, [rsi + rcx]\n"
                    "    mov [rdi + rcx], al\n"
                    "    inc rcx\n"
                    "    test al, al\n"
                    "    jnz .copy_native_str_%d\n",
                    q->arg1, q->result, i, i);
            }
            else
            {
                fprintf(f, "    mov rax, [rel %s]\n    mov [rel %s], rax\n", q->arg1, q->result);
            }
            break;

        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
        case IR_DIV:
        case IR_MOD:
        {
            if (es_operando_flotante(q->arg1) || es_operando_flotante(q->arg2))
            {
                if (q->op == IR_MOD)
                {
                    fprintf(stderr, "Error: el operador %% no admite operandos flotantes.\n");
                    exit(EXIT_FAILURE);
                }
                cargar_float_en_xmm(f, q->arg1, "xmm0");
                cargar_float_en_xmm(f, q->arg2, "xmm1");
                if (q->op == IR_ADD) fprintf(f, "    addsd xmm0, xmm1\n");
                else if (q->op == IR_SUB) fprintf(f, "    subsd xmm0, xmm1\n");
                else if (q->op == IR_MUL) fprintf(f, "    mulsd xmm0, xmm1\n");
                else fprintf(f, "    divsd xmm0, xmm1\n");
                fprintf(f, "    movsd [rel %s], xmm0\n", q->result);
                break;
            }
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
            if (es_operando_flotante(q->arg1))
            {
                fprintf(f, "    pxor xmm1, xmm1\n");
                cargar_float_en_xmm(f, q->arg1, "xmm0");
                fprintf(f, "    subsd xmm1, xmm0\n    movsd [rel %s], xmm1\n", q->result);
                break;
            }
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

            if (es_operando_flotante(q->arg1) || es_operando_flotante(q->arg2))
            {
                const char *float_cond;
                switch (q->op)
                {
                case IR_LT: float_cond = "b"; break;
                case IR_GT: float_cond = "a"; break;
                case IR_LE: float_cond = "be"; break;
                case IR_GE: float_cond = "ae"; break;
                case IR_EQ: float_cond = "e"; break;
                default: float_cond = "ne"; break;
                }
                cargar_float_en_xmm(f, q->arg1, "xmm0");
                cargar_float_en_xmm(f, q->arg2, "xmm1");
                fprintf(f, "    ucomisd xmm0, xmm1\n    set%s al\n"
                           "    movzx rax, al\n    mov [rel %s], rax\n",
                        float_cond, q->result);
                break;
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

#if defined(_WIN32)
            if (is_string_literal(q->arg1))
            {
                fprintf(f,
                    "    lea rcx, [rel fmt_str]\n"
                    "    lea rdx, [rel str_%d]\n"
                    "    xor eax, eax\n"
                    "    call %s\n",
                    i, printf_sym);
            }
            else if (is_number(q->arg1))
            {
                fprintf(f,
                    "    lea rcx, [rel fmt_int]\n"
                    "    mov rdx, %s\n"
                    "    xor eax, eax\n"
                    "    call %s\n",
                    q->arg1, printf_sym);
            }
            else if (es_operando_flotante(q->arg1))
            {
                cargar_float_en_xmm(f, q->arg1, "xmm0");
                fprintf(f,
                    "    lea rcx, [rel fmt_float]\n"
                    "    mov eax, 1\n"
                    "    call %s\n", printf_sym);
            }
            else if (entry != NULL)
            {
                if (entry->tipo == STRING)
                {
                    fprintf(f,
                        "    lea rcx, [rel fmt_str]\n"
                        "    lea rdx, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->arg1, printf_sym);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                        "    lea rcx, [rel fmt_float]\n"
                        "    movsd xmm0, qword [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->arg1, printf_sym);
                }
                else
                {
                    fprintf(f,
                        "    lea rcx, [rel fmt_int]\n"
                        "    mov rdx, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->arg1, printf_sym);
                }
            }
#else
            if (is_string_literal(q->arg1))
            {
                fprintf(f,
                    "    lea rdi, [rel fmt_str]\n"
                    "    lea rsi, [rel str_%d]\n"
                    "    xor eax, eax\n"
                    "    call %s\n",
                    i, printf_sym);
            }
            else if (is_number(q->arg1))
            {
                fprintf(f,
                    "    mov rsi, %s\n"
                    "    lea rdi, [rel fmt_int]\n"
                    "    xor eax, eax\n"
                    "    call %s\n",
                    q->arg1, printf_sym);
            }
            else if (es_operando_flotante(q->arg1))
            {
                cargar_float_en_xmm(f, q->arg1, "xmm0");
                fprintf(f,
                    "    lea rdi, [rel fmt_float]\n"
                    "    mov eax, 1\n"
                    "    call %s\n",
                    printf_sym);
            }
            else if (entry != NULL)
            {
                if (entry->tipo == STRING)
                {
                    fprintf(f,
                        "    lea rdi, [rel fmt_str]\n"
                        "    lea rsi, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->arg1, printf_sym);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                        "    movsd xmm0, qword [rel %s]\n"
                        "    lea rdi, [rel fmt_float]\n"
                        "    mov eax, 1\n"
                        "    call %s\n",
                        q->arg1, printf_sym);
                }
                else
                {
                    fprintf(f,
                        "    mov rsi, [rel %s]\n"
                        "    lea rdi, [rel fmt_int]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->arg1, printf_sym);
                }
            }
            else if (q->arg1[0] == 's' && isdigit((unsigned char)q->arg1[1]))
            {
                fprintf(f,
                    "    mov rsi, [rel %s]\n"
                    "    lea rdi, [rel fmt_str]\n"
                    "    xor eax, eax\n"
                    "    call %s\n", q->arg1, printf_sym);
            }
            else
            {
                fprintf(f,
                        "    mov rsi, [rel %s]\n"
                        "    lea rdi, [rel fmt_int]\n"
                        "    xor eax, eax\n"
                        "    call %s\n", q->arg1, printf_sym);
            }
#endif
            break;
        }

        case IR_NATIVE_CALL:
        {
            const char *native_prefix =
#if defined(__APPLE__)
                "_";
#else
                "";
#endif
            char args[512];
            snprintf(args, sizeof(args), "%s", q->arg2 ? q->arg2 : "");
            char *first = strtok(args, "|");
            char *second = strtok(NULL, "|");
            char *third = strtok(NULL, "|");
            char *fourth = strtok(NULL, "|");
            int is_string = strcmp(q->arg1, "Longitud") == 0 ||
                            strcmp(q->arg1, "Comparar") == 0 ||
                            strcmp(q->arg1, "Contiene") == 0;
            if (strcmp(q->arg1, "Http") == 0) {
                char *http_args[] = {first, second, third, fourth};
                const char *registers[] = {"rdi", "rsi", "rdx", "rcx"};
                for (int n = 0; n < 4; n++) {
                    if (!http_args[n])
                        fprintf(f, "    xor %s, %s\n", registers[n], registers[n]);
                    else if (is_string_literal(http_args[n]))
                        fprintf(f, "    lea %s, [rel str_native_%d_%d]\n", registers[n], i, n);
                    else
                        fprintf(f, "    lea %s, [rel %s]\n", registers[n], http_args[n]);
                }
                fprintf(f, "    call %smx_http_request\n    mov [rel %s], rax\n",
                        native_prefix, q->result);
            } else if (strcmp(q->arg1, "HttpCuerpo") == 0) {
                fprintf(f, "    call %smx_http_body\n    mov [rel %s], rax\n",
                        native_prefix, q->result);
            } else if (strcmp(q->arg1, "HttpCabeceras") == 0) {
                fprintf(f, "    call %smx_http_headers\n    mov [rel %s], rax\n",
                        native_prefix, q->result);
            } else if (strcmp(q->arg1, "Aleatorio") == 0 || strcmp(q->arg1, "AleatorioEntre") == 0) {
                fprintf(f, "    call %srand\n", native_prefix);
                fprintf(f, "    movsxd rax, eax\n");
                if (strcmp(q->arg1, "Aleatorio") == 0 && first) {
                    if (is_number(first)) fprintf(f, "    mov rcx, %s\n", first);
                    else fprintf(f, "    mov rcx, [rel %s]\n", first);
                    fprintf(f, "    xor rdx, rdx\n    div rcx\n    mov rax, rdx\n");
                } else if (strcmp(q->arg1, "AleatorioEntre") == 0) {
                    if (is_number(first)) fprintf(f, "    mov rcx, %s\n", first);
                    else fprintf(f, "    mov rcx, [rel %s]\n", first);
                    if (is_number(second)) fprintf(f, "    mov r8, %s\n", second);
                    else fprintf(f, "    mov r8, [rel %s]\n", second);
                    fprintf(f, "    sub r8, rcx\n    inc r8\n    xor rdx, rdx\n    div r8\n    add rdx, rcx\n    mov rax, rdx\n");
                }
                fprintf(f, "    mov [rel %s], rax\n", q->result);
            } else if (is_string) {
                if (is_string_literal(first))
                    fprintf(f, "    lea rdi, [rel str_native_%d_0]\n", i);
                else
                    fprintf(f, "    lea rdi, [rel %s]\n", first);
                if (second) {
                    if (is_string_literal(second))
                        fprintf(f, "    lea rsi, [rel str_native_%d_1]\n", i);
                    else
                        fprintf(f, "    lea rsi, [rel %s]\n", second);
                }
                fprintf(f, "    call %s%s\n", native_prefix,
                        strcmp(q->arg1, "Longitud") == 0 ? "strlen" :
                        strcmp(q->arg1, "Comparar") == 0 ? "strcmp" : "strstr"
                );
                if (strcmp(q->arg1, "Contiene") == 0) {
                    fprintf(f, "    test rax, rax\n    setne al\n    movzx rax, al\n");
                }
                fprintf(f, "    mov [rel %s], rax\n", q->result);
            } else if (strcmp(q->arg1, "Abs") == 0 || strcmp(q->arg1, "Absoluto") == 0) {
                if (is_number(first)) fprintf(f, "    mov rax, %s\n", first);
                else fprintf(f, "    mov rax, [rel %s]\n", first);
                fprintf(f, "    cqo\n    xor rax, rdx\n    sub rax, rdx\n    mov [rel %s], rax\n", q->result);
            } else if (strcmp(q->arg1, "Min") == 0 || strcmp(q->arg1, "Max") == 0) {
                if (is_number(first)) fprintf(f, "    mov rax, %s\n", first);
                else fprintf(f, "    mov rax, [rel %s]\n", first);
                if (is_number(second)) fprintf(f, "    mov rcx, %s\n", second);
                else fprintf(f, "    mov rcx, [rel %s]\n", second);
                fprintf(f, "    cmp rax, rcx\n");
                fprintf(f, "    cmov%s rax, rcx\n    mov [rel %s], rax\n",
                        strcmp(q->arg1, "Min") == 0 ? "g" : "l", q->result);
            } else {
                const char *symbol = strcmp(q->arg1, "Potencia") == 0 ? "pow" :
                    strcmp(q->arg1, "RaizCuadrada") == 0 ? "sqrt" :
                    strcmp(q->arg1, "Seno") == 0 ? "sin" :
                    strcmp(q->arg1, "Coseno") == 0 ? "cos" :
                    strcmp(q->arg1, "Tangente") == 0 ? "tan" :
                    strcmp(q->arg1, "Logaritmo") == 0 ? "log" :
                    strcmp(q->arg1, "Exponencial") == 0 ? "exp" :
                    strcmp(q->arg1, "Piso") == 0 ? "floor" :
                    strcmp(q->arg1, "Techo") == 0 ? "ceil" : "round";
                cargar_float_en_xmm(f, first, "xmm0");
                if (second) cargar_float_en_xmm(f, second, "xmm1");
                fprintf(f, "    call %s%s\n", native_prefix, symbol);
                if (strcmp(q->arg1, "Redondear") == 0) {
                    fprintf(f, "    cvttsd2si rax, xmm0\n    mov [rel %s], rax\n", q->result);
                } else fprintf(f, "    movsd [rel %s], xmm0\n", q->result);
            }
            break;
        }

        case IR_READ:
        {
            EntradaSimbolo *entry = buscar_simbolo(ambito_actual, q->result);

#if defined(_WIN32)
            if (entry != NULL)
            {
                if (entry->tipo == STRING)
                {
                    fprintf(f,
                        "    lea rcx, [rel fmt_read_str]\n"
                        "    lea rdx, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->result, scanf_sym);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                        "    lea rcx, [rel fmt_read_float]\n"
                        "    lea rdx, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->result, scanf_sym);
                }
                else
                {
                    fprintf(f,
                        "    lea rcx, [rel fmt_read_int]\n"
                        "    lea rdx, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->result, scanf_sym);
                }
            }
#else
            if (entry != NULL)
            {
                if (entry->tipo == STRING)
                {
                    fprintf(f,
                        "    lea rdi, [rel fmt_read_str]\n"
                        "    lea rsi, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->result, scanf_sym);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                        "    lea rdi, [rel fmt_read_float]\n"
                        "    lea rsi, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->result, scanf_sym);
                }
                else
                {
                    fprintf(f,
                        "    lea rdi, [rel fmt_read_int]\n"
                        "    lea rsi, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        q->result, scanf_sym);
                }
            }
#endif
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

#if defined(_WIN32)
    fprintf(f, "    extern ExitProcess\n");
    fprintf(f, "    mov ecx, 0\n");
    fprintf(f, "    call ExitProcess\n");
#else
    fprintf(f, "    ret\n");
#endif

#if !defined(__APPLE__)
    fprintf(f, "section .note.GNU-stack noalloc noexec nowrite progbits\n");
#endif
}
