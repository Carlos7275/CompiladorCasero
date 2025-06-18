#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h" 


enum ASTNodeType
{
    // Estructuras de programa
    AST_PROGRAMA,
    AST_LISTA_SENTENCIAS, // Un nodo para agrupar múltiples sentencias secuenciales

    // Declaraciones
    AST_DECLARACION_VAR,  // Nodo para la declaración de variables

    // Sentencias
    AST_ASIGNACION_STMT,  // Sentencia de asignación (ej. a = 5;)
    AST_MOSTRAR_STMT,     // Sentencia Mostrar (ej. Mostrar("Hola");)
    AST_LEER_STMT,
    // Estructuras de control
    AST_BLOQUE,          // Para agrupar sentencias (como las que van entre llaves o después de 'Entonces')
    AST_SI_STMT,         // Sentencia 'Si' (If)
    AST_SINO_STMT,       // Parte 'Sino' (Else) de la sentencia condicional
    AST_MIENTRAS_STMT,   // Sentencia 'Mientras' (While)
    AST_PARA_STMT,       // Sentencia 'Para' (For)
    AST_PARA_PARAMS,
    // Expresiones booleanas (lógicos y comparaciones)
    AST_OR_EXPR,           // Operador 'OR' (Baja precedencia)
    AST_AND_EXPR,          // Operador 'AND' (Media precedencia)
    AST_NOT_EXPR,          // Operador 'NOT' (Alta precedencia - unario)
    AST_IGUAL_EXPR,        // Operador '=='
    AST_DIFERENTE_EXPR,    // Operador '!='
    AST_MENOR_QUE_EXPR,    // Operador '<'
    AST_MAYOR_QUE_EXPR,    // Operador '>'
    AST_MENOR_IGUAL_EXPR,  // Operador '<='
    AST_MAYOR_IGUAL_EXPR,  // Operador '>='

    // Expresiones aritméticas (ordenadas por precedencia, de menor a mayor)
    AST_SUMA_EXPR,
    AST_RESTA_EXPR,
    AST_MULT_EXPR,
    AST_DIV_EXPR,
    AST_NEGACION_UNARIA_EXPR, // Para el operador unario '-' (ej. -5)

    // Nodos terminales (hojas del AST que representan valores concretos)
    AST_IDENTIFICADOR,    // Un nombre de variable (ej. 'x', 'contador')
    AST_LITERAL_ENTERO,   // Un número entero (ej. 10, 1000)
    AST_LITERAL_FLOTANTE, // Un número flotante (ej. 3.14, 0.5)
    AST_LITERAL_CADENA    // Una cadena de texto (ej. "Hola mundo")
};

// Estructura de un nodo del Árbol de Sintaxis Abstracta (AST)
typedef struct ASTNode {
    enum ASTNodeType type; // Tipo de nodo AST

    // Punteros a los nodos hijos (hasta dos hijos directos)
    struct ASTNode *hijo_izq; 
    struct ASTNode *hijo_der;
    // siguiente_hermano se usa para encadenar sentencias o elementos en una lista
    struct ASTNode *siguiente_hermano; 

    // Unión para almacenar el valor del nodo si es un terminal (literal o identificador)
    union {
        char   *nombre_id;     // Para AST_IDENTIFICADOR
        double  valor_numero;  // Para AST_LITERAL_ENTERO y AST_LITERAL_FLOTANTE
        char   *valor_cadena;  // Para AST_LITERAL_CADENA
    } valor;

    // Campo específico para AST_DECLARACION_VAR para almacenar el tipo de dato declarado
    enum TipoDato declared_type_info; 

    // Información de posición en el código fuente para mensajes de error
    int renglon;
    int columna;
} ASTNode;

// ===============================================
//           Prototipos de Funciones del Parser
// ===============================================

// Inicializa el parser, usualmente configurando el puntero al primer token
void iniciarParser();

// Obtiene el token actual sin consumirlo
struct Token* peekToken();

// Consume el token actual y avanza al siguiente
struct Token* consumirToken();

// Verifica si el token actual coincide con el tipo y/o lexema esperado, y lo consume
void match(enum TipoToken tipo_esperado, const char* lexema_esperado);

// Funciones para construir el AST para diferentes componentes de la gramática
ASTNode* parsePrograma();                  // Regla inicial: programa
ASTNode* parseListaSentencias();           // Lista de sentencias
ASTNode* parseSentenciaODeclaracion();     // Una sentencia o una declaración
ASTNode* parseDeclaracion();               // Declaración de variable (ej. Entero x;)
ASTNode* parseAsignacion();                // Asignación (ej. x = 5;)
ASTNode* parseMostrarStmt();               // Sentencia Mostrar (ej. Mostrar(x);)
ASTNode *parseLeerStmt();
// Prototipos para estructuras de control
ASTNode* parseSentenciaCondicional();   // Para 'Si-Sino'
ASTNode* parseSentenciaBucleMientras(); // Para 'Mientras'
ASTNode* parseSentenciaBuclePara();     // Para 'Para'
ASTNode* parseBloqueSentencias();       // Para bloques de sentencias (sin los delimitadores de inicio/fin)
ASTNode * parseSentencia();
// Funciones para parsear expresiones (con precedencia)
ASTNode* parseExpresion();                 // Punto de entrada principal para expresiones (maneja precedencia)
ASTNode* parseExpresionOR();               // Para operador 'OR'
ASTNode* parseExpresionAND();              // Para operador 'AND'
ASTNode* parseExpresionNOT();              // Para operador 'NOT' (unario)
ASTNode* parseExpresionComparacion();      // Para operadores de comparación (==, !=, <, >, <=, >=)
ASTNode* parseExpresionAritmetica();       // Para operadores de suma y resta
ASTNode* parseTermino();                   // Para operadores de multiplicación y división
ASTNode* parseFactor();                    // Para ID, LITERAL, -FACTOR, (EXPRESION)

// Función para crear un nuevo nodo AST
ASTNode* crearNodoAST(enum ASTNodeType type, int renglon, int columna);

// Función para liberar la memoria del AST
void liberarAST(ASTNode* node);

void ImprimirAST(ASTNode * node, int indent_level);

#endif // PARSER_H