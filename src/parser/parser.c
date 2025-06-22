#include "parser.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

extern struct nodo *raiz;

struct nodo *token_actual_parser;

void iniciarParser()
{
    token_actual_parser = raiz;
}

struct Token *peekToken()
{
    if (token_actual_parser != NULL)
    {
        return &(token_actual_parser->info);
    }
    return NULL;
}

struct Token *consumirToken()
{
    if (token_actual_parser != NULL)
    {
        struct Token *token_consumido = &(token_actual_parser->info);

        token_actual_parser = token_actual_parser->der;
        return token_consumido;
    }
    return NULL;
}

void match(enum TipoToken tipo_esperado, const char *lexema_esperado)
{
    struct Token *actual_token = peekToken();

    if (actual_token == NULL)
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba TipoToken %d ('%s'), pero se encontró el fin del archivo (EOF) inesperadamente.\n",
                (token_actual_parser && token_actual_parser->izq) ? token_actual_parser->izq->info.Renglon : 1,
                (token_actual_parser && token_actual_parser->izq) ? token_actual_parser->izq->info.Columna : 1,
                tipo_esperado, lexema_esperado ? lexema_esperado : "Cualquier Lexema");
        exit(EXIT_FAILURE);
    }

    if (actual_token->TipoToken != tipo_esperado)
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba TipoToken %d, se encontró TipoToken %d (Lexema: '%s').\n",
                actual_token->Renglon, actual_token->Columna,
                tipo_esperado, actual_token->TipoToken, actual_token->Lexema);
        exit(EXIT_FAILURE);
    }

    if (lexema_esperado != NULL && strcmp(actual_token->Lexema, lexema_esperado) != 0)
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba el lexema '%s', se encontró '%s'.\n",
                actual_token->Renglon, actual_token->Columna, lexema_esperado, actual_token->Lexema);
        exit(EXIT_FAILURE);
    }

    consumirToken();
}

ASTNode *crearNodoAST(enum ASTNodeType type, int renglon, int columna)
{
    ASTNode *newNode = (ASTNode *)malloc(sizeof(ASTNode));

    if (newNode == NULL)
    {
        perror("Error de asignación de memoria para nodo AST");
        exit(EXIT_FAILURE);
    }

    newNode->type = type;
    newNode->renglon = renglon;
    newNode->columna = columna;

    newNode->hijo_izq = NULL;
    newNode->hijo_der = NULL;
    newNode->siguiente_hermano = NULL;

    newNode->valor.nombre_id = NULL;
    newNode->valor.valor_numero = 0.0;
    newNode->valor.valor_cadena = NULL;
    newNode->valor.valor_booleano = 0;
    newNode->tipoconstante = -1;
    newNode->declared_type_info = -1;
    newNode->ir_result_name = NULL;

    return newNode;
}

void liberar_ast(ASTNode *node)
{
    if (node == NULL)
        return;

    liberar_ast(node->hijo_izq);
    liberar_ast(node->hijo_der);
    liberar_ast(node->siguiente_hermano);

    if (node->type == AST_IDENTIFICADOR && node->valor.nombre_id != NULL)
    {
        free(node->valor.nombre_id);
    }
    else if (node->type == AST_LITERAL_CADENA && node->valor.valor_cadena != NULL)
    {

        free(node->valor.valor_cadena);
    }

    free(node);
}

ASTNode *parsePrograma()
{
    iniciarParser();
    ASTNode *programa_node = crearNodoAST(AST_PROGRAMA, 1, 1);
    programa_node->hijo_izq = parseListaSentencias();
    return programa_node;
}

ASTNode *parseListaSentencias()
{
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    while (peekToken() != NULL)
    {
        struct Token *current_token = peekToken();

        if (current_token->TipoToken == PalRes &&
            strcmp(current_token->Lexema, "Sino") == 0)
        {
            break;
        }

        if (current_token->TipoToken == ESPECIAL && strcmp(current_token->Lexema, "}") == 0)
        {
            break;
        }

        ASTNode *current_stmt = parseSentenciaODeclaracion();
        if (current_stmt == NULL)
        {

            break;
        }
        if (head == NULL)
        {
            head = current_stmt;
            tail = current_stmt;
        }
        else
        {
            tail->siguiente_hermano = current_stmt;
            tail = current_stmt;
        }
    }
    return head;
}

