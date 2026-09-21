#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

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
static int current_return_value_owned = 0;
static char *current_return_storage = NULL;
static char *current_function_end_label = NULL;
static enum TipoDato current_return_type = TIPO_ERROR;
static ASTNode *current_inline_function = NULL;
static unsigned long long current_inline_instance = 0;
static unsigned long long next_inline_instance = 0;
static ASTNode *current_recursive_function_codegen = NULL;

#define MAX_RECURSIVE_LAYOUTS 128
#define MAX_RECURSIVE_SLOTS 512

typedef struct
{
    char name[256];
    int offset;
    int size;
} RecursiveSlot;

typedef struct
{
    char function_name[128];
    RecursiveSlot slots[MAX_RECURSIVE_SLOTS];
    int slot_count;
    int frame_size;
} RecursiveLayout;

static RecursiveLayout recursive_layouts[MAX_RECURSIVE_LAYOUTS];
static int recursive_layout_count = 0;
static RecursiveLayout *current_asm_recursive_layout = NULL;

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

static int funcion_contiene_llamada(const ASTNode *node, const char *nombre)
{
    if (!node || !nombre) return 0;
    if (node->type == AST_LLAMADA && node->hijo_izq && node->hijo_izq->valor.nombre_id &&
        strcmp(node->hijo_izq->valor.nombre_id, nombre) == 0)
        return 1;
    if (funcion_contiene_llamada(node->hijo_izq, nombre)) return 1;
    if (funcion_contiene_llamada(node->hijo_der, nombre)) return 1;
    if (funcion_contiene_llamada(node->siguiente_hermano, nombre)) return 1;
    return 0;
}

static int es_funcion_recursiva_ast(const ASTNode *fn)
{
    return fn && fn->type == AST_FUNCION && fn->hijo_izq &&
           fn->hijo_izq->valor.nombre_id &&
           funcion_contiene_llamada(fn->hijo_der, fn->hijo_izq->valor.nombre_id);
}

static char *nombre_funcion_asm_recursiva(const char *nombre)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "__mxf_%s", nombre ? nombre : "anon");
    return strdup(buffer);
}

static void generate_code_for_node(ASTNode *node);
static char *generate_code_for_expression(ASTNode *expr_node);
static void generate_code_for_statement(ASTNode *stmt_node);
static void generate_code_for_declaration(ASTNode *decl_node);
static void generate_code_for_if_statement(ASTNode *if_node);
static void generate_code_for_while_statement(ASTNode *while_node);
static void generate_code_for_for_statement(ASTNode *for_node);
static char *new_float_temp(void);
static char *new_string_temp(void);
static int es_temporal_prefijado(const char *s, char prefix);
static int es_operando_nulo(const char *s);
static int es_registro_x86(const char *s);
static int es_palabra_reservada_asm(const char *s);
static const char *asm_symbol(const char *s);
static int es_operando_flotante(const char *s);
static void cargar_float_en_xmm(FILE *f, const char *operando, const char *registro);
int es_literal(const char *s);
int is_number(const char *s);
int is_string_literal(const char *s);
static int es_funcion_nativa(const char *nombre);
static int funcion_contiene_llamada(const ASTNode *node, const char *nombre);
static int es_funcion_recursiva_ast(const ASTNode *fn);
static char *nombre_funcion_asm_recursiva(const char *nombre);
static RecursiveLayout *obtener_layout_recursivo(const char *nombre);
static const char *mem_ref(const char *operando);
static void emitir_carga_entero(FILE *f, const char *reg, const char *op);
static void emitir_guardado_entero(FILE *f, const char *op, const char *reg);
static void emitir_guardado_float(FILE *f, const char *op, const char *reg);
static char *resolver_entorno(const char *operando);
int is_valid_varname(const char *s);
static enum TipoDato tipo_nodo(const ASTNode *node)
{
    if (!node)
        return TIPO_ERROR;
    return node->resolved_type;
}

static const char *tipo_prefijo_mx(enum TipoDato tipo)
{
    switch (tipo)
    {
    case INT: return "i";
    case FLOAT: return "f";
    case STRING: return "s";
    case BOOL: return "b";
    default: return "v";
    }
}

static int localizar_local_funcion(const ASTNode *node, const char *nombre, enum TipoDato *tipo)
{
    if (!node || !nombre)
        return 0;

    if (node->type == AST_DECLARACION_VAR || node->type == AST_DECLARACION_CONST)
    {
        if (node->hijo_izq && node->hijo_izq->valor.nombre_id &&
            strcmp(node->hijo_izq->valor.nombre_id, nombre) == 0)
        {
            if (tipo)
                *tipo = tipo_nodo(node->hijo_izq);
            return 1;
        }
    }

    if (localizar_local_funcion(node->hijo_izq, nombre, tipo))
        return 1;
    if (localizar_local_funcion(node->hijo_der, nombre, tipo))
        return 1;
    if (localizar_local_funcion(node->siguiente_hermano, nombre, tipo))
        return 1;

    return 0;
}

static int es_parametro_funcion_actual(const char *nombre, enum TipoDato *tipo)
{
    if (!current_inline_function || !nombre)
        return 0;

    for (ASTNode *p = current_inline_function->parametros; p; p = p->siguiente_hermano)
    {
        if (p->hijo_izq && p->hijo_izq->valor.nombre_id &&
            strcmp(p->hijo_izq->valor.nombre_id, nombre) == 0)
        {
            if (tipo)
                *tipo = tipo_nodo(p->hijo_izq);
            return 1;
        }
    }

    return 0;
}

static int es_local_funcion_actual(const char *nombre, enum TipoDato *tipo)
{
    if (es_parametro_funcion_actual(nombre, tipo))
        return 1;

    if (!current_inline_function || !current_inline_function->hijo_der)
        return 0;

    return localizar_local_funcion(current_inline_function->hijo_der, nombre, tipo);
}

static char *nombre_local_funcion_actual(const char *nombre, enum TipoDato tipo)
{
    if (!nombre)
        return NULL;

    char buffer[256];
    if (current_recursive_function_codegen && current_recursive_function_codegen->hijo_izq &&
        current_recursive_function_codegen->hijo_izq->valor.nombre_id)
    {
        snprintf(buffer, sizeof(buffer), "__mxrec_%s_%s_%s",
                 current_recursive_function_codegen->hijo_izq->valor.nombre_id,
                 tipo_prefijo_mx(tipo), nombre);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "__mxfn%llu_%s_%s",
                 current_inline_instance, tipo_prefijo_mx(tipo), nombre);
    }
    return strdup(buffer);
}

static char *resolver_nombre_array_codegen(const char *base)
{
    if (!base) return NULL;
    enum TipoDato tipo = TIPO_ERROR;
    if (es_local_funcion_actual(base, &tipo))
        return nombre_local_funcion_actual(base, tipo);
    return strdup(base);
}

static char *crear_temp_para_tipo(enum TipoDato tipo)
{
    if (tipo == FLOAT)
        return new_float_temp();
    if (tipo == STRING)
        return new_string_temp();
    return new_temp();
}

static char *generar_acceso_array(ASTNode *expr_node)
{
    if (!expr_node || !expr_node->hijo_izq || !expr_node->hijo_der) return strdup("0");
    const char *base = expr_node->hijo_izq->valor.nombre_id;
    if (!base) return strdup("0");
    char *indice = generate_code_for_expression(expr_node->hijo_der);
    if (!indice) return strdup("0");
    char *array_name = resolver_nombre_array_codegen(base);
    if (!array_name) { free(indice); return strdup("0"); }
    char *resultado = crear_temp_para_tipo(expr_node->resolved_type);
    emit_quad(IR_LOAD_ARRAY, array_name, indice, resultado);
    free(array_name); free(indice);
    return resultado;
}

static void generar_guardado_array(ASTNode *lhs, const char *valor)
{
    if (!lhs || lhs->type != AST_ACCESO_ARRAY || !lhs->hijo_izq || !lhs->hijo_der || !valor) return;
    const char *base = lhs->hijo_izq->valor.nombre_id;
    if (!base) return;
    char *indice = generate_code_for_expression(lhs->hijo_der);
    if (!indice) return;
    char *array_name = resolver_nombre_array_codegen(base);
    if (!array_name) { free(indice); return; }
    emit_quad(IR_STORE_ARRAY, array_name, indice, valor);
    free(array_name); free(indice);
}

