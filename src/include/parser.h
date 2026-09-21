#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
    AST_LITERAL_BOOLEANO,

    AST_DECLARACION_TIPO,
    AST_FUNCION,
    AST_LLAMADA,
    AST_RETORNAR_STMT,
    AST_IMPORT,

    AST_ACCESO_ARRAY,
    AST_TERNARIO_EXPR
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
        int64_t valor_entero;
        char *valor_cadena;
        unsigned int valor_booleano;
    } valor;

    enum TipoDato declared_type_info;
    enum TipoDato resolved_type;

    enum ASTConstant tipoconstante;

    int renglon;
    int columna;

    char *ir_result_name;

    struct ASTNode *parametros;

    enum TipoDato return_type;

    char *tipo_nombre;

} ASTNode;

void iniciarParser(void);

struct Token *peekToken(void);

struct Token *consumirToken(void);

void match(enum TipoToken tipo_esperado, const char *lexema_esperado);

ASTNode *parsePrograma(void);
ASTNode *parseListaSentencias(void);
ASTNode *parseSentenciaODeclaracion(void);

ASTNode *parseDeclaracion(void);
ASTNode *parseDeclaracionConstante(void);
ASTNode *parseAsignacion(void);

ASTNode *parseMostrarStmt(void);
ASTNode *parseLeerStmt(void);

ASTNode *parseSentenciaCondicional(void);
ASTNode *parseSentenciaBucleMientras(void);
ASTNode *parseSentenciaBuclePara(void);

ASTNode *parseBloqueSentencias(void);
ASTNode *parseSentencia(void);

ASTNode *parseExpresion(void);
ASTNode *parseExpresionOR(void);
ASTNode *parseExpresionAND(void);
ASTNode *parseExpresionNOT(void);
ASTNode *parseExpresionComparacion(void);
ASTNode *parseExpresionAritmetica(void);
ASTNode *parseTermino(void);
ASTNode *parseFactor(void);

ASTNode *crearNodoAST(
    enum ASTNodeType type,
    int renglon,
    int columna
);

void liberar_ast(ASTNode *node);

void imprimir_ast(
    ASTNode *node,
    int indent_level
);

#endif