ASTNode *parseSentenciaODeclaracion()
{
    ASTNode *node = NULL;
    struct Token *token_inicio_sentencia = peekToken();

    if (token_inicio_sentencia == NULL)
    {
        return NULL;
    }

    if (token_inicio_sentencia->TipoToken == PalRes)
    {
        if (strcmp(token_inicio_sentencia->Lexema, "Entero") == 0 ||
            strcmp(token_inicio_sentencia->Lexema, "Cadena") == 0 ||
            strcmp(token_inicio_sentencia->Lexema, "Flotante") == 0 ||
            strcmp(token_inicio_sentencia->Lexema, "Caracter") == 0 ||
            strcmp(token_inicio_sentencia->Lexema, "Booleano") == 0)
        {

            node = parseDeclaracion();
            match(ESPECIAL, ";");
        }
        else if (strcmp(token_inicio_sentencia->Lexema, "Mostrar") == 0)
        {
            node = parseMostrarStmt();
            match(ESPECIAL, ";");
        }
        else if (strcmp(token_inicio_sentencia->Lexema, "Constante") == 0)
        {
            node = parseDeclaracionConstante();
            match(ESPECIAL, ";");
        }
        else if (strcmp(token_inicio_sentencia->Lexema, "Continuar") == 0)
        {
            struct Token *continuar_token = consumirToken();
            node = crearNodoAST(AST_CONTINUAR_STMT, continuar_token->Renglon, continuar_token->Columna);
            match(ESPECIAL, ";");
        }
        else if (strcmp(token_inicio_sentencia->Lexema, "Romper") == 0)
        {
            struct Token *romper_token = consumirToken();
            node = crearNodoAST(AST_ROMPER_STMT, romper_token->Renglon, romper_token->Columna);
            match(ESPECIAL, ";");
        }
        else if (strcmp(token_inicio_sentencia->Lexema, "Leer") == 0)
        {
            node = parseLeerStmt();
            match(ESPECIAL, ";");
        }
        else if (strcmp(token_inicio_sentencia->Lexema, "Si") == 0)
        {
            node = parseSentenciaCondicional();
        }
        else if (strcmp(token_inicio_sentencia->Lexema, "Mientras") == 0)
        {
            node = parseSentenciaBucleMientras();
        }
        else if (strcmp(token_inicio_sentencia->Lexema, "Para") == 0)
        {
            node = parseSentenciaBuclePara();
        }
        else
        {
            fprintf(stderr, "Error de sintaxis en (R%d, C%d): Palabra reservada inesperada para el inicio de una sentencia/declaración: '%s'.\n",
                    token_inicio_sentencia->Renglon, token_inicio_sentencia->Columna, token_inicio_sentencia->Lexema);
            exit(EXIT_FAILURE);
        }
    }
    else if (token_inicio_sentencia->TipoToken == ID)
    {
        node = parseAsignacion();
        match(ESPECIAL, ";");
    }
    else
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Token inesperado para el inicio de una sentencia/declaración: '%s'.\n",
                token_inicio_sentencia->Renglon, token_inicio_sentencia->Columna, token_inicio_sentencia->Lexema);
        exit(EXIT_FAILURE);
    }

    return node;
}

ASTNode *parseDeclaracion()
{
    struct Token *tipo_token_consumido = consumirToken();
    int renglon = tipo_token_consumido->Renglon;
    int columna = tipo_token_consumido->Columna;

    struct Token *id_token = peekToken();
    if (id_token == NULL || id_token->TipoToken != ID)
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba un identificador después del tipo de dato.\n",
                id_token ? id_token->Renglon : renglon, id_token ? id_token->Columna : columna);
        exit(EXIT_FAILURE);
    }
    match(ID, NULL);

    ASTNode *id_node = crearNodoAST(AST_IDENTIFICADOR, id_token->Renglon, id_token->Columna);
    id_node->valor.nombre_id = strdup(id_token->Lexema);

    ASTNode *declaracion_node = crearNodoAST(AST_DECLARACION_VAR, renglon, columna);
    declaracion_node->hijo_izq = id_node;

    if (strcmp(tipo_token_consumido->Lexema, "Entero") == 0)
    {
        declaracion_node->declared_type_info = INT;
    }
    else if (strcmp(tipo_token_consumido->Lexema, "Cadena") == 0)
    {
        declaracion_node->declared_type_info = STRING;
    }
    else if (strcmp(tipo_token_consumido->Lexema, "Flotante") == 0)
    {
        declaracion_node->declared_type_info = FLOAT;
    }
    else if (strcmp(tipo_token_consumido->Lexema, "Caracter") == 0)
    {
        declaracion_node->declared_type_info = CHAR;
    }
    else if (strcmp(tipo_token_consumido->Lexema, "Booleano") == 0)
    {
        declaracion_node->declared_type_info = BOOL;
    }
    else
    {
        fprintf(stderr, "Error interno: Tipo de dato '%s' no reconocido en declaración.\n", tipo_token_consumido->Lexema);
        declaracion_node->declared_type_info = DESCONOCIDO;
    }

    struct Token *assign_token = peekToken();
    if (assign_token != NULL && assign_token->TipoToken == OPASIGN && strcmp(assign_token->Lexema, "=") == 0)
    {
        consumirToken();
        ASTNode *expr_node = parseExpresion();
        if (expr_node == NULL)
        {
            fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba una expresión de inicialización.\n",
                    peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
            exit(EXIT_FAILURE);
        }
        declaracion_node->hijo_der = expr_node;
    }
    return declaracion_node;
}