static char *generate_code_for_expression(ASTNode *expr_node)
{
    if (!expr_node)
    {
        return NULL;
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
        if (strcmp(native, "Entorno") == 0)
        {
            result_name = resolver_entorno(expr_node->hijo_der
                                               ? generate_code_for_expression(expr_node->hijo_der)
                                               : "\"\"");
            break;
        }
        if (strcmp(native, "ExisteEntorno") == 0)
        {
            char *clave = NULL;
            int clave_owned = 0;

            if (expr_node->hijo_der)
                clave = generate_code_for_expression(expr_node->hijo_der);
            else
            {
                clave = strdup("\"\"");
                clave_owned = 1;
            }

            char *valor = resolver_entorno(clave);
            result_name = new_temp();
            emit_quad(IR_ASSIGN, valor && strcmp(valor, "\"\"") != 0 ? "1" : "0", NULL, result_name);

            if (clave_owned)
                free(clave);
            free(valor);
            break;
        }
        if (es_funcion_nativa(native))
        {
            char *values[4] = {0};
            size_t len = 1;
            int count = 0;
            for (ASTNode *a = expr_node->hijo_der; a && count < 4; a = a->siguiente_hermano)
            {
                values[count] = generate_code_for_expression(a);
                len += strlen(values[count]) + 1;
                count++;
            }
            char *args = calloc(len, 1);
            for (int n = 0; n < count; n++)
            {
                if (n > 0)
                    strcat(args, "|");
                strcat(args, values[n]);
            }
            result_name = (expr_node->resolved_type == FLOAT) ? new_float_temp() : (expr_node->resolved_type == STRING ? new_string_temp() : new_temp());
            emit_quad(IR_NATIVE_CALL, native, args, result_name);
            free(args);
            break;
        }
        ASTNode *fn = NULL, *c = program_root ? program_root->hijo_izq : NULL;
        while (c)
        {
            if (c->type == AST_FUNCION && c->hijo_izq &&
                strcmp(c->hijo_izq->valor.nombre_id, expr_node->hijo_izq->valor.nombre_id) == 0)
            {
                fn = c;
                break;
            }
            c = c->siguiente_hermano;
        }
        if (!fn)
        {
            result_name = strdup("0");
            break;
        }

        if (es_funcion_recursiva_ast(fn))
        {
            char args_buffer[4096];
            args_buffer[0] = '\0';
            int first_arg = 1;
            for (ASTNode *ra = expr_node->hijo_der; ra; ra = ra->siguiente_hermano)
            {
                char *value = generate_code_for_expression(ra);
                if (!value) value = strdup("0");
                if (!first_arg)
                    strncat(args_buffer, "|", sizeof(args_buffer) - strlen(args_buffer) - 1);
                strncat(args_buffer, value, sizeof(args_buffer) - strlen(args_buffer) - 1);
                free(value);
                first_arg = 0;
            }

            if (fn->return_type == FLOAT)
                result_name = new_float_temp();
            else if (fn->return_type == STRING)
                result_name = new_string_temp();
            else
                result_name = new_temp();

            char *asm_name = nombre_funcion_asm_recursiva(fn->hijo_izq->valor.nombre_id);
            emit_quad(IR_CALL, asm_name, args_buffer, result_name);
            free(asm_name);
            break;
        }

        ASTNode *p = fn->parametros, *a = expr_node->hijo_der;

        /*
         * Los argumentos deben generarse en el contexto del caller.
         * Si cambiamos current_inline_function antes de evaluarlos,
         * argumentos como duplicar(x) dentro de combinar() dejan de
         * resolver x en combinar y terminan como un identificador global.
         */
        typedef struct
        {
            char *valor;
            enum TipoDato tipo;
        } ArgumentoGenerado;

        ArgumentoGenerado argumentos[64];
        size_t cantidad_argumentos = 0;

        while (a && cantidad_argumentos < 64)
        {
            argumentos[cantidad_argumentos].valor =
                generate_code_for_expression(a);
            argumentos[cantidad_argumentos].tipo =
                p ? tipo_nodo(p->hijo_izq) : TIPO_ERROR;
            cantidad_argumentos++;
            a = a->siguiente_hermano;
            if (p)
                p = p->siguiente_hermano;
        }

        unsigned long long previous_inline_instance = current_inline_instance;
        ASTNode *previous_inline_function = current_inline_function;
        current_inline_instance = ++next_inline_instance;
        current_inline_function = fn;

        p = fn->parametros;
        for (size_t i = 0; i < cantidad_argumentos && p; ++i, p = p->siguiente_hermano)
        {
            char *param_name =
                nombre_local_funcion_actual(p->hijo_izq->valor.nombre_id,
                                            argumentos[i].tipo);
            emit_quad(IR_ASSIGN, argumentos[i].valor, NULL, param_name);
            free(param_name);
            free(argumentos[i].valor);
        }

        char *previous_return_value = current_return_value;
        int previous_return_value_owned = current_return_value_owned;
        char *previous_return_storage = current_return_storage;
        char *previous_function_end_label = current_function_end_label;

        current_return_value = NULL;
        current_return_value_owned = 0;

        enum TipoDato previous_return_type = current_return_type;
        current_return_type = fn->return_type;
        current_return_storage = nombre_local_funcion_actual("return", fn->return_type);
        current_function_end_label = new_label();

        generate_code_for_node(fn->hijo_der);
        emit_quad(IR_LABEL, NULL, NULL, current_function_end_label);

        char *function_return_value = current_return_value;
        int function_return_value_owned = current_return_value_owned;

        char *function_return_storage = current_return_storage;

        current_return_value = previous_return_value;
        current_return_value_owned = previous_return_value_owned;
        current_return_storage = previous_return_storage;
        current_function_end_label = previous_function_end_label;
        current_return_type = previous_return_type;
        current_inline_function = previous_inline_function;
        current_inline_instance = previous_inline_instance;

        if (function_return_value)
        {
            if (fn->return_type == STRING)
                result_name = strdup(function_return_value);
            else
            {
                result_name = (fn->return_type == FLOAT) ? new_float_temp() : new_temp();
                emit_quad(IR_ASSIGN, function_return_storage, NULL, result_name);
            }
        }
        else
            result_name = strdup("0");

        if (function_return_value_owned)
            free(function_return_value);
        free(function_return_storage);
        break;
    }
    case AST_ACCESO_ARRAY:
        result_name = generar_acceso_array(expr_node);
        break;

    case AST_TERNARIO_EXPR:
    {
        if (!expr_node->hijo_izq || !expr_node->hijo_der ||
            !expr_node->hijo_der->siguiente_hermano)
        {
            result_name = strdup("0");
            break;
        }

        char *condicion = generate_code_for_expression(expr_node->hijo_izq);
        char *resultado = crear_temp_para_tipo(expr_node->resolved_type);
        char *falso = new_label();
        char *fin = new_label();

        emit_quad(IR_IF_FALSE_GOTO, condicion, NULL, falso);
        char *valor_verdadero = generate_code_for_expression(expr_node->hijo_der);
        emit_quad(IR_ASSIGN, valor_verdadero, NULL, resultado);
        emit_quad(IR_GOTO, NULL, NULL, fin);
        emit_quad(IR_LABEL, NULL, NULL, falso);
        char *valor_falso = generate_code_for_expression(expr_node->hijo_der->siguiente_hermano);
        emit_quad(IR_ASSIGN, valor_falso, NULL, resultado);
        emit_quad(IR_LABEL, NULL, NULL, fin);

        free(condicion); free(valor_verdadero); free(valor_falso); free(falso); free(fin);
        result_name = resultado;
        break;
    }

    case AST_IDENTIFICADOR:
    {
        enum TipoDato local_type = TIPO_ERROR;
        if (es_local_funcion_actual(expr_node->valor.nombre_id, &local_type))
        {
            result_name = nombre_local_funcion_actual(expr_node->valor.nombre_id, local_type);
            break;
        }

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
        free(operand_name);
        result_name = temp;
        break;
    }
    case AST_NOT_EXPR:
    {
        char *operand_name = generate_code_for_expression(expr_node->hijo_izq);
        char *temp = new_temp();
        emit_quad(IR_NOT, operand_name, NULL, temp);
        free(operand_name);
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
        char *temp;
        int es_comparacion =
            (expr_node->type == AST_IGUAL_EXPR ||
             expr_node->type == AST_DIFERENTE_EXPR ||
             expr_node->type == AST_MENOR_QUE_EXPR ||
             expr_node->type == AST_MAYOR_QUE_EXPR ||
             expr_node->type == AST_MENOR_IGUAL_EXPR ||
             expr_node->type == AST_MAYOR_IGUAL_EXPR);

        int es_logico =
            (expr_node->type == AST_OR_EXPR ||
             expr_node->type == AST_AND_EXPR);

        if (es_comparacion || es_logico)
            temp = new_temp();
        else if (expr_node->resolved_type == FLOAT ||
                 es_operando_flotante(left_operand) ||
                 es_operando_flotante(right_operand))
            temp = new_float_temp();
        else
            temp = new_temp();

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
        free(left_operand);
        free(right_operand);
        result_name = temp;
        break;
    }

    default:
        fprintf(stderr, "Error at %d:%d: Error interno: Tipo de expresión no manejado para generación de CI.\n", expr_node->renglon, expr_node->columna);
        result_name = strdup("ERROR_EXPR");
        break;
    }

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
        if (!stmt_node->hijo_izq)
            break;

        char *expr_result = generate_code_for_expression(stmt_node->hijo_der);

        if (stmt_node->hijo_izq->type == AST_ACCESO_ARRAY)
        {
            generar_guardado_array(stmt_node->hijo_izq, expr_result);
        }
        else
        {
            char *var_name = stmt_node->hijo_izq->valor.nombre_id;
            enum TipoDato local_type = TIPO_ERROR;
            char *target_name = NULL;

            if (es_local_funcion_actual(var_name, &local_type))
                target_name = nombre_local_funcion_actual(var_name, local_type);
            else
                target_name = strdup(var_name);

            emit_quad(IR_ASSIGN, expr_result, NULL, target_name);
            free(target_name);
        }

        free(expr_result);
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
        enum TipoDato local_type = TIPO_ERROR;
        char *target_name = NULL;
        if (es_local_funcion_actual(read_target, &local_type))
            target_name = nombre_local_funcion_actual(read_target, local_type);
        else
            target_name = strdup(read_target);

        emit_quad(IR_READ, NULL, NULL, target_name);
        free(target_name);
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
        "Piso", "Techo", "Redondear", "Longitud", "Comparar", "Contiene", "Aleatorio", "AleatorioEntre", "Entorno", "ExisteEntorno",
        "Http", "HttpCuerpo", "HttpCabeceras", "Decimales"};
    for (size_t i = 0; i < sizeof(nombres) / sizeof(nombres[0]); i++)
        if (strcmp(nombre, nombres[i]) == 0)
            return 1;
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
    if (!value)
        return strdup("\"\"");

    char *result = malloc(strlen(value) * 2 + 3);
    size_t j = 0;
    result[j++] = '"';
    for (size_t i = 0; value[i] != '\0'; i++)
    {
        if (value[i] == '"' || value[i] == '\\')
            result[j++] = '\\';
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
        enum TipoDato local_type = tipo_nodo(decl_node->hijo_izq);
        char *target_name = NULL;
        if (current_inline_function)
            target_name = nombre_local_funcion_actual(var_name, local_type);
        else
            target_name = strdup(var_name);
        emit_quad(IR_ASSIGN, expr_result, NULL, target_name);
        free(target_name);
    }
}

static void generate_code_for_if_statement(ASTNode *if_node)
{
    if (!if_node)
        return;

    char *end_if_label = new_label();
    char *condition_result = generate_code_for_expression(if_node->hijo_izq);
    char *false_label = new_label();

    /* Si / Sino Si / Sino se representa mediante nodos Sino hermanos. */
    ASTNode *sino_actual = if_node->siguiente_hermano;

    emit_quad(IR_IF_FALSE_GOTO, condition_result, NULL, false_label);

    /* Rama verdadera del Si original. */
    if (if_node->hijo_der)
        generate_code_for_node(if_node->hijo_der);

    emit_quad(IR_GOTO, NULL, NULL, end_if_label);
    emit_quad(IR_LABEL, NULL, NULL, false_label);

    /* Genera toda la cadena de Sino Si / Sino. */
    while (sino_actual && sino_actual->type == AST_SINO_STMT)
    {
        ASTNode *contenido = sino_actual->hijo_izq;

        if (contenido && contenido->type == AST_SI_STMT)
        {
            char *else_if_condition =
                generate_code_for_expression(contenido->hijo_izq);
            char *next_false_label = new_label();

            emit_quad(IR_IF_FALSE_GOTO,
                      else_if_condition,
                      NULL,
                      next_false_label);

            if (contenido->hijo_der)
                generate_code_for_node(contenido->hijo_der);

            emit_quad(IR_GOTO, NULL, NULL, end_if_label);
            emit_quad(IR_LABEL, NULL, NULL, next_false_label);

            free(else_if_condition);
            free(next_false_label);

            sino_actual = sino_actual->siguiente_hermano;
            continue;
        }

        /* Sino final. */
        if (contenido)
            generate_code_for_node(contenido);

        break;
    }

    emit_quad(IR_LABEL, NULL, NULL, end_if_label);

    free(condition_result);
    free(false_label);
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

    free(condition_result);
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
        case IR_LOAD_ARRAY:
            printf("LOAD_ARRAY");
            break;
        case IR_STORE_ARRAY:
            printf("STORE_ARRAY");
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
    if (strncmp(s, "__float_", 8) == 0 ||
        es_temporal_prefijado(s, 'f') ||
        (strncmp(s, "__mxfn", 6) == 0 && strstr(s, "_f_") != NULL) ||
        (strcmp(s, "__return") == 0 && current_return_type == FLOAT))
        return 1;
    EntradaSimbolo *entry = buscar_simbolo_ambitos(ambito_actual, s);
    return entry != NULL && entry->tipo == FLOAT;
}

#define MAX_FLOAT_SYMBOLS 2048
static char float_symbols[MAX_FLOAT_SYMBOLS][128];
static int float_symbol_count = 0;

static int float_symbol_known(const char *s)
{
    if (!s) return 0;
    for (int i = 0; i < float_symbol_count; ++i)
        if (strcmp(float_symbols[i], s) == 0) return 1;
    return 0;
}

static void mark_float_symbol(const char *s)
{
    if (!s || !*s || float_symbol_known(s)) return;
    if (float_symbol_count >= MAX_FLOAT_SYMBOLS) return;
    snprintf(float_symbols[float_symbol_count], sizeof(float_symbols[0]), "%s", s);
    ++float_symbol_count;
}

