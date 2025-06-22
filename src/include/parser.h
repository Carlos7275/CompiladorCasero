#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

enum ASTNodeType
{

    AST_PROGRAMA,
    AST_LISTA_SENTENCIAS,

    AST_DECLARACION_VAR,
    AST_DECLARACION_CONST,

    AST_ASIGNACION_STMT,
    AST_MOSTRAR_STMT,
    AST_LEER_STMT,

    AST_BLOQUE,
    AST_SI_STMT,
    AST_SINO_STMT,
    AST_MIENTRAS_STMT,
    AST_PARA_STMT,
    AST_PARA_PARAMS,

    AST_CONTINUAR_STMT,
    AST_ROMPER_STMT,

    AST_OR_EXPR,
    AST_AND_EXPR,
    AST_NOT_EXPR,
    AST_IGUAL_EXPR,
    AST_DIFERENTE_EXPR,
    AST_MENOR_QUE_EXPR,
    AST_MAYOR_QUE_EXPR,
    AST_MENOR_IGUAL_EXPR,
    AST_MAYOR_IGUAL_EXPR,

    AST_SUMA_EXPR,
    AST_RESTA_EXPR,
    AST_MULT_EXPR,
    AST_DIV_EXPR,
    AST_MOD_EXPR,
    AST_NEGACION_UNARIA_EXPR,

    AST_IDENTIFICADOR,
    AST_LITERAL_ENTERO,
    AST_LITERAL_FLOTANTE,
    AST_LITERAL_CADENA,
    AST_LITERAL_BOOLEANO
};

enum ASTConstant
{
    NO_CONSTANTE = 0,
    CONSTANTE_LITERAL = 1,
    CONSTANTE_SIMBOLICA = 2
};

typedef struct ASTNode
{
    enum ASTNodeType type;

    struct ASTNode *hijo_izq;
    struct ASTNode *hijo_der;
    struct ASTNode *siguiente_hermano;
    union
    {
        char *nombre_id;
        double valor_numero;
        char *valor_cadena;
        unsigned int valor_booleano;
    } valor;

    enum TipoDato declared_type_info;
    enum TipoDato resolved_type;

    enum ASTConstant tipoconstante;
    int renglon;
    int columna;
    char *ir_result_name;

} ASTNode;

void iniciarParser();

struct Token *peekToken();

struct Token *consumirToken();

void match(enum TipoToken tipo_esperado, const char *lexema_esperado);

ASTNode *parsePrograma();
ASTNode *parseListaSentencias();
ASTNode *parseSentenciaODeclaracion();
ASTNode *parseDeclaracion();
ASTNode *parseDeclaracionConstante();
ASTNode *parseAsignacion();
ASTNode *parseMostrarStmt();
ASTNode *parseLeerStmt();
ASTNode *parseSentenciaCondicional();
ASTNode *parseSentenciaBucleMientras();
ASTNode *parseSentenciaBuclePara();
ASTNode *parseBloqueSentencias();
ASTNode *parseSentencia();
ASTNode *parseExpresion();
ASTNode *parseExpresionOR();
ASTNode *parseExpresionAND();
ASTNode *parseExpresionNOT();
ASTNode *parseExpresionComparacion();
ASTNode *parseExpresionAritmetica();
ASTNode *parseTermino();
ASTNode *parseFactor();

ASTNode *crearNodoAST(enum ASTNodeType type, int renglon, int columna);
void liberar_ast(ASTNode *node);
void imprimir_ast(ASTNode *node, int indent_level);
#endif