ASTNode *parseDeclaracionConstante()
{
    struct Token *const_token = consumirToken();
    int renglon = const_token->Renglon;
    int columna = const_token->Columna;

    struct Token *tipo_token_consumido = consumirToken();
    if (tipo_token_consumido == NULL || tipo_token_consumido->TipoToken != PalRes ||
        (strcmp(tipo_token_consumido->Lexema, "Entero") != 0 &&
         strcmp(tipo_token_consumido->Lexema, "Cadena") != 0 &&
         strcmp(tipo_token_consumido->Lexema, "Flotante") != 0 &&
         strcmp(tipo_token_consumido->Lexema, "Caracter") != 0 &&
         strcmp(tipo_token_consumido->Lexema, "Booleano") != 0))
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba un tipo de dato (Entero, Cadena, etc.) después de 'Constante'.\n",
                tipo_token_consumido ? tipo_token_consumido->Renglon : renglon, tipo_token_consumido ? tipo_token_consumido->Columna : columna);
        exit(EXIT_FAILURE);
    }

    struct Token *id_token = peekToken();
    if (id_token == NULL || id_token->TipoToken != ID)
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba un identificador para la constante.\n",
                id_token ? id_token->Renglon : renglon, id_token ? id_token->Columna : columna);
        exit(EXIT_FAILURE);
    }
    match(ID, NULL);

    ASTNode *id_node = crearNodoAST(AST_IDENTIFICADOR, id_token->Renglon, id_token->Columna);
    id_node->valor.nombre_id = strdup(id_token->Lexema);
    id_node->tipoconstante = CONSTANTE_SIMBOLICA;

    ASTNode *expr_node = NULL;

    if (peekToken()->TipoToken == OPASIGN)
    {
        consumirToken();
        expr_node = parseExpresion();
        if (expr_node == NULL)
        {
            fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba una expresión de inicialización para la constante después de '='.\n",
                    peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
            exit(EXIT_FAILURE);
        }
    }

    ASTNode *const_decl_node = crearNodoAST(AST_DECLARACION_CONST, renglon, columna);
    const_decl_node->hijo_izq = id_node;
    const_decl_node->hijo_der = expr_node;

    if (strcmp(tipo_token_consumido->Lexema, "Entero") == 0)
    {
        const_decl_node->declared_type_info = INT;
    }
    else if (strcmp(tipo_token_consumido->Lexema, "Cadena") == 0)
    {
        const_decl_node->declared_type_info = STRING;
    }
    else if (strcmp(tipo_token_consumido->Lexema, "Flotante") == 0)
    {
        const_decl_node->declared_type_info = FLOAT;
    }
    else if (strcmp(tipo_token_consumido->Lexema, "Caracter") == 0)
    {
        const_decl_node->declared_type_info = CHAR;
    }
    else if (strcmp(tipo_token_consumido->Lexema, "Booleano") == 0)
    {
        const_decl_node->declared_type_info = BOOL;
    }
    else
    {
        fprintf(stderr, "Error interno: Tipo de dato '%s' no reconocido en declaración de constante.\n", tipo_token_consumido->Lexema);
        const_decl_node->declared_type_info = DESCONOCIDO;
    }

    return const_decl_node;
}
ASTNode *parseAsignacion()
{
    struct Token *id_token = peekToken();
    match(ID, NULL);

    ASTNode *id_node_lhs = crearNodoAST(AST_IDENTIFICADOR, id_token->Renglon, id_token->Columna);
    id_node_lhs->valor.nombre_id = strdup(id_token->Lexema);

    struct Token *next = peekToken();

    if (next == NULL)
    {
        fprintf(stderr, "Error de sintaxis: Se esperaba '=', '++' o '--' después del identificador.\n");
        exit(EXIT_FAILURE);
    }

    if (next->TipoToken == OPASIGN)
    {
        match(OPASIGN, "=");

        ASTNode *expr_node = parseExpresion();

        ASTNode *asignacion_node = crearNodoAST(AST_ASIGNACION_STMT, id_token->Renglon, id_token->Columna);
        asignacion_node->hijo_izq = id_node_lhs;
        asignacion_node->hijo_der = expr_node;
        return asignacion_node;
    }
    else if (strcmp(next->Lexema, "++") == 0 || strcmp(next->Lexema, "--") == 0)
    {
        match(next->TipoToken, next->Lexema);  // Consume ++ o --

        // Crear RHS: x + 1 o x - 1
        ASTNode *id_node_rhs = crearNodoAST(AST_IDENTIFICADOR, id_token->Renglon, id_token->Columna);
        id_node_rhs->valor.nombre_id = strdup(id_token->Lexema);

        ASTNode *literal_one = crearNodoAST(AST_LITERAL_ENTERO, next->Renglon, next->Columna);
        literal_one->valor.valor_numero = 1;
        literal_one->declared_type_info = INT;

        ASTNode *op_node;
        if (strcmp(next->Lexema, "++") == 0)
        {
            op_node = crearNodoAST(AST_SUMA_EXPR, next->Renglon, next->Columna);
        }
        else
        {
            op_node = crearNodoAST(AST_RESTA_EXPR, next->Renglon, next->Columna);
        }

        op_node->hijo_izq = id_node_rhs;
        op_node->hijo_der = literal_one;

        ASTNode *asignacion_node = crearNodoAST(AST_ASIGNACION_STMT, id_token->Renglon, id_token->Columna);
        asignacion_node->hijo_izq = id_node_lhs;
        asignacion_node->hijo_der = op_node;
        return asignacion_node;
    }
    else
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba '=', '++' o '--' después del identificador.\n",
                next->Renglon, next->Columna);
        exit(EXIT_FAILURE);
    }
}