static void preparar_simbolos_flotantes(void)
{
    float_symbol_count = 0;

    for (int pass = 0; pass < 3; ++pass)
    {
        for (int i = 0; i < ir_current_size; ++i)
        {
            Quadruple *q = &ir_code[i];

            if (q->result && strncmp(q->result, "f", 1) == 0 &&
                isdigit((unsigned char)q->result[1]))
                mark_float_symbol(q->result);

            if (q->op == IR_ASSIGN && q->result && q->arg1)
            {
                if (strncmp(q->arg1, "__float_", 8) == 0 ||
                    float_symbol_known(q->arg1))
                    mark_float_symbol(q->result);
            }

            if ((q->op == IR_ADD || q->op == IR_SUB ||
                 q->op == IR_MUL || q->op == IR_DIV ||
                 q->op == IR_NEG) && q->result)
            {
                if (float_symbol_known(q->arg1) ||
                    float_symbol_known(q->arg2) ||
                    (q->arg1 && strncmp(q->arg1, "__float_", 8) == 0) ||
                    (q->arg2 && strncmp(q->arg2, "__float_", 8) == 0))
                    mark_float_symbol(q->result);
            }

            if (q->op == IR_NATIVE_CALL && q->result &&
                q->arg1 &&
                (strcmp(q->arg1, "Potencia") == 0 ||
                 strcmp(q->arg1, "RaizCuadrada") == 0 ||
                 strcmp(q->arg1, "Seno") == 0 ||
                 strcmp(q->arg1, "Coseno") == 0 ||
                 strcmp(q->arg1, "Tangente") == 0 ||
                 strcmp(q->arg1, "Logaritmo") == 0 ||
                 strcmp(q->arg1, "Exponencial") == 0 ||
                 strcmp(q->arg1, "Piso") == 0 ||
                 strcmp(q->arg1, "Techo") == 0))
                mark_float_symbol(q->result);
        }
    }
}

static int es_operando_flotante_codegen(const char *s)
{
    return es_operando_flotante(s) || float_symbol_known(s);
}

static void cargar_float_en_xmm(FILE *f, const char *operando, const char *registro)
{
    if (!operando || es_operando_nulo(operando))
    {
        fprintf(f,
                "    xor eax, eax\n"
                "    cvtsi2sd %s, eax\n",
                registro);
        return;
    }

    if (strncmp(operando, "__float_", 8) == 0)
    {
        unsigned long long bits = 0;
        sscanf(operando + 8, "%llx", &bits);

        fprintf(f,
                "    mov rax, 0x%llx\n"
                "    movq %s, rax\n",
                bits, registro);
    }
    else if (isdigit(operando[0]) ||
             (operando[0] == '-' && isdigit(operando[1])))
    {
        fprintf(f,
                "    mov eax, %s\n"
                "    cvtsi2sd %s, eax\n",
                operando, registro);
    }
    else
    {
        fprintf(f,
                "    movsd %s, qword %s\n",
                registro, mem_ref(operando));
    }
}

static int lista_contiene_operando(const char *lista, const char *operando)
{
    if (!lista || !operando)
        return 0;

    char copia[4096];
    snprintf(copia, sizeof(copia), "%s", lista);

    char *token = strtok(copia, "|");
    while (token)
    {
        if (strcmp(token, operando) == 0)
            return 1;
        token = strtok(NULL, "|");
    }

    return 0;
}

static char *sustituir_operando_en_lista(const char *lista, const char *buscado, const char *reemplazo)
{
    if (!lista || !buscado || !reemplazo)
        return lista ? strdup(lista) : NULL;

    char copia[4096];
    snprintf(copia, sizeof(copia), "%s", lista);

    char *resultado = calloc(sizeof(copia), 1);
    if (!resultado)
        return NULL;

    char *token = strtok(copia, "|");
    int primero = 1;

    while (token)
    {
        const char *valor = strcmp(token, buscado) == 0 ? reemplazo : token;

        if (!primero)
            strcat(resultado, "|");
        strcat(resultado, valor);
        primero = 0;

        token = strtok(NULL, "|");
    }

    return resultado;
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
    return es_temporal_prefijado(s, 't');
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

        /* Los argumentos de una llamada están serializados como a|b|c. */
        if ((q->op == IR_CALL || q->op == IR_NATIVE_CALL) &&
            q->arg2 && lista_contiene_operando(q->arg2, dst))
        {
            char *nuevo = sustituir_operando_en_lista(q->arg2, dst, src);
            if (nuevo)
            {
                free(q->arg2);
                q->arg2 = nuevo;
            }
        }
    }
}

static int ir_op_is_terminator(IROperation op)
{
    return op == IR_GOTO || op == IR_HALT || op == IR_FUNCTION_END;
}

static int ir_result_used_after(const char *name, int from)
{
    if (!name) return 0;
    for (int j = from; j < ir_current_size; ++j)
    {
        Quadruple *q = &ir_code[j];
        if ((q->arg1 && strcmp(q->arg1, name) == 0) ||
            (q->arg2 && strcmp(q->arg2, name) == 0) ||
            ((q->op == IR_STORE_ARRAY) && q->result && strcmp(q->result, name) == 0) ||
            (q->op == IR_CALL && lista_contiene_operando(q->arg2, name)) ||
            (q->op == IR_NATIVE_CALL && lista_contiene_operando(q->arg2, name)))
            return 1;
        if (q->result && strcmp(q->result, name) == 0 && q->op != IR_STORE_ARRAY)
            return 0;
    }
    return 0;
}

static int ir_label_target_equal(const char *target, const char *label)
{
    return target && label && strcmp(target, label) == 0;
}

static int ir_parse_integer(const char *s, long long *out)
{
    if (!s || !out) return 0;
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

static void ir_replace_with_assign(Quadruple *q, const char *value)
{
    if (!q || !value) return;

    /*
     * value puede apuntar al propio q->arg1 (por ejemplo, x + 0).
     * No podemos liberar q->arg1 antes de copiar value porque eso
     * convierte value en un puntero colgante y corrompe el heap.
     */
    char *nuevo_arg1 = strdup(value);
    if (!nuevo_arg1)
    {
        fprintf(stderr, "Error: No se pudo asignar memoria al simplificar una cuádrupla.\n");
        exit(EXIT_FAILURE);
    }

    free(q->arg1);
    free(q->arg2);
    q->arg1 = nuevo_arg1;
    q->arg2 = NULL;
    q->op = IR_ASSIGN;
}

static int ir_try_constant_fold(Quadruple *q)
{
    if (!q || !q->arg1 || !q->result) return 0;

    long long a, b;
    double da, db;
    char buffer[64];

    if (q->arg2 && ir_parse_integer(q->arg1, &a) && ir_parse_integer(q->arg2, &b))
    {
        long long r = 0;
        int valid = 1;
        switch (q->op)
        {
        case IR_ADD: r = a + b; break;
        case IR_SUB: r = a - b; break;
        case IR_MUL: r = a * b; break;
        case IR_DIV: if (b == 0) valid = 0; else r = a / b; break;
        case IR_MOD: if (b == 0) valid = 0; else r = a % b; break;
        case IR_LT: r = a < b; break;
        case IR_GT: r = a > b; break;
        case IR_LE: r = a <= b; break;
        case IR_GE: r = a >= b; break;
        case IR_EQ: r = a == b; break;
        case IR_NE: r = a != b; break;
        case IR_AND: r = (a != 0) && (b != 0); break;
        case IR_OR:  r = (a != 0) || (b != 0); break;
        default: valid = 0; break;
        }
        if (!valid) return 0;
        snprintf(buffer, sizeof(buffer), "%lld", r);
        ir_replace_with_assign(q, buffer);
        return 1;
    }

    if (q->arg2 && es_literal(q->arg1) && es_literal(q->arg2) &&
        ((strchr(q->arg1, '.') != NULL) || (strchr(q->arg2, '.') != NULL)))
    {
        da = atof(q->arg1);
        db = atof(q->arg2);
        double r = 0.0;
        int valid = 1;
        switch (q->op)
        {
        case IR_ADD: r = da + db; break;
        case IR_SUB: r = da - db; break;
        case IR_MUL: r = da * db; break;
        case IR_DIV: if (db == 0.0) valid = 0; else r = da / db; break;
        default: valid = 0; break;
        }
        if (!valid) return 0;
        snprintf(buffer, sizeof(buffer), "%.17g", r);
        ir_replace_with_assign(q, buffer);
        return 1;
    }

    return 0;
}

static int ir_try_algebraic_simplification(Quadruple *q)
{
    if (!q || !q->arg1 || !q->arg2 || !q->result) return 0;

    long long v;
    if (!ir_parse_integer(q->arg2, &v)) return 0;

    switch (q->op)
    {
    case IR_ADD:
        if (v == 0) { ir_replace_with_assign(q, q->arg1); return 1; }
        break;
    case IR_SUB:
        if (v == 0) { ir_replace_with_assign(q, q->arg1); return 1; }
        break;
    case IR_MUL:
        if (v == 0) { ir_replace_with_assign(q, "0"); return 1; }
        if (v == 1) { ir_replace_with_assign(q, q->arg1); return 1; }
        break;
    case IR_DIV:
        if (v == 1) { ir_replace_with_assign(q, q->arg1); return 1; }
        break;
    case IR_AND:
        if (v == 0) { ir_replace_with_assign(q, "0"); return 1; }
        break;
    case IR_OR:
        if (v == 0) { ir_replace_with_assign(q, q->arg1); return 1; }
        break;
    default:
        break;
    }

    return 0;
}

static void compactar_ir(void)
{
    int out = 0;
    for (int i = 0; i < ir_current_size; ++i)
    {
        if (ir_code[i].op == (IROperation)-1)
            continue;

        if (i != out)
        {
            ir_code[out] = ir_code[i];
            ir_code[i].arg1 = NULL;
            ir_code[i].arg2 = NULL;
            ir_code[i].result = NULL;
            ir_code[i].op = (IROperation)-1;
        }
        ++out;
    }
    ir_current_size = out;
}

static void eliminar_codigo_inalcanzable(void)
{
    int dead = 0;
    for (int i = 0; i < ir_current_size; ++i)
    {
        Quadruple *q = &ir_code[i];

        if (q->op == IR_LABEL || q->op == IR_FUNCTION_BEGIN)
        {
            dead = 0;
            continue;
        }

        if (dead)
        {
            free(q->arg1);
            free(q->arg2);
            free(q->result);
            q->arg1 = q->arg2 = q->result = NULL;
            q->op = (IROperation)-1;
            continue;
        }

        if (ir_op_is_terminator(q->op) || q->op == IR_GOTO)
            dead = 1;
    }

    compactar_ir();
}

static int ir_temporal_en_bloque_lineal(int indice, const char *nombre)
{
    if (!nombre) return 0;
    for (int j=indice+1; j<ir_current_size; ++j)
    {
        Quadruple *q=&ir_code[j];
        if (q->op==IR_LABEL || q->op==IR_GOTO || q->op==IR_IF_FALSE_GOTO ||
            q->op==IR_FUNCTION_BEGIN || q->op==IR_FUNCTION_END) return 0;
        if ((q->arg1 && strcmp(q->arg1,nombre)==0) || (q->arg2 && strcmp(q->arg2,nombre)==0) ||
            ((q->op==IR_STORE_ARRAY) && q->result && strcmp(q->result,nombre)==0) ||
            (q->op==IR_CALL && lista_contiene_operando(q->arg2,nombre)) ||
            (q->op==IR_NATIVE_CALL && lista_contiene_operando(q->arg2,nombre))) return 1;
        if (q->result && strcmp(q->result,nombre)==0 && q->op != IR_STORE_ARRAY) return 1;
    }
    return 0;
}

static void eliminar_gotos_redundantes(void)
{
    for (int i = 0; i + 1 < ir_current_size; ++i)
    {
        Quadruple *q = &ir_code[i];
        Quadruple *next = &ir_code[i + 1];
        if (q->op == IR_GOTO && next->op == IR_LABEL &&
            ir_label_target_equal(q->result, next->result))
        {
            free(q->arg1);
            free(q->arg2);
            free(q->result);
            q->arg1 = q->arg2 = q->result = NULL;
            q->op = (IROperation)-1;
        }
    }

    compactar_ir();
}

static void eliminar_labels_no_referenciados(void)
{
    for (int i = 0; i < ir_current_size; ++i)
    {
        Quadruple *q = &ir_code[i];
        if (q->op != IR_LABEL || !q->result) continue;

        int referenced = 0;
        for (int j = 0; j < ir_current_size && !referenced; ++j)
        {
            if (i == j) continue;
            Quadruple *r = &ir_code[j];
            if ((r->op == IR_GOTO || r->op == IR_IF_FALSE_GOTO) &&
                r->result && strcmp(r->result, q->result) == 0)
                referenced = 1;
        }

        if (!referenced)
        {
            free(q->arg1);
            free(q->arg2);
            free(q->result);
            q->arg1 = q->arg2 = q->result = NULL;
            q->op = (IROperation)-1;
        }
    }

    compactar_ir();
}

void optimize_ir_code(void)
{
    int changed = 1;
    int passes = 0;

    while (changed && passes++ < 32)
    {
        changed = 0;

        for (int i = 0; i < ir_current_size; ++i)
        {
            Quadruple *q = &ir_code[i];

            if (q->op == IR_ASSIGN && q->arg1 && q->result &&
                strcmp(q->arg1, q->result) == 0)
            {
                free(q->arg1);
                free(q->arg2);
                free(q->result);
                q->arg1 = q->arg2 = q->result = NULL;
                q->op = (IROperation)-1;
                changed = 1;
                continue;
            }

            if (ir_try_constant_fold(q))
            {
                changed = 1;
                continue;
            }

            if (ir_try_algebraic_simplification(q))
            {
                changed = 1;
                continue;
            }

        }

        int out = 0;
        for (int i = 0; i < ir_current_size; ++i)
        {
            if (ir_code[i].op == (IROperation)-1)
                continue;

            if (i != out)
            {
                ir_code[out] = ir_code[i];
                ir_code[i].arg1 = NULL;
                ir_code[i].arg2 = NULL;
                ir_code[i].result = NULL;
                ir_code[i].op = (IROperation)-1;
            }
            ++out;
        }
        ir_current_size = out;

        eliminar_codigo_inalcanzable();
        eliminar_gotos_redundantes();
    }

    /*
     * No eliminar temporales por uso lineal aquí.
     * Un temporal puede producirse en una rama y consumirse después
     * de un LABEL/GOTO (por ejemplo, una expresión ternaria).
     * Sin análisis de flujo de control, eliminarlo rompe la semántica.
     */

    eliminar_gotos_redundantes();
    eliminar_labels_no_referenciados();
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
static int agregar_slot_recursivo(RecursiveLayout *layout, const char *name, int size)
{
    if (!layout || !name || !*name) return 0;
    for (int i = 0; i < layout->slot_count; ++i)
        if (strcmp(layout->slots[i].name, name) == 0) return 1;
    if (layout->slot_count >= MAX_RECURSIVE_SLOTS) return 0;
    int align = size >= 16 ? 16 : 8;
    int off = (layout->frame_size + align - 1) & ~(align - 1);
    off += size;
    layout->frame_size = off;
    snprintf(layout->slots[layout->slot_count].name, sizeof(layout->slots[0].name), "%s", name);
    layout->slots[layout->slot_count].offset = off;
    layout->slots[layout->slot_count].size = size;
    layout->slot_count++;
    return 1;
}

static void preparar_layouts_recursivos(void)
{
    recursive_layout_count = 0;
    for (int i = 0; i < ir_current_size; ++i)
    {
        if (ir_code[i].op != IR_FUNCTION_BEGIN) continue;
        if (recursive_layout_count >= MAX_RECURSIVE_LAYOUTS) break;
        RecursiveLayout *layout = &recursive_layouts[recursive_layout_count++];
        memset(layout, 0, sizeof(*layout));
        snprintf(layout->function_name, sizeof(layout->function_name), "%s", ir_code[i].arg1 ? ir_code[i].arg1 : "");
        int j = i + 1;
        for (; j < ir_current_size && ir_code[j].op != IR_FUNCTION_END; ++j)
        {
            Quadruple *q = &ir_code[j];
            const char *ops[] = {q->arg1, q->arg2, q->result};
            for (int k = 0; k < 3; ++k)
            {
                const char *op = ops[k];
                if (!op) continue;

                /*
                 * IR_CALL e IR_NATIVE_CALL serializan sus argumentos como:
                 *     arg0|arg1|arg2
                 * Cada operando debe formar parte del layout de la función.
                 */
                if ((q->op == IR_CALL || q->op == IR_NATIVE_CALL) && k == 1)
                {
                    char copia[4096];
                    snprintf(copia, sizeof(copia), "%s", op);
                    char *tok = strtok(copia, "|");
                    while (tok)
                    {
                        if (strncmp(tok, "__mxrec_", 8) == 0)
                            agregar_slot_recursivo(layout, tok, strstr(tok, "_s_") ? 256 : 8);
                        else if (es_temporal_prefijado(tok, 't') ||
                                 es_temporal_prefijado(tok, 'f') ||
                                 es_temporal_prefijado(tok, 's'))
                            agregar_slot_recursivo(layout, tok, 8);
                        tok = strtok(NULL, "|");
                    }
                    continue;
                }

                if (strncmp(op, "__mxrec_", 8) == 0)
                    agregar_slot_recursivo(layout, op, strstr(op, "_s_") ? 256 : 8);
                else if (es_temporal_prefijado(op, 't') ||
                         es_temporal_prefijado(op, 'f') ||
                         es_temporal_prefijado(op, 's'))
                    agregar_slot_recursivo(layout, op, 8);
            }
        }
        layout->frame_size = (layout->frame_size + 15) & ~15;
        if (!layout->frame_size) layout->frame_size = 16;
        i = j;
    }
}

static RecursiveLayout *obtener_layout_recursivo(const char *nombre)
{
    for (int i = 0; i < recursive_layout_count; ++i)
        if (strcmp(recursive_layouts[i].function_name, nombre) == 0) return &recursive_layouts[i];
    return NULL;
}

static const char *mem_ref(const char *operando)
{
    static char buf[16][256];
    static int idx = 0;
    idx = (idx + 1) % 16;
    if (current_asm_recursive_layout)
    {
        for (int i = 0; i < current_asm_recursive_layout->slot_count; ++i)
            if (strcmp(current_asm_recursive_layout->slots[i].name, operando) == 0)
            {
                snprintf(buf[idx], sizeof(buf[idx]), "[rbp-%d]", current_asm_recursive_layout->slots[i].offset);
                return buf[idx];
            }
    }
    snprintf(buf[idx], sizeof(buf[idx]), "[rel %s]", asm_symbol(operando));
    return buf[idx];
}

static void emitir_carga_entero(FILE *f, const char *reg, const char *op)
{
    if (is_number(op)) fprintf(f, "    mov %s, %s\n", reg, op);
    else fprintf(f, "    mov %s, %s\n", reg, mem_ref(op));
}

static void emitir_guardado_entero(FILE *f, const char *op, const char *reg)
{
    fprintf(f, "    mov %s, %s\n", mem_ref(op), reg);
}

static void emitir_guardado_float(FILE *f, const char *op, const char *reg)
{
    fprintf(f, "    movsd %s, %s\n", mem_ref(op), reg);
}

static void emitir_parametros_recursivos(FILE *f, ASTNode *fn)
{
    int index = 0;
    for (ASTNode *p = fn ? fn->parametros : NULL; p; p = p->siguiente_hermano, ++index)
    {
        if (!p->hijo_izq || !p->hijo_izq->valor.nombre_id) continue;
        enum TipoDato type = tipo_nodo(p->hijo_izq);
        char *local = nombre_local_funcion_actual(p->hijo_izq->valor.nombre_id, type);
        RecursiveSlot *slot = NULL;
        if (current_asm_recursive_layout)
            for (int i = 0; i < current_asm_recursive_layout->slot_count; ++i)
                if (strcmp(current_asm_recursive_layout->slots[i].name, local) == 0) { slot=&current_asm_recursive_layout->slots[i]; break; }
        if (!slot) { free(local); continue; }
#if defined(_WIN32)
        fprintf(f, "    mov rax, [rbp+%d]\n    mov [rbp-%d], rax\n", 48 + index*8, slot->offset);
#else
        fprintf(f, "    mov rax, [rbp+%d]\n    mov [rbp-%d], rax\n", 16 + index*8, slot->offset);
#endif
        free(local);
    }
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
    int uses_decimales = 0;
    preparar_simbolos_flotantes();
    preparar_layouts_recursivos();
    const char *data_section = "section .data\n";
    const char *bss_section = "section .bss\n";
    const char *text_section = "section .text\n";
    const char *printf_sym = "printf";
    const char *scanf_sym = "scanf";
    const char *main_sym = "main";

    fprintf(f, "BITS 64\n");

    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        if (q->op == IR_PRINT)
        {
            uses_print = 1;
            if (is_string_literal(asm_symbol(q->arg1)))
                uses_string_print = 1;
            else if (es_operando_flotante_codegen(q->arg1))
            {
                uses_float_print = 1;
                uses_decimales = 1;
            }
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
            const char *native = asm_symbol(q->arg1);
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
            else if (strcmp(native, "Decimales") == 0)
            {
                uses_decimales = 1;
                uses_pow = 1;
                uses_round = 1;
            }
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
    if (uses_int_print)
        fprintf(f, "fmt_int db \"%%lld\", 10, 0\n");
    if (uses_float_print)
        fprintf(f, "fmt_float db \"%%f\", 10, 0\n");

    if (uses_decimales)
        fprintf(f, "fmt_float_dec db \"%%.*f\", 10, 0\n");
    if (uses_string_print)
        fprintf(f, "fmt_str db \"%%s\", 10, 0\n");

    if (uses_int_read)
        fprintf(f, "fmt_read_int db \"%%lld\", 0\n");
    if (uses_float_read)
        fprintf(f, "fmt_read_float db \"%%lf\", 0\n");
    if (uses_string_read)
        fprintf(f, "fmt_read_str db \"%%255s\", 0\n");

    // Literales string (buscar en todas las operaciones)
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        // Buscar cadenas en arg1 y arg2
        if (is_string_literal(asm_symbol(q->arg1)))
        {
            fprintf(f, "str_%d db ", i);
            print_asm_string_literal(f, strip_quotes(asm_symbol(q->arg1)));
            fprintf(f, ", 0\n");
        }
        if (is_string_literal(asm_symbol(q->arg2)))
        {
            fprintf(f, "str_%d_2 db ", i);
            print_asm_string_literal(f, strip_quotes(asm_symbol(q->arg2)));
            fprintf(f, ", 0\n");
        }
        if (q->op == IR_NATIVE_CALL && asm_symbol(q->arg2))
        {
            char args[512];
            snprintf(args, sizeof(args), "%s", asm_symbol(q->arg2));
            char *arg = strtok(args, "|");
            int arg_index = 0;
            while (arg)
            {
                if (is_string_literal(arg))
                {
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
    if (uses_decimales)
    {
        fprintf(f, "    __decimales_factor resq 1\n");
        fprintf(f, "    __decimales_precision resq 1\n");
    }
    for (int i = 0; i < ir_current_size; i++)
    {
        Quadruple *q = &ir_code[i];
        const char *args[] = {
            (q->op == IR_NATIVE_CALL || q->op == IR_CALL || q->op == IR_FUNCTION_BEGIN || q->op == IR_FUNCTION_END) ? NULL : asm_symbol(q->arg1),
            asm_symbol(q->arg2), asm_symbol(q->result)};
        for (int j = 0; j < 3; j++)
        {
            const char *var = args[j];
            if ((q->op == IR_FUNCTION_BEGIN || q->op == IR_FUNCTION_END) && j < 2)
                continue;
            if (var && !es_operando_nulo(var) && is_valid_varname(var) &&
                !var_declared(declared_vars, declared_vars_count, var))
            {
                if (!(var[0] == 'L' && isdigit((unsigned char)var[1])))
                {
                    if (declared_vars_count >= MAX_BUFFER || strlen(var) >= sizeof(declared_vars[0]))
                    {
                        fprintf(stderr, "Error: demasiadas variables o nombre de variable demasiado largo.\n");
                        exit(EXIT_FAILURE);
                    }
                    snprintf(declared_vars[declared_vars_count++], sizeof(declared_vars[0]), "%s", var);
                    EntradaSimbolo *entry = buscar_simbolo(ambito_actual, var);
                    if (entry != NULL && entry->tipo == STRING)
                        fprintf(f, "    %s resb 256\n", var);
                    else
                        fprintf(f, "    %s resq 1\n", var);
                }
            }
        }
        if (q->op == IR_NATIVE_CALL && asm_symbol(q->arg2))
        {
            char native_args[512];
            snprintf(native_args, sizeof(native_args), "%s", asm_symbol(q->arg2));
            char *arg = strtok(native_args, "|");
            while (arg)
            {
                const char *asm_arg = asm_symbol(arg);
                if (!es_operando_nulo(arg) && is_valid_varname(arg) &&
                    !var_declared(declared_vars, declared_vars_count, asm_arg))
                {
                    if (declared_vars_count >= MAX_BUFFER || strlen(arg) >= sizeof(declared_vars[0]))
                    {
                        fprintf(stderr, "Error: demasiadas variables o nombre de variable demasiado largo.\n");
                        exit(EXIT_FAILURE);
                    }
                    snprintf(declared_vars[declared_vars_count++], sizeof(declared_vars[0]), "%s", asm_arg);
                    EntradaSimbolo *entry = buscar_simbolo(ambito_actual, arg);
                    if (entry && entry->tipo == STRING)
                        fprintf(f, "    %s resb 256\n", asm_arg);
                    else
                        fprintf(f, "    %s resq 1\n", asm_arg);
                }
                arg = strtok(NULL, "|");
            }
        }
    }

    // Código principal
    fprintf(f, "%s", text_section);
    fprintf(f, "global %s\n", main_sym);
    if (uses_print)
        fprintf(f, "extern %s\n", printf_sym);
    if (uses_read)
        fprintf(f, "extern %s\n", scanf_sym);
    {
        const char *native_prefix =
#if defined(__APPLE__)
            "_";
#else
            "";
#endif
        if (uses_strlen)
            fprintf(f, "extern %sstrlen\n", native_prefix);
        if (uses_strcmp)
            fprintf(f, "extern %sstrcmp\n", native_prefix);
        if (uses_strstr)
            fprintf(f, "extern %sstrstr\n", native_prefix);
        if (uses_sqrt)
            fprintf(f, "extern %ssqrt\n", native_prefix);
        if (uses_sin)
            fprintf(f, "extern %ssin\n", native_prefix);
        if (uses_cos)
            fprintf(f, "extern %scos\n", native_prefix);
        if (uses_tan)
            fprintf(f, "extern %stan\n", native_prefix);
        if (uses_log)
            fprintf(f, "extern %slog\n", native_prefix);
        if (uses_exp)
            fprintf(f, "extern %sexp\n", native_prefix);
        if (uses_floor)
            fprintf(f, "extern %sfloor\n", native_prefix);
        if (uses_ceil)
            fprintf(f, "extern %sceil\n", native_prefix);
        if (uses_pow)
            fprintf(f, "extern %spow\n", native_prefix);
        if (uses_round)
            fprintf(f, "extern %sround\n", native_prefix);
        if (uses_rand)
            fprintf(f, "extern %srand\n", native_prefix);
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
    if (uses_decimales)
        fprintf(f, "    mov qword [rel __decimales_precision], 6\n");
#if defined(_WIN32)
    fprintf(f, "    sub rsp, 32\n");
#endif

    int function_ir_start = ir_current_size;
    for (int i = 0; i < ir_current_size; ++i)
        if (ir_code[i].op == IR_FUNCTION_BEGIN) { function_ir_start = i; break; }

    for (int i = 0; i < function_ir_start; i++)
    {
        Quadruple *q = &ir_code[i];

        if (q->op == IR_LABEL)
        {
            fprintf(f, "%s:\n", asm_symbol(q->result));
            continue;
        }

        switch (q->op)
        {
        case IR_LOAD_ARRAY:
        {
            if (is_number(asm_symbol(q->arg2))) fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg2));
            else fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg2));
            fprintf(f, "    lea rdx, [rel %s]\n", asm_symbol(q->arg1));
            fprintf(f, "    mov rax, [rdx + rax*8]\n");
            if (es_operando_flotante_codegen(q->result))
            { fprintf(f, "    movq xmm0, rax\n"); fprintf(f, "    movsd [rel %s], xmm0\n", asm_symbol(q->result)); }
            else fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
            break;
        }
        case IR_STORE_ARRAY:
        {
            if (is_number(asm_symbol(q->arg2))) fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg2));
            else fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg2));
            fprintf(f, "    lea rdx, [rel %s]\n", asm_symbol(q->arg1));
            if (es_operando_flotante_codegen(q->result))
            { cargar_float_en_xmm(f, asm_symbol(q->result), "xmm0"); fprintf(f, "    movsd [rdx + rax*8], xmm0\n"); }
            else { if (is_number(asm_symbol(q->result))) fprintf(f, "    mov rcx, %s\n", asm_symbol(q->result)); else fprintf(f, "    mov rcx, [rel %s]\n", asm_symbol(q->result)); fprintf(f, "    mov [rdx + rax*8], rcx\n"); }
            break;
        }

        case IR_ASSIGN:
            if (es_operando_flotante_codegen(q->arg1))
            {
                cargar_float_en_xmm(f, asm_symbol(q->arg1), "xmm0");
                fprintf(f, "    movsd [rel %s], xmm0\n", asm_symbol(q->result));
            }
            else if (is_number(asm_symbol(q->arg1)))
            {
                fprintf(f, "    mov rax, %s\n    mov [rel %s], rax\n", asm_symbol(q->arg1), asm_symbol(q->result));
            }
            else if (is_string_literal(asm_symbol(q->arg1)))
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
                            i, asm_symbol(q->result), i, i);
                }
                else
                {
                    // Si no es STRING, copiar la dirección (para compatibilidad)
                    fprintf(f, "    lea rax, [rel str_%d]\n    mov [rel %s], rax\n", i, asm_symbol(q->result));
                }
            }
            else if (es_temporal_prefijado(asm_symbol(q->arg1), 's') &&
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
                        asm_symbol(q->arg1), asm_symbol(q->result), i, i);
            }
            else
            {
                fprintf(f, "    mov rax, [rel %s]\n    mov [rel %s], rax\n", asm_symbol(q->arg1), asm_symbol(q->result));
            }
            break;

        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
        case IR_DIV:
        case IR_MOD:
        {
            if (es_operando_flotante_codegen(q->arg1) || es_operando_flotante_codegen(q->arg2))
            {
                if (q->op == IR_MOD)
                {
                    fprintf(stderr, "Error: el operador %% no admite operandos flotantes.\n");
                    exit(EXIT_FAILURE);
                }
                cargar_float_en_xmm(f, asm_symbol(q->arg1), "xmm0");
                cargar_float_en_xmm(f, asm_symbol(q->arg2), "xmm1");
                if (q->op == IR_ADD)
                    fprintf(f, "    addsd xmm0, xmm1\n");
                else if (q->op == IR_SUB)
                    fprintf(f, "    subsd xmm0, xmm1\n");
                else if (q->op == IR_MUL)
                    fprintf(f, "    mulsd xmm0, xmm1\n");
                else
                    fprintf(f, "    divsd xmm0, xmm1\n");
                fprintf(f, "    movsd [rel %s], xmm0\n", asm_symbol(q->result));
                break;
            }
            const char *op;
            if (q->op == IR_ADD)
                op = "add";
            else if (q->op == IR_SUB)
                op = "sub";
            else if (q->op == IR_MUL)
                op = "imul";
            else if (q->op == IR_DIV || q->op == IR_MOD)
                op = "idiv";
            else
                op = "";

            if (q->op == IR_DIV || q->op == IR_MOD)
            {
                if (es_literal(asm_symbol(q->arg1)))
                    fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg1));
                else
                    fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg1));
                fprintf(f, "    cqo\n");

                if (es_literal(asm_symbol(q->arg2)))
                    fprintf(f, "    mov rcx, %s\n", asm_symbol(q->arg2));
                else
                    fprintf(f, "    mov rcx, [rel %s]\n", asm_symbol(q->arg2));

                fprintf(f, "    idiv rcx\n");

                if (q->op == IR_DIV)
                    fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
                else
                    fprintf(f, "    mov [rel %s], rdx\n", asm_symbol(q->result));
            }
            else
            {
                if (es_literal(asm_symbol(q->arg1)))
                    fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg1));
                else
                    fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg1));

                if (es_literal(asm_symbol(q->arg2)))
                    fprintf(f, "    %s rax, %s\n", op, asm_symbol(q->arg2));
                else
                    fprintf(f, "    %s rax, [rel %s]\n", op, asm_symbol(q->arg2));

                fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
            }
            break;
        }

        case IR_NEG:
            if (es_operando_flotante_codegen(q->arg1))
            {
                fprintf(f, "    pxor xmm1, xmm1\n");
                cargar_float_en_xmm(f, asm_symbol(q->arg1), "xmm0");
                fprintf(f, "    subsd xmm1, xmm0\n    movsd [rel %s], xmm1\n", asm_symbol(q->result));
                break;
            }
            if (es_literal(asm_symbol(q->arg1)))
                fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg1));
            else
                fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg1));
            fprintf(f, "    neg rax\n");
            fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
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
            case IR_LT:
                cond = "l";
                break;
            case IR_GT:
                cond = "g";
                break;
            case IR_LE:
                cond = "le";
                break;
            case IR_GE:
                cond = "ge";
                break;
            case IR_EQ:
                cond = "e";
                break;
            case IR_NE:
                cond = "ne";
                break;
            default:
                cond = "e";
                break;
            }

            if (es_operando_flotante_codegen(q->arg1) || es_operando_flotante_codegen(q->arg2))
            {
                const char *float_cond;
                switch (q->op)
                {
                case IR_LT:
                    float_cond = "b";
                    break;
                case IR_GT:
                    float_cond = "a";
                    break;
                case IR_LE:
                    float_cond = "be";
                    break;
                case IR_GE:
                    float_cond = "ae";
                    break;
                case IR_EQ:
                    float_cond = "e";
                    break;
                default:
                    float_cond = "ne";
                    break;
                }
                cargar_float_en_xmm(f, asm_symbol(q->arg1), "xmm0");
                cargar_float_en_xmm(f, asm_symbol(q->arg2), "xmm1");
                fprintf(f, "    ucomisd xmm0, xmm1\n    set%s al\n"
                           "    movzx rax, al\n    mov [rel %s], rax\n",
                        float_cond, asm_symbol(q->result));
                break;
            }

            if (is_number(asm_symbol(q->arg1)))
            {
                fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg1));
            }
            else
            {
                fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg1));
            }

            if (is_number(asm_symbol(q->arg2)))
            {
                fprintf(f, "    cmp rax, %s\n", asm_symbol(q->arg2));
            }
            else
            {
                fprintf(f, "    cmp rax, [rel %s]\n", asm_symbol(q->arg2));
            }
            fprintf(f, "    set%s al\n", cond);
            fprintf(f, "    movzx rax, al\n");
            fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
            break;
        }

        case IR_AND:
            if (is_number(asm_symbol(q->arg1)))
                fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg1));
            else
                fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg1));
            fprintf(f, "    test rax, rax\n    setne al\n    movzx rax, al\n");

            if (is_number(asm_symbol(q->arg2)))
                fprintf(f, "    mov rcx, %s\n", asm_symbol(q->arg2));
            else
                fprintf(f, "    mov rcx, [rel %s]\n", asm_symbol(q->arg2));
            fprintf(f, "    test rcx, rcx\n    setne cl\n    movzx rcx, cl\n    and rax, rcx\n");
            fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
            break;

        case IR_OR:
            if (is_number(asm_symbol(q->arg1)))
                fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg1));
            else
                fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg1));
            fprintf(f, "    test rax, rax\n    setne al\n    movzx rax, al\n");

            if (is_number(asm_symbol(q->arg2)))
                fprintf(f, "    mov rcx, %s\n", asm_symbol(q->arg2));
            else
                fprintf(f, "    mov rcx, [rel %s]\n", asm_symbol(q->arg2));
            fprintf(f, "    test rcx, rcx\n    setne cl\n    movzx rcx, cl\n    or rax, rcx\n");
            fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
            break;

        case IR_NOT:
            if (is_number(asm_symbol(q->arg1)))
                fprintf(f, "    mov rax, %s\n", asm_symbol(q->arg1));
            else
                fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(q->arg1));
            fprintf(f, "    cmp rax, 0\n");
            fprintf(f, "    sete al\n");
            fprintf(f, "    movzx rax, al\n");
            fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
            break;

        case IR_PRINT:
        {
            if (es_operando_nulo(q->arg1))
                break;

            EntradaSimbolo *entry = buscar_simbolo(ambito_actual, q->arg1);

#if defined(_WIN32)
            if (is_string_literal(asm_symbol(q->arg1)))
            {
                fprintf(f,
                        "    lea rcx, [rel fmt_str]\n"
                        "    lea rdx, [rel str_%d]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        i, printf_sym);
            }
            else if (is_number(asm_symbol(q->arg1)))
            {
                fprintf(f,
                        "    lea rcx, [rel fmt_int]\n"
                        "    mov rdx, %s\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        asm_symbol(q->arg1), printf_sym);
            }
            else if (es_operando_flotante_codegen(q->arg1))
            {
                cargar_float_en_xmm(f, asm_symbol(q->arg1), "xmm0");
                fprintf(f,
                        "    mov rdx, [rel __decimales_precision]\n"
                        "    lea rcx, [rel fmt_float_dec]\n"
                        "    movq r8, xmm0\n"
                        "    mov eax, 1\n"
                        "    call %s\n",
                        printf_sym);
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
                            asm_symbol(q->arg1), printf_sym);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                            "    movsd xmm0, qword [rel %s]\n"
                            "    mov rdx, [rel __decimales_precision]\n"
                            "    lea rcx, [rel fmt_float_dec]\n"
                            "    mov eax, 1\n"
                            "    call %s\n",
                            asm_symbol(q->arg1), printf_sym);
                }
                else
                {
                    fprintf(f,
                            "    lea rcx, [rel fmt_int]\n"
                            "    mov rdx, [rel %s]\n"
                            "    xor eax, eax\n"
                            "    call %s\n",
                            asm_symbol(q->arg1), printf_sym);
                }
            }
            else
            {
                fprintf(f,
                        "    lea rcx, [rel fmt_int]\n"
                        "    mov rdx, [rel %s]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        asm_symbol(q->arg1), printf_sym);
            }
#else
            if (is_string_literal(asm_symbol(q->arg1)))
            {
                fprintf(f,
                        "    lea rdi, [rel fmt_str]\n"
                        "    lea rsi, [rel str_%d]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        i, printf_sym);
            }
            else if (is_number(asm_symbol(q->arg1)))
            {
                fprintf(f,
                        "    mov rsi, %s\n"
                        "    lea rdi, [rel fmt_int]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        asm_symbol(q->arg1), printf_sym);
            }
            else if (es_operando_flotante_codegen(q->arg1))
            {
                cargar_float_en_xmm(f, asm_symbol(q->arg1), "xmm0");
                fprintf(f,
                        "    mov rsi, [rel __decimales_precision]\n"
                        "    lea rdi, [rel fmt_float_dec]\n"
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
                            asm_symbol(q->arg1), printf_sym);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                            "    lea rdi, [rel fmt_read_float]\n"
                            "    lea rsi, [rel %s]\n"
                            "    xor eax, eax\n"
                            "    call %s\n",
                            asm_symbol(q->result), scanf_sym);
                }
                else
                {
                    fprintf(f,
                            "    mov rsi, [rel %s]\n"
                            "    lea rdi, [rel fmt_int]\n"
                            "    xor eax, eax\n"
                            "    call %s\n",
                            asm_symbol(q->arg1), printf_sym);
                }
            }
            else if (es_temporal_prefijado(asm_symbol(q->arg1), 's'))
            {
                fprintf(f,
                        "    mov rsi, [rel %s]\n"
                        "    lea rdi, [rel fmt_str]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        asm_symbol(q->arg1), printf_sym);
            }
            else
            {
                fprintf(f,
                        "    mov rsi, [rel %s]\n"
                        "    lea rdi, [rel fmt_int]\n"
                        "    xor eax, eax\n"
                        "    call %s\n",
                        asm_symbol(q->arg1), printf_sym);
            }
#endif
            break;
        }

        case IR_CALL:
        {
            char args_call[4096];
            snprintf(args_call, sizeof(args_call), "%s", q->arg2 ? q->arg2 : "");
            char *argv[64] = {0}; int argc = 0;
            char *tok = strtok(args_call, "|");
            while (tok && argc < 64) { argv[argc++] = tok; tok = strtok(NULL, "|"); }
#if defined(_WIN32)
            /* Win64: el caller debe reservar 32 bytes de shadow space antes
             * de colocar los argumentos en la pila. */
            int pad = (argc % 2 == 1) ? 8 : 0;
            if (pad) fprintf(f, "    sub rsp, 8\n");
            fprintf(f, "    sub rsp, 32\n");
#else
            /* SysV: con el prólogo actual, una cantidad impar de pushes
             * requiere 8 bytes de padding para conservar la alineación de 16
             * bytes antes del CALL. El padding va antes de los argumentos. */
            int pad = (argc % 2 == 1) ? 8 : 0;
            if (pad) fprintf(f, "    sub rsp, 8\n");
#endif
            for (int n = argc - 1; n >= 0; --n)
            {
                if (is_number(argv[n])) fprintf(f, "    mov rax, %s\n", argv[n]);
                else if (strncmp(argv[n], "__float_", 8) == 0)
                {
                    unsigned long long bits = 0; sscanf(argv[n] + 8, "%llx", &bits);
                    fprintf(f, "    mov rax, 0x%llx\n", bits);
                }
                else fprintf(f, "    mov rax, %s\n", mem_ref(argv[n]));
                fprintf(f, "    push rax\n");
            }
            fprintf(f, "    call %s\n", asm_symbol(q->arg1));
            if (argc) fprintf(f, "    add rsp, %d\n", argc * 8);
#if defined(_WIN32)
            fprintf(f, "    add rsp, 32\n");
#endif
            if (pad) fprintf(f, "    add rsp, 8\n");
#if defined(_WIN32)
            fprintf(f, "    add rsp, 32\n");
#endif
            if (q->result)
            {
                if (es_operando_flotante_codegen(q->result)) fprintf(f, "    movsd %s, xmm0\n", mem_ref(q->result));
                else fprintf(f, "    mov %s, rax\n", mem_ref(q->result));
            }
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
            snprintf(args, sizeof(args), "%s", asm_symbol(q->arg2) ? asm_symbol(q->arg2) : "");
            char *first = strtok(args, "|");
            char *second = strtok(NULL, "|");
            char *third = strtok(NULL, "|");
            char *fourth = strtok(NULL, "|");
            int is_string = strcmp(asm_symbol(q->arg1), "Longitud") == 0 ||
                            strcmp(asm_symbol(q->arg1), "Comparar") == 0 ||
                            strcmp(asm_symbol(q->arg1), "Contiene") == 0;
            if (strcmp(asm_symbol(q->arg1), "Http") == 0)
            {
                char *http_args[] = {first, second, third, fourth};
#if defined(_WIN32)
                const char *registers[] = {"rcx", "rdx", "r8", "r9"};
#else
                const char *registers[] = {"rdi", "rsi", "rdx", "rcx"};
#endif
                for (int n = 0; n < 4; n++)
                {
                    if (!http_args[n])
                        fprintf(f, "    xor %s, %s\n", registers[n], registers[n]);
                    else if (is_string_literal(asm_symbol(http_args[n])))
                        fprintf(f, "    lea %s, [rel str_native_%d_%d]\n", registers[n], i, n);
                    else
                        fprintf(f, "    lea %s, [rel %s]\n", registers[n], asm_symbol(http_args[n]));
                }
                fprintf(f, "    call %smx_http_request\n    mov [rel %s], rax\n",
                        native_prefix, asm_symbol(q->result));
            }
            else if (strcmp(asm_symbol(q->arg1), "HttpCuerpo") == 0)
            {
                fprintf(f, "    call %smx_http_body\n    mov [rel %s], rax\n",
                        native_prefix, asm_symbol(q->result));
            }
            else if (strcmp(asm_symbol(q->arg1), "HttpCabeceras") == 0)
            {
                fprintf(f, "    call %smx_http_headers\n    mov [rel %s], rax\n",
                        native_prefix, asm_symbol(q->result));
            }
            else if (strcmp(asm_symbol(q->arg1), "Aleatorio") == 0 || strcmp(asm_symbol(q->arg1), "AleatorioEntre") == 0)
            {
                fprintf(f, "    call %srand\n", native_prefix);
                fprintf(f, "    movsxd rax, eax\n");
                if (strcmp(asm_symbol(q->arg1), "Aleatorio") == 0 && first)
                {
                    if (is_number(first))
                        fprintf(f, "    mov rcx, %s\n", first);
                    else
                        fprintf(f, "    mov rcx, [rel %s]\n", asm_symbol(first));
                    fprintf(f, "    xor rdx, rdx\n    div rcx\n    mov rax, rdx\n");
                }
                else if (strcmp(asm_symbol(q->arg1), "AleatorioEntre") == 0)
                {
                    if (is_number(first))
                        fprintf(f, "    mov rcx, %s\n", first);
                    else
                        fprintf(f, "    mov rcx, [rel %s]\n", asm_symbol(first));
                    if (is_number(second))
                        fprintf(f, "    mov r8, %s\n", second);
                    else
                        fprintf(f, "    mov r8, [rel %s]\n", asm_symbol(second));
                    fprintf(f, "    sub r8, rcx\n    inc r8\n    xor rdx, rdx\n    div r8\n    add rdx, rcx\n    mov rax, rdx\n");
                }
                fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
            }
            else if (is_string)
            {
                if (is_string_literal(first))
                    fprintf(f, "    lea rdi, [rel str_native_%d_0]\n", i);
                else
                    fprintf(f, "    lea rdi, [rel %s]\n", asm_symbol(first));
                if (second)
                {
                    if (is_string_literal(second))
                        fprintf(f, "    lea rsi, [rel str_native_%d_1]\n", i);
                    else
                        fprintf(f, "    lea rsi, [rel %s]\n", asm_symbol(second));
                }
                fprintf(f, "    call %s%s\n", native_prefix,
                        strcmp(asm_symbol(q->arg1), "Longitud") == 0 ? "strlen" : strcmp(asm_symbol(q->arg1), "Comparar") == 0 ? "strcmp"
                                                                                                       : "strstr");
                if (strcmp(asm_symbol(q->arg1), "Contiene") == 0)
                {
                    fprintf(f, "    test rax, rax\n    setne al\n    movzx rax, al\n");
                }
                fprintf(f, "    mov [rel %s], rax\n", asm_symbol(q->result));
            }
            else if (strcmp(asm_symbol(q->arg1), "Abs") == 0 || strcmp(asm_symbol(q->arg1), "Absoluto") == 0)
            {
                if (is_number(first))
                    fprintf(f, "    mov rax, %s\n", first);
                else
                    fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(first));
                fprintf(f, "    cqo\n    xor rax, rdx\n    sub rax, rdx\n    mov [rel %s], rax\n", asm_symbol(q->result));
            }
            else if (strcmp(asm_symbol(q->arg1), "Decimales") == 0)
            {
                fprintf(f, "    mov rax, 0x4024000000000000\n");
                fprintf(f, "    movq xmm0, rax\n");

                if (is_number(second))
                {
                    fprintf(f, "    mov eax, %s\n", second);
                    fprintf(f, "    cvtsi2sd xmm1, eax\n");
                    fprintf(f, "    mov [rel __decimales_precision], rax\n");
                }
                else if (second && !es_operando_nulo(second))
                {
                    fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(second));
                    fprintf(f, "    cvtsi2sd xmm1, rax\n");
                    fprintf(f, "    mov [rel __decimales_precision], rax\n");
                }
                else
                {
                    fprintf(f,
                            "    mov rax, 6\n"
                            "    mov [rel __decimales_precision], rax\n"
                            "    cvtsi2sd xmm1, eax\n");
                }

                fprintf(f, "    call %spow\n", native_prefix);
                fprintf(f, "    movsd [rel __decimales_factor], xmm0\n");

                cargar_float_en_xmm(f, first, "xmm0");

                fprintf(f, "    mulsd xmm0, [rel __decimales_factor]\n");
                fprintf(f, "    call %sround\n", native_prefix);
                fprintf(f, "    divsd xmm0, [rel __decimales_factor]\n");
                fprintf(f, "    movsd [rel %s], xmm0\n", asm_symbol(q->result));
            }
            else if (strcmp(asm_symbol(q->arg1), "Min") == 0 || strcmp(asm_symbol(q->arg1), "Max") == 0)
            {
                if (is_number(first))
                    fprintf(f, "    mov rax, %s\n", first);
                else
                    fprintf(f, "    mov rax, [rel %s]\n", asm_symbol(first));
                if (is_number(second))
                    fprintf(f, "    mov rcx, %s\n", second);
                else
                    fprintf(f, "    mov rcx, [rel %s]\n", asm_symbol(second));
                fprintf(f, "    cmp rax, rcx\n");
                fprintf(f, "    cmov%s rax, rcx\n    mov [rel %s], rax\n",
                        strcmp(asm_symbol(q->arg1), "Min") == 0 ? "g" : "l", asm_symbol(q->result));
            }
            else
            {
                const char *symbol = strcmp(asm_symbol(q->arg1), "Potencia") == 0 ? "pow" : strcmp(asm_symbol(q->arg1), "RaizCuadrada") == 0 ? "sqrt"
                                                                            : strcmp(asm_symbol(q->arg1), "Seno") == 0           ? "sin"
                                                                            : strcmp(asm_symbol(q->arg1), "Coseno") == 0         ? "cos"
                                                                            : strcmp(asm_symbol(q->arg1), "Tangente") == 0       ? "tan"
                                                                            : strcmp(asm_symbol(q->arg1), "Logaritmo") == 0      ? "log"
                                                                            : strcmp(asm_symbol(q->arg1), "Exponencial") == 0    ? "exp"
                                                                            : strcmp(asm_symbol(q->arg1), "Piso") == 0           ? "floor"
                                                                            : strcmp(asm_symbol(q->arg1), "Techo") == 0          ? "ceil"
                                                                                                                     : "round";
                cargar_float_en_xmm(f, first, "xmm0");
                if (second)
                    cargar_float_en_xmm(f, second, "xmm1");
                fprintf(f, "    call %s%s\n", native_prefix, symbol);
                if (strcmp(asm_symbol(q->arg1), "Redondear") == 0)
                {
                    fprintf(f, "    cvttsd2si rax, xmm0\n    mov [rel %s], rax\n", asm_symbol(q->result));
                }
                else
                    fprintf(f, "    movsd [rel %s], xmm0\n", asm_symbol(q->result));
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
                            asm_symbol(q->result), scanf_sym);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                            "    lea rcx, [rel fmt_read_float]\n"
                            "    lea rdx, [rel %s]\n"
                            "    xor eax, eax\n"
                            "    call %s\n",
                            asm_symbol(q->result), scanf_sym);
                }
                else
                {
                    fprintf(f,
                            "    lea rcx, [rel fmt_read_int]\n"
                            "    lea rdx, [rel %s]\n"
                            "    xor eax, eax\n"
                            "    call %s\n",
                            asm_symbol(q->result), scanf_sym);
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
                            asm_symbol(q->result), scanf_sym);
                }
                else if (entry->tipo == FLOAT)
                {
                    fprintf(f,
                            "    lea rdi, [rel fmt_read_float]\n"
                            "    lea rsi, [rel %s]\n"
                            "    xor eax, eax\n"
                            "    call %s\n",
                            asm_symbol(q->result), scanf_sym);
                }
                else
                {
                    fprintf(f,
                            "    lea rdi, [rel fmt_read_int]\n"
                            "    lea rsi, [rel %s]\n"
                            "    xor eax, eax\n"
                            "    call %s\n",
                            asm_symbol(q->result), scanf_sym);
                }
            }
#endif
            break;
        }

        case IR_GOTO:
            fprintf(f, "    jmp %s\n", asm_symbol(q->result));
            break;

        case IR_IF_FALSE_GOTO:
            if (is_number(asm_symbol(q->arg1)))
            {
                fprintf(f,
                        "    mov rax, %s\n"
                        "    cmp rax, 0\n"
                        "    je %s\n",
                        asm_symbol(q->arg1), asm_symbol(q->result));
            }
            else
            {
                fprintf(f,
                        "    mov rax, [rel %s]\n"
                        "    cmp rax, 0\n"
                        "    je %s\n",
                        asm_symbol(q->arg1), asm_symbol(q->result));
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

#if defined(_WIN32)
    fprintf(f, "    extern ExitProcess\n");
    fprintf(f, "    mov ecx, 0\n");
    fprintf(f, "    call ExitProcess\n");
#else
    fprintf(f, "    mov eax, 0\n");
    fprintf(f, "    pop rbp\n");
    fprintf(f, "    ret\n");
#endif

    for (int i = function_ir_start; i < ir_current_size; ++i)
    {
        if (ir_code[i].op != IR_FUNCTION_BEGIN) continue;
        RecursiveLayout *layout = obtener_layout_recursivo(ir_code[i].arg1);
        if (!layout) continue;
        current_asm_recursive_layout = layout;

        ASTNode *fn_ast = NULL;
        ASTNode *cfn = program_root ? program_root->hijo_izq : NULL;
        while (cfn)
        {
            if (cfn->type == AST_FUNCION && cfn->hijo_izq && cfn->hijo_izq->valor.nombre_id)
            {
                char *cand = nombre_funcion_asm_recursiva(cfn->hijo_izq->valor.nombre_id);
                int same = strcmp(cand, ir_code[i].arg1) == 0;
                free(cand);
                if (same) { fn_ast = cfn; break; }
            }
            cfn = cfn->siguiente_hermano;
        }

        current_recursive_function_codegen = fn_ast;
        fprintf(f, "\n%s:\n    push rbp\n    mov rbp, rsp\n", asm_symbol(ir_code[i].arg1));
#if defined(_WIN32)
        fprintf(f, "    sub rsp, %d\n", layout->frame_size + 32);
#else
        fprintf(f, "    sub rsp, %d\n", layout->frame_size);
#endif
        if (fn_ast) emitir_parametros_recursivos(f, fn_ast);

        int j = i + 1;
        for (; j < ir_current_size && ir_code[j].op != IR_FUNCTION_END; ++j)
        {
            Quadruple *qf = &ir_code[j];
            if (qf->op == IR_LABEL) { fprintf(f, "%s:\n", asm_symbol(qf->result)); continue; }
            switch (qf->op)
            {
            case IR_ASSIGN:
                if (es_operando_flotante_codegen(qf->arg1)) { cargar_float_en_xmm(f, qf->arg1, "xmm0"); emitir_guardado_float(f, qf->result, "xmm0"); }
                else if (is_number(qf->arg1)) { fprintf(f, "    mov rax, %s\n", qf->arg1); emitir_guardado_entero(f, qf->result, "rax"); }
                else { emitir_carga_entero(f, "rax", qf->arg1); emitir_guardado_entero(f, qf->result, "rax"); }
                break;
            case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
                if (es_operando_flotante_codegen(qf->arg1) || es_operando_flotante_codegen(qf->arg2))
                {
                    cargar_float_en_xmm(f, qf->arg1, "xmm0"); cargar_float_en_xmm(f, qf->arg2, "xmm1");
                    if (qf->op == IR_ADD) fprintf(f,"    addsd xmm0, xmm1\n");
                    else if (qf->op == IR_SUB) fprintf(f,"    subsd xmm0, xmm1\n");
                    else if (qf->op == IR_MUL) fprintf(f,"    mulsd xmm0, xmm1\n");
                    else if (qf->op == IR_DIV) fprintf(f,"    divsd xmm0, xmm1\n");
                    else { fprintf(stderr,"Error: modulo flotante en función recursiva.\n"); exit(EXIT_FAILURE); }
                    emitir_guardado_float(f, qf->result, "xmm0");
                }
                else
                {
                    emitir_carga_entero(f,"rax",qf->arg1);
                    if (qf->op == IR_DIV || qf->op == IR_MOD)
                    { fprintf(f,"    cqo\n"); emitir_carga_entero(f,"rcx",qf->arg2); fprintf(f,"    idiv rcx\n"); emitir_guardado_entero(f,qf->result,qf->op==IR_DIV?"rax":"rdx"); }
                    else
                    {
                        const char *o = qf->op==IR_ADD?"add":qf->op==IR_SUB?"sub":"imul";
                        fprintf(f,"    %s rax, %s\n",o,is_number(qf->arg2)?qf->arg2:mem_ref(qf->arg2));
                        emitir_guardado_entero(f,qf->result,"rax");
                    }
                }
                break;
            case IR_NEG:
                if (es_operando_flotante_codegen(qf->arg1)) { fprintf(f,"    pxor xmm1,xmm1\n"); cargar_float_en_xmm(f,qf->arg1,"xmm0"); fprintf(f,"    subsd xmm1,xmm0\n"); emitir_guardado_float(f,qf->result,"xmm1"); }
                else { emitir_carga_entero(f,"rax",qf->arg1); fprintf(f,"    neg rax\n"); emitir_guardado_entero(f,qf->result,"rax"); }
                break;
            case IR_LT: case IR_GT: case IR_LE: case IR_GE: case IR_EQ: case IR_NE:
            {
                const char *cc=qf->op==IR_LT?"l":qf->op==IR_GT?"g":qf->op==IR_LE?"le":qf->op==IR_GE?"ge":qf->op==IR_EQ?"e":"ne";
                if (es_operando_flotante_codegen(qf->arg1)||es_operando_flotante_codegen(qf->arg2))
                { const char *fc=qf->op==IR_LT?"b":qf->op==IR_GT?"a":qf->op==IR_LE?"be":qf->op==IR_GE?"ae":qf->op==IR_EQ?"e":"ne"; cargar_float_en_xmm(f,qf->arg1,"xmm0"); cargar_float_en_xmm(f,qf->arg2,"xmm1"); fprintf(f,"    ucomisd xmm0,xmm1\n    set%s al\n    movzx rax,al\n",fc); emitir_guardado_entero(f,qf->result,"rax"); }
                else { emitir_carga_entero(f,"rax",qf->arg1); fprintf(f,"    cmp rax, %s\n",is_number(qf->arg2)?qf->arg2:mem_ref(qf->arg2)); fprintf(f,"    set%s al\n    movzx rax,al\n",cc); emitir_guardado_entero(f,qf->result,"rax"); }
                break;
            }
            case IR_AND:
                emitir_carga_entero(f,"rax",qf->arg1);
                fprintf(f,"    test rax,rax\n    setne al\n    movzx rax,al\n");
                emitir_carga_entero(f,"rcx",qf->arg2);
                fprintf(f,"    test rcx,rcx\n    setne cl\n    movzx rcx,cl\n    and rax,rcx\n");
                emitir_guardado_entero(f,qf->result,"rax");
                break;
            case IR_OR:
                emitir_carga_entero(f,"rax",qf->arg1);
                fprintf(f,"    test rax,rax\n    setne al\n    movzx rax,al\n");
                emitir_carga_entero(f,"rcx",qf->arg2);
                fprintf(f,"    test rcx,rcx\n    setne cl\n    movzx rcx,cl\n    or rax,rcx\n");
                emitir_guardado_entero(f,qf->result,"rax");
                break;
            case IR_NOT: emitir_carga_entero(f,"rax",qf->arg1); fprintf(f,"    cmp rax,0\n    sete al\n    movzx rax,al\n"); emitir_guardado_entero(f,qf->result,"rax"); break;
            case IR_LOAD_ARRAY:
            {
                int off=0, found=0;
                if (current_asm_recursive_layout) for (int s=0;s<current_asm_recursive_layout->slot_count;++s) if (qf->arg1 && strcmp(current_asm_recursive_layout->slots[s].name,qf->arg1)==0) { off=current_asm_recursive_layout->slots[s].offset; found=1; break; }
                if (!found) break;
                emitir_carga_entero(f,"rax",qf->arg2);
                fprintf(f,"    lea rdx, [rbp-%d]\n    mov rax, [rdx + rax*8]\n",off);
                emitir_guardado_entero(f,qf->result,"rax");
                break;
            }
            case IR_STORE_ARRAY:
            {
                int off=0, found=0;
                if (current_asm_recursive_layout) for (int s=0;s<current_asm_recursive_layout->slot_count;++s) if (qf->arg1 && strcmp(current_asm_recursive_layout->slots[s].name,qf->arg1)==0) { off=current_asm_recursive_layout->slots[s].offset; found=1; break; }
                if (!found) break;
                emitir_carga_entero(f,"rax",qf->arg2);
                fprintf(f,"    lea rdx, [rbp-%d]\n",off);
                emitir_carga_entero(f,"rcx",qf->result);
                fprintf(f,"    mov [rdx + rax*8], rcx\n");
                break;
            }
            case IR_CALL:
            {
                char acopy[4096]; snprintf(acopy,sizeof(acopy),"%s",qf->arg2?qf->arg2:""); char *av[64]={0}; int ac=0; char *tk=strtok(acopy,"|"); while(tk&&ac<64){av[ac++]=tk;tk=strtok(NULL,"|");}
#if defined(_WIN32)
                int pad=(ac%2==1)?8:0;
                if(pad)fprintf(f,"    sub rsp,8\n");
                fprintf(f,"    sub rsp,32\n");
#else
                int pad=(ac%2==1)?8:0;
                if(pad)fprintf(f,"    sub rsp,8\n");
#endif
                for(int n=ac-1;n>=0;--n){ if(is_number(av[n]))fprintf(f,"    mov rax,%s\n",av[n]); else if(strncmp(av[n],"__float_",8)==0){unsigned long long bits=0;sscanf(av[n]+8,"%llx",&bits);fprintf(f,"    mov rax,0x%llx\n",bits);} else fprintf(f,"    mov rax,%s\n",mem_ref(av[n])); fprintf(f,"    push rax\n"); }
                fprintf(f,"    call %s\n",asm_symbol(qf->arg1));
                if(ac)fprintf(f,"    add rsp,%d\n",ac*8);
#if defined(_WIN32)
                fprintf(f,"    add rsp,32\n");
#endif
                if(pad)fprintf(f,"    add rsp,8\n");
                if(qf->result){if(es_operando_flotante_codegen(qf->result))fprintf(f,"    movsd %s,xmm0\n",mem_ref(qf->result));else fprintf(f,"    mov %s,rax\n",mem_ref(qf->result));}
                break;
            }
            case IR_IF_FALSE_GOTO: emitir_carga_entero(f,"rax",qf->arg1); fprintf(f,"    cmp rax,0\n    je %s\n",asm_symbol(qf->result)); break;
            case IR_GOTO: fprintf(f,"    jmp %s\n",asm_symbol(qf->result)); break;
            default: break;
            }
        }

        if (fn_ast)
        {
            char *ret = nombre_local_funcion_actual("return", fn_ast->return_type);
            if (fn_ast->return_type == FLOAT) fprintf(f,"    movsd xmm0,%s\n",mem_ref(ret));
            else if (fn_ast->return_type == STRING) fprintf(f,"    lea rax,%s\n",mem_ref(ret));
            else fprintf(f,"    mov rax,%s\n",mem_ref(ret));
            free(ret);
        }
        fprintf(f,"    leave\n    ret\n");
        current_asm_recursive_layout = NULL;
        current_recursive_function_codegen = NULL;
        i = j;
    }

#if defined(__linux__)
    fprintf(f, "section .note.GNU-stack noalloc noexec nowrite progbits\n");
#endif
}