ASTNode *parseUpdateStatement()
{
    ASTNode *stmt_node = NULL;
    struct Token *first_token = peekToken();
    if (first_token == NULL)
    {
        return NULL;
    }

    if (first_token->TipoToken == ESPECIAL && (strcmp(first_token->Lexema, ")") == 0 || strcmp(first_token->Lexema, ";") == 0))
    {
        return NULL;
    }

    if (first_token->TipoToken == ID)
    {

        struct nodo *next_token_node_ll = token_actual_parser->der;
        if (next_token_node_ll != NULL && next_token_node_ll->info.TipoToken == UNARIO)
        {
            if (strcmp(next_token_node_ll->info.Lexema, "++") == 0 || strcmp(next_token_node_ll->info.Lexema, "--") == 0)
            {

                struct Token *id_token = consumirToken();

                ASTNode *id_node_lhs = crearNodoAST(AST_IDENTIFICADOR, id_token->Renglon, id_token->Columna);
                id_node_lhs->valor.nombre_id = strdup(id_token->Lexema);

                ASTNode *id_node_rhs_expr = crearNodoAST(AST_IDENTIFICADOR, id_token->Renglon, id_token->Columna);
                id_node_rhs_expr->valor.nombre_id = strdup(id_token->Lexema);

                struct Token *op_token = consumirToken();

                ASTNode *literal_one = crearNodoAST(AST_LITERAL_ENTERO, op_token->Renglon, op_token->Columna);
                literal_one->valor.valor_numero = 1;
                literal_one->declared_type_info = INT;

                ASTNode *binary_op_expr;
                if (strcmp(op_token->Lexema, "++") == 0)
                {
                    binary_op_expr = crearNodoAST(AST_SUMA_EXPR, op_token->Renglon, op_token->Columna);
                }
                else
                {
                    binary_op_expr = crearNodoAST(AST_RESTA_EXPR, op_token->Renglon, op_token->Columna);
                }
                binary_op_expr->hijo_izq = id_node_rhs_expr;
                binary_op_expr->hijo_der = literal_one;

                stmt_node = crearNodoAST(AST_ASIGNACION_STMT, id_token->Renglon, id_token->Columna);
                stmt_node->hijo_izq = id_node_lhs;
                stmt_node->hijo_der = binary_op_expr;

                return stmt_node;
            }
        }
    }

    stmt_node = parseAsignacion();
    if (stmt_node != NULL)
    {
        return stmt_node;
    }

    fprintf(stderr, "Error de sintaxis (R%d, C%d): Sentencia de actualización no reconocida en 'Para'.\n",
            first_token->Renglon, first_token->Columna);
    exit(EXIT_FAILURE);
    return NULL;
}

ASTNode *parseMostrarStmt()
{
    struct Token *mostrar_token = consumirToken();
    int renglon = mostrar_token->Renglon;
    int columna = mostrar_token->Columna;

    ASTNode *mostrar_node = crearNodoAST(AST_MOSTRAR_STMT, renglon, columna);

    match(ESPECIAL, "(");

    ASTNode *first_arg_expr = parseExpresion();
    if (first_arg_expr == NULL)
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba al menos una expresión como argumento en Mostrar().\n",
                peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
        exit(EXIT_FAILURE);
    }
    mostrar_node->hijo_izq = first_arg_expr;

    ASTNode *current_arg_tail = first_arg_expr;

    while (peekToken() != NULL &&
           peekToken()->TipoToken == ESPECIAL && strcmp(peekToken()->Lexema, ",") == 0)
    {
        consumirToken();

        ASTNode *next_arg_expr = parseExpresion();
        if (next_arg_expr == NULL)
        {
            fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba una expresión después de la coma en Mostrar().\n",
                    peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
            exit(EXIT_FAILURE);
        }

        current_arg_tail->siguiente_hermano = next_arg_expr;
        current_arg_tail = next_arg_expr;
    }

    match(ESPECIAL, ")");

    return mostrar_node;
}
ASTNode *parseLeerStmt()
{
    struct Token *mostrar_token = consumirToken();
    int renglon = mostrar_token->Renglon;
    int columna = mostrar_token->Columna;

    match(ESPECIAL, "(");
    ASTNode *expr_node = parseExpresion();
    if (expr_node == NULL)
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba una expresión dentro de Leer().\n",
                peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
        exit(EXIT_FAILURE);
    }
    match(ESPECIAL, ")");

    ASTNode *mostrar_node = crearNodoAST(AST_LEER_STMT, renglon, columna);
    mostrar_node->hijo_izq = expr_node;
    return mostrar_node;
}

ASTNode *parseBloqueSentencias()
{
    ASTNode *block_node = crearNodoAST(AST_BLOQUE,
                                       peekToken() ? peekToken()->Renglon : 0,
                                       peekToken() ? peekToken()->Columna : 0);
    block_node->hijo_izq = parseListaSentencias();
    return block_node;
}

ASTNode *parseExpresion()
{
    ASTNode *expr = parseExpresionOR();
    return expr;
}

ASTNode *parseExpresionOR()
{
    ASTNode *left_expr = parseExpresionAND();

    struct Token *op_token = peekToken();
    while (op_token != NULL && op_token->TipoToken == OPLOG && strcmp(op_token->Lexema, "||") == 0)
    {
        consumirToken();
        ASTNode *new_expr_node = crearNodoAST(AST_OR_EXPR, op_token->Renglon, op_token->Columna);
        new_expr_node->hijo_izq = left_expr;
        new_expr_node->hijo_der = parseExpresionAND();
        if (new_expr_node->hijo_der == NULL)
        {
            fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión después del operador 'OR'.\n",
                    op_token->Renglon, op_token->Columna);
            exit(EXIT_FAILURE);
        }

        left_expr = new_expr_node;
        op_token = peekToken();
    }
    return left_expr;
}

ASTNode *parseExpresionAND()
{
    ASTNode *left_expr = parseExpresionNOT();

    struct Token *op_token = peekToken();
    while (op_token != NULL && op_token->TipoToken == OPLOG && strcmp(op_token->Lexema, "&&") == 0)
    {
        consumirToken();
        ASTNode *new_expr_node = crearNodoAST(AST_AND_EXPR, op_token->Renglon, op_token->Columna);
        new_expr_node->hijo_izq = left_expr;
        new_expr_node->hijo_der = parseExpresionNOT();
        if (new_expr_node->hijo_der == NULL)
        {
            fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión después del operador 'AND'.\n",
                    op_token->Renglon, op_token->Columna);
            exit(EXIT_FAILURE);
        }

        left_expr = new_expr_node;
        op_token = peekToken();
    }
    return left_expr;
}

ASTNode *parseExpresionNOT()
{
    struct Token *current_token = peekToken();
    if (current_token != NULL && current_token->TipoToken == PalRes && strcmp(current_token->Lexema, "!") == 0)
    {
        consumirToken();
        ASTNode *not_expr_node = crearNodoAST(AST_NOT_EXPR, current_token->Renglon, current_token->Columna);
        not_expr_node->hijo_izq = parseExpresionNOT();

        if (not_expr_node->hijo_izq == NULL)
        {
            fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión después de 'NOT'.\n",
                    current_token->Renglon, current_token->Columna);
            exit(EXIT_FAILURE);
        }
        return not_expr_node;
    }
    return parseExpresionComparacion();
}

ASTNode *parseExpresionComparacion()
{
    ASTNode *left_expr = parseExpresionAritmetica();

    struct Token *op_token = peekToken();
    if (op_token != NULL && op_token->TipoToken == OPCOMP)
    {
        enum ASTNodeType comparison_type = -1;

        if (strcmp(op_token->Lexema, "==") == 0)
        {
            comparison_type = AST_IGUAL_EXPR;
        }
        else if (strcmp(op_token->Lexema, "!=") == 0)
        {
            comparison_type = AST_DIFERENTE_EXPR;
        }
        else if (strcmp(op_token->Lexema, "<") == 0)
        {
            comparison_type = AST_MENOR_QUE_EXPR;
        }
        else if (strcmp(op_token->Lexema, ">") == 0)
        {
            comparison_type = AST_MAYOR_QUE_EXPR;
        }
        else if (strcmp(op_token->Lexema, "<=") == 0)
        {
            comparison_type = AST_MENOR_IGUAL_EXPR;
        }
        else if (strcmp(op_token->Lexema, ">=") == 0)
        {
            comparison_type = AST_MAYOR_IGUAL_EXPR;
        }
        else
        {
            return left_expr;
        }

        consumirToken();

        ASTNode *right_expr = parseExpresionAritmetica();

        ASTNode *newNode = crearNodoAST(comparison_type, op_token->Renglon, op_token->Columna);

        newNode->hijo_izq = left_expr;
        newNode->hijo_der = right_expr;

        return newNode;
    }

    return left_expr;
}

ASTNode *parseExpresionAritmetica()
{
    ASTNode *expr_node = parseTermino();

    struct Token *op_token = peekToken();
    while (op_token != NULL && op_token->TipoToken == OPAR &&
           (strcmp(op_token->Lexema, "+") == 0 || strcmp(op_token->Lexema, "-") == 0))
    {

        consumirToken();

        ASTNode *new_expr_node = NULL;
        if (strcmp(op_token->Lexema, "+") == 0)
        {
            new_expr_node = crearNodoAST(AST_SUMA_EXPR, op_token->Renglon, op_token->Columna);
        }
        else
        {
            new_expr_node = crearNodoAST(AST_RESTA_EXPR, op_token->Renglon, op_token->Columna);
        }

        new_expr_node->hijo_izq = expr_node;
        new_expr_node->hijo_der = parseTermino();
        if (new_expr_node->hijo_der == NULL)
        {
            fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión después de '%s'.\n",
                    op_token->Renglon, op_token->Columna, op_token->Lexema);
            exit(EXIT_FAILURE);
        }

        expr_node = new_expr_node;
        op_token = peekToken();
    }
    return expr_node;
}