/* RESTAURADO: funciones base del backend */
static int es_registro_x86(const char *s)
{
    if (!s) return 0;
    static const char *regs[] = {
        "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
        "r8","r9","r10","r11","r12","r13","r14","r15",
        "eax","ebx","ecx","edx","esi","edi","ebp","esp",
        "ax","bx","cx","dx","si","di","bp","sp"
    };
    size_t n = sizeof(regs) / sizeof(regs[0]);
    for (size_t i = 0; i < n; ++i)
        if (strcmp(s, regs[i]) == 0) return 1;
    return 0;
}

static int es_palabra_reservada_asm(const char *s)
{
    if (!s)
        return 0;

    static const char *reservadas[] = {
        "aaa", "aad", "aam", "aas",
        "adc", "add", "and", "call", "cbw", "cdq", "cdqe",
        "clc", "cld", "cli", "cmc", "cmp", "cqo", "cwd", "cwde",
        "dec", "div", "enter", "idiv", "imul", "inc", "jmp",
        "jz", "je", "jne", "jnz", "jl", "jle", "jg", "jge",
        "jb", "jbe", "ja", "jae", "jo", "jno", "js", "jns",
        "lea", "leave", "mov", "movq", "movsd", "movzx", "mul",
        "neg", "nop", "not", "or", "pop", "push", "ret", "sar",
        "sbb", "seta", "setae", "setb", "setbe", "sete", "setg",
        "setge", "setl", "setle", "setne", "setno", "setns", "seto",
        "sets", "shl", "shr", "sub", "test", "xor",
        "addsd", "subsd", "mulsd", "divsd", "comisd", "ucomisd",
        "pxor", "cvtsi2sd", "cvtsd2si",
        "fadd", "faddp", "fsub", "fsubp", "fmul", "fmulp",
        "fdiv", "fdivp", "fdivr", "fdivrp", "fld", "fldz",
        "fst", "fstp", "fild", "fist", "fistp", "fcom",
        "fcomp", "fcompp", "fxch"
    };

    size_t n = sizeof(reservadas) / sizeof(reservadas[0]);
    for (size_t i = 0; i < n; ++i)
    {
        if (strcmp(s, reservadas[i]) == 0)
            return 1;
    }

    return 0;
}

static const char *asm_symbol(const char *s)
{
    static char buffer[8][128];
    static int slot = 0;

    if (!s)
        return s;

    if (!es_registro_x86(s) && !es_palabra_reservada_asm(s))
        return s;

    slot = (slot + 1) % 8;
    snprintf(buffer[slot], sizeof(buffer[slot]), "__mx_%s", s);
    return buffer[slot];
}

static int es_operando_nulo(const char *s)
{
    return !s || strcmp(s, "null") == 0 || strcmp(s, "(null)") == 0;
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

static int es_temporal_prefijado(const char *s, char prefix)
{
    return s && s[0] == prefix && isdigit((unsigned char)s[1]);
}

/**
 * Genera una etiqueta única para saltos y bifurcaciones del IR.
 *

 */
char *new_label(void)
{
    static char label_name_buffer[32];
    sprintf(label_name_buffer, "L%d", next_label_number++);
    return strdup(label_name_buffer);
}

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

    ASTNode *recursive_fn = root_ast_node->hijo_izq;
    while (recursive_fn)
    {
        if (recursive_fn->type == AST_FUNCION && es_funcion_recursiva_ast(recursive_fn))
        {
            char *asm_name = nombre_funcion_asm_recursiva(recursive_fn->hijo_izq->valor.nombre_id);
            emit_quad(IR_FUNCTION_BEGIN, asm_name, NULL, NULL);
            free(asm_name);

            ASTNode *prev_recursive = current_recursive_function_codegen;
            ASTNode *prev_inline = current_inline_function;
            unsigned long long prev_instance = current_inline_instance;
            char *prev_storage = current_return_storage;
            char *prev_end = current_function_end_label;
            enum TipoDato prev_type = current_return_type;
            char *prev_return = current_return_value;
            int prev_return_owned = current_return_value_owned;

            current_recursive_function_codegen = recursive_fn;
            current_inline_function = recursive_fn;
            current_inline_instance = 0;
            current_return_storage = NULL;
            current_function_end_label = new_label();
            current_return_type = recursive_fn->return_type;
            current_return_value = NULL;
            current_return_value_owned = 0;

            generate_code_for_node(recursive_fn->hijo_der);
            emit_quad(IR_LABEL, NULL, NULL, current_function_end_label);
            emit_quad(IR_FUNCTION_END, NULL, NULL, NULL);

            free(current_function_end_label);
            current_function_end_label = prev_end;
            current_return_storage = prev_storage;
            current_return_type = prev_type;
            current_return_value = prev_return;
            current_return_value_owned = prev_return_owned;
            current_inline_function = prev_inline;
            current_inline_instance = prev_instance;
            current_recursive_function_codegen = prev_recursive;
        }
        recursive_fn = recursive_fn->siguiente_hermano;
    }

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
    current_inline_instance = 0;
    next_inline_instance = 0;
    current_inline_function = NULL;
    current_recursive_function_codegen = NULL;
    recursive_layout_count = 0;
    current_asm_recursive_layout = NULL;
    current_return_value = NULL;
    current_return_value_owned = 0;
    current_return_type = TIPO_ERROR;
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

            /* Una cadena Si / Sino Si / Sino se genera completa desde el Si.
             * Por eso, al volver al bloque, saltamos todos los Sino asociados. */
            if (child->type == AST_SI_STMT)
            {
                ASTNode *next = child->siguiente_hermano;
                while (next && next->type == AST_SINO_STMT)
                    next = next->siguiente_hermano;
                child = next;
            }
            else
            {
                child = child->siguiente_hermano;
            }
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
        if (node->hijo_izq)
        {
            char *v = generate_code_for_expression(node->hijo_izq);
            char *return_name = NULL;

            if (current_inline_function)
            {
                if (!current_return_storage)
                    current_return_storage = nombre_local_funcion_actual("return", current_return_type);
                return_name = current_return_storage;
            }
            else
            {
                return_name = "__return";
            }

            emit_quad(IR_ASSIGN, v, NULL, return_name);
            current_return_value = return_name;
            current_return_value_owned = current_inline_function ? 0 : 0;

            if (current_function_end_label)
                emit_quad(IR_GOTO, NULL, NULL, current_function_end_label);
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