ASTNode *parseTermino()
{
    ASTNode *term_node = parseFactor();

    struct Token *op_token = peekToken();
    while (op_token != NULL && op_token->TipoToken == 4 &&
           (strcmp(op_token->Lexema, "*") == 0 || strcmp(op_token->Lexema, "/") == 0 ||
            strcmp(op_token->Lexema, "%") == 0))
    {

        consumirToken();

        ASTNode *new_term_node = NULL;
        if (strcmp(op_token->Lexema, "*") == 0)
        {
            new_term_node = crearNodoAST(AST_MULT_EXPR, op_token->Renglon, op_token->Columna);
        }
        else if (strcmp(op_token->Lexema, "/") == 0)
        {
            new_term_node = crearNodoAST(AST_DIV_EXPR, op_token->Renglon, op_token->Columna);
        }
        else
        {
            new_term_node = crearNodoAST(AST_MOD_EXPR, op_token->Renglon, op_token->Columna);
        }

        new_term_node->hijo_izq = term_node;
        new_term_node->hijo_der = parseFactor();
        if (new_term_node->hijo_der == NULL)
        {
            fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión después de '%s'.\n",
                    op_token->Renglon, op_token->Columna, op_token->Lexema);
            exit(EXIT_FAILURE);
        }

        term_node = new_term_node;
        op_token = peekToken();
    }
    return term_node;
}
ASTNode *parseFactor()
{
    struct Token *current_token = peekToken();
    ASTNode *node = NULL;

    if (current_token == NULL)
    {
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Se esperaba un factor (identificador, numero, cadena, o expresion entre parentesis).\n",
                token_actual_parser && token_actual_parser->izq ? token_actual_parser->izq->info.Renglon : 1,
                token_actual_parser && token_actual_parser->izq ? token_actual_parser->izq->info.Columna : 1);
        exit(EXIT_FAILURE);
    }

    switch (current_token->TipoToken)
    {
    case ID:
        match(ID, NULL);
        node = crearNodoAST(AST_IDENTIFICADOR, current_token->Renglon, current_token->Columna);
        node->valor.nombre_id = strdup(current_token->Lexema);
        break;
    case NUM:
        match(NUM, NULL);
        if (current_token->tipoDato == INT)
        {
            node = crearNodoAST(AST_LITERAL_ENTERO, current_token->Renglon, current_token->Columna);
        }
        else
        {
            node = crearNodoAST(AST_LITERAL_FLOTANTE, current_token->Renglon, current_token->Columna);
        }

        node->valor.valor_numero = strtod(current_token->Lexema, NULL);
        break;
    case CAD:
        match(CAD, NULL);
        node = crearNodoAST(AST_LITERAL_CADENA, current_token->Renglon, current_token->Columna);

        node->valor.valor_cadena = strdup(current_token->Lexema);
        break;
    case ESPECIAL:
        if (strcmp(current_token->Lexema, "-") == 0)
        {
            match(ESPECIAL, "-");
            ASTNode *neg_expr = parseFactor();
            if (neg_expr == NULL)
            {
                fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión después del operador unario '-'.\n",
                        current_token->Renglon, current_token->Columna);
                exit(EXIT_FAILURE);
            }
            node = crearNodoAST(AST_NEGACION_UNARIA_EXPR, current_token->Renglon, current_token->Columna);
            node->hijo_izq = neg_expr;
        }
        else if (strcmp(current_token->Lexema, "(") == 0)
        {
            match(ESPECIAL, "(");
            node = parseExpresion();
            if (node == NULL)
            {
                fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión dentro de los paréntesis.\n",
                        current_token->Renglon, current_token->Columna);
                exit(EXIT_FAILURE);
            }
            match(ESPECIAL, ")");
        }
        else
        {
            fprintf(stderr, "Error de sintaxis en (R%d, C%d): Token especial inesperado en el factor: '%s'.\n",
                    current_token->Renglon, current_token->Columna, current_token->Lexema);
            exit(EXIT_FAILURE);
        }
        break;
    case PalRes:
        if (current_token->tipoDato == BOOL)
        {
            node = crearNodoAST(AST_LITERAL_BOOLEANO, current_token->Renglon, current_token->Columna);
            if (strcmp(current_token->Lexema, "Verdadero") == 0)
            {
                node->valor.valor_booleano = 1;
            }
            else
            {
                node->valor.valor_booleano = 0;
            }
            node->resolved_type = BOOL;

            consumirToken();
            break;
            ;
        }
        else
        {

            fprintf(stderr, "Error de sintaxis en (R%d, C%d): Palabra reservada inesperada en el factor: '%s'.\n",
                    current_token->Renglon, current_token->Columna, current_token->Lexema);
            exit(EXIT_FAILURE);
        }

        break;

    default:
        fprintf(stderr, "Error de sintaxis en (R%d, C%d): Tipo de token inesperado en el factor: %d (Lexema: '%s').\n",
                current_token->Renglon, current_token->Columna, current_token->TipoToken, current_token->Lexema);
        exit(EXIT_FAILURE);
    }
    return node;
}

ASTNode *parseSentenciaCondicional()
{
    struct Token *si_token = consumirToken();
    int renglon = si_token->Renglon;
    int columna = si_token->Columna;

    ASTNode *if_node = crearNodoAST(AST_SI_STMT, renglon, columna);

    match(ESPECIAL, "(");
    ASTNode *condition_expr = parseExpresion();
    if (condition_expr == NULL)
    {
        fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión para la condición 'Si'.\n",
                peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
        exit(EXIT_FAILURE);
    }
    match(ESPECIAL, ")");

    match(ESPECIAL, "{");
    ASTNode *then_block = parseBloqueSentencias();
    if (then_block == NULL)
    {
        fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba un bloque de sentencias para el 'Si'.\n",
                peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
        exit(EXIT_FAILURE);
    }
    match(ESPECIAL, "}");

    if_node->hijo_izq = condition_expr;
    if_node->hijo_der = then_block;

    ASTNode *current_else_chain_tail = if_node;

    struct Token *peek_next_keyword = peekToken();
    while (peek_next_keyword != NULL && strcmp(peek_next_keyword->Lexema, "Sino") == 0)
    {
        consumirToken();

        peek_next_keyword = peekToken();

        if (peek_next_keyword != NULL && strcmp(peek_next_keyword->Lexema, "Si") == 0)
        {
            consumirToken();

            ASTNode *else_if_wrapper_node = crearNodoAST(AST_SINO_STMT, peek_next_keyword->Renglon, peek_next_keyword->Columna);
            current_else_chain_tail->siguiente_hermano = else_if_wrapper_node;
            current_else_chain_tail = else_if_wrapper_node;

            ASTNode *nested_if_node = crearNodoAST(AST_SI_STMT, peek_next_keyword->Renglon, peek_next_keyword->Columna);
            else_if_wrapper_node->hijo_izq = nested_if_node;
            else_if_wrapper_node->hijo_der = NULL;

            match(ESPECIAL, "(");
            ASTNode *else_if_condition = parseExpresion();
            if (else_if_condition == NULL)
            {
                fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión para la condición 'Sino Si'.\n",
                        peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
                exit(EXIT_FAILURE);
            }
            match(ESPECIAL, ")");

            match(ESPECIAL, "{");
            ASTNode *else_if_block = parseBloqueSentencias();
            if (else_if_block == NULL)
            {
                fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba un bloque de sentencias para el 'Sino Si'.\n",
                        peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
                exit(EXIT_FAILURE);
            }
            match(ESPECIAL, "}");

            nested_if_node->hijo_izq = else_if_condition;
            nested_if_node->hijo_der = else_if_block;

            peek_next_keyword = peekToken();
        }
        else
        {

            ASTNode *else_node = crearNodoAST(AST_SINO_STMT, peek_next_keyword->Renglon, peek_next_keyword->Columna);
            current_else_chain_tail->siguiente_hermano = else_node;
            current_else_chain_tail = else_node;

            match(ESPECIAL, "{");
            ASTNode *else_block = parseBloqueSentencias();
            if (else_block == NULL)
            {
                fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba un bloque de sentencias después de 'Sino'.\n",
                        peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
                exit(EXIT_FAILURE);
            }
            match(ESPECIAL, "}");

            else_node->hijo_izq = else_block;
            else_node->hijo_der = NULL;

            break;
        }
    }

    return if_node;
}
ASTNode *parseSentenciaBucleMientras()
{
    struct Token *mientras_token = consumirToken();
    int renglon = mientras_token->Renglon;
    int columna = mientras_token->Columna;

    match(ESPECIAL, "(");
    ASTNode *condition_expr = parseExpresion();
    if (condition_expr == NULL)
    {
        fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una expresión booleana para la condición 'Mientras'.\n",
                peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
        exit(EXIT_FAILURE);
    }
    match(ESPECIAL, ")");

    match(ESPECIAL, "{");

    ASTNode *body_block = parseBloqueSentencias();
    if (body_block == NULL)
    {
        fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba un bloque de sentencias para el cuerpo del bucle 'Mientras'.\n",
                peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
        exit(EXIT_FAILURE);
    }

    match(ESPECIAL, "}");

    ASTNode *mientras_node = crearNodoAST(AST_MIENTRAS_STMT, renglon, columna);
    mientras_node->hijo_izq = condition_expr;
    mientras_node->hijo_der = body_block;

    return mientras_node;
}
ASTNode *parseSentenciaBuclePara()
{
    struct Token *para_token = consumirToken();
    int renglon = para_token->Renglon;
    int columna = para_token->Columna;

    ASTNode *for_node = crearNodoAST(AST_PARA_STMT, renglon, columna);

    match(ESPECIAL, "(");

    ASTNode *init_stmt = NULL;
    struct Token *peek_init = peekToken();
    if (peek_init != NULL && !(peek_init->TipoToken == ESPECIAL && strcmp(peek_init->Lexema, ";") == 0))
    {
        if (peek_init->TipoToken == PalRes && (strcmp(peek_init->Lexema, "Entero") == 0 ||
                                               strcmp(peek_init->Lexema, "Flotante") == 0 ||
                                               strcmp(peek_init->Lexema, "Cadena") == 0 ||
                                               strcmp(peek_init->Lexema, "Caracter") == 0 ||
                                               strcmp(peek_init->Lexema, "Booleano") == 0))
        {
            init_stmt = parseDeclaracion();
        }
        else if (peek_init->TipoToken == ID)
        {
            init_stmt = parseAsignacion();
        }
        else
        {
            fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba una declaración o asignación en la inicialización de 'Para'.\n",
                    peek_init->Renglon, peek_init->Columna);
            exit(EXIT_FAILURE);
        }
    }
    match(ESPECIAL, ";");

    ASTNode *condition_expr = NULL;
    struct Token *peek_cond = peekToken();
    if (peek_cond != NULL && !(peek_cond->TipoToken == ESPECIAL && strcmp(peek_cond->Lexema, ";") == 0))
    {
        condition_expr = parseExpresion();
    }
    match(ESPECIAL, ";");

    ASTNode *increment_stmt = NULL;
    struct Token *peek_inc = peekToken();
    if (peek_inc != NULL && !(peek_inc->TipoToken == ESPECIAL && strcmp(peek_inc->Lexema, ")") == 0))
    {
        increment_stmt = parseUpdateStatement();
    }

    match(ESPECIAL, ")");

    ASTNode *for_params_node = crearNodoAST(AST_PARA_PARAMS, renglon, columna);

    ASTNode *current_param_link = NULL;

    if (init_stmt != NULL)
    {
        for_params_node->hijo_izq = init_stmt;
        current_param_link = init_stmt;
    }

    if (condition_expr != NULL)
    {
        if (current_param_link != NULL)
        {
            current_param_link->siguiente_hermano = condition_expr;
        }
        else
        {
            for_params_node->hijo_izq = condition_expr;
        }
        current_param_link = condition_expr;
    }

    if (increment_stmt != NULL)
    {
        if (current_param_link != NULL)
        {
            current_param_link->siguiente_hermano = increment_stmt;
        }
        else
        {
            for_params_node->hijo_izq = increment_stmt;
        }
    }

    match(ESPECIAL, "{");
    ASTNode *loop_block = parseBloqueSentencias();
    if (loop_block == NULL)
    {
        fprintf(stderr, "Error de sintaxis (R%d, C%d): Se esperaba un bloque de sentencias para el bucle 'Para'.\n",
                peekToken() ? peekToken()->Renglon : renglon, peekToken() ? peekToken()->Columna : columna);
        exit(EXIT_FAILURE);
    }
    match(ESPECIAL, "}");

    for_node->hijo_izq = for_params_node;
    for_node->hijo_der = loop_block;

    return for_node;
}
const char *ASTNodeTypeNames[] = {
    "AST_PROGRAMA",
    "AST_LISTA_SENTENCIAS",
    "AST_DECLARACION_VAR",
    "AST_DECLARACION_CONSTANTE",
    "AST_ASIGNACION_STMT",
    "AST_MOSTRAR_STMT",
    "AST_LEER_STMT",
    "AST_BLOQUE",
    "AST_SI_STMT",
    "AST_SINO_STMT",
    "AST_MIENTRAS_STMT",
    "AST_PARA_STMT",
    "AST_FOR_PARAMS",
    "AST_CONTINUAR_STMT",
    "AST_ROMPER_STMT",
    "AST_OR_EXPR",
    "AST_AND_EXPR",
    "AST_NOT_EXPR",
    "AST_IGUAL_EXPR",
    "AST_DISTINTO_EXPR",
    "AST_MENOR_EXPR",
    "AST_MAYOR_EXPR",
    "AST_MENOR_IGUAL_EXPR",
    "AST_MAYOR_IGUAL_EXPR",
    "AST_SUMA_EXPR",
    "AST_RESTA_EXPR",
    "AST_MULT_EXPR",
    "AST_DIV_EXPR",
    "AST_MOD_EXPR",
    "AST_NEGACION_UNARIA_EXPR",
    "AST_IDENTIFICADOR",
    "AST_LITERAL_ENTERO",
    "AST_LITERAL_FLOTANTE",
    "AST_LITERAL_CADENA",
    "AST_LITERAL_BOOLEANO"};

const char *DataTypeNames[] = {
    "Entero", "Cadena", "Caracter", "Flotante", "Booleano", "Otro"};

void imprimir_ast(ASTNode *node, int indent_level)
{
    if (node == NULL)
        return;

    for (int i = 0; i < indent_level; i++)
        printf("  ");

    if (node->type >= 0 && node->type < sizeof(ASTNodeTypeNames) / sizeof(ASTNodeTypeNames[0]))
    {
        printf("%s", ASTNodeTypeNames[node->type]);
    }
    else
    {
        printf("Tipo: %d", node->type);
    }

    switch (node->type)
    {
    case AST_DECLARACION_CONST:
    case AST_DECLARACION_VAR:
        if (node->declared_type_info >= 0 && node->declared_type_info < sizeof(DataTypeNames) / sizeof(DataTypeNames[0]))
        {
            printf(" (%s)", DataTypeNames[node->declared_type_info]);
        }
        else
        {
            printf(" (Tipo de dato desconocido: %d)", node->declared_type_info);
        }
        break;

    case AST_IDENTIFICADOR:
        printf(" (ID: %s)", node->valor.nombre_id);
        break;
    case AST_LITERAL_ENTERO:
        printf(" (Valor: %lld)", (long long)node->valor.valor_numero);
        break;
    case AST_LITERAL_FLOTANTE:
        printf(" (Valor: %.6f)", node->valor.valor_numero);
        break;
    case AST_LITERAL_CADENA:
        printf(" (Cadena: \"%s\")", node->valor.valor_cadena);
        break;

    case AST_LITERAL_BOOLEANO:
        printf(" (Cadena: \"%d\")", node->valor.valor_booleano);
        break;
    default:
        break;
    }
    printf(" (R%d, C%d)\n", node->renglon, node->columna);

    imprimir_ast(node->hijo_izq, indent_level + 1);
    imprimir_ast(node->hijo_der, indent_level + 1);
    imprimir_ast(node->siguiente_hermano, indent_level);
}