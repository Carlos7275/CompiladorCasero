#ifndef ANALIZADOR_SEMANTICO_H
#define ANALIZADOR_SEMANTICO_H

#include "parser.h"
#include "types.h"
#include "symbols.h"

TablaSimbolos *realizar_analisis_semantico(ASTNode *raiz_ast);
void reportar_error_semantico(int renglon, int columna, const char *formato, ...);
void visit_ast_semantic(ASTNode *node);
void imprimir_errores_semanticos();
void verificar_asignacion(int renglon, int columna, enum TipoDato tipo_destino, enum TipoDato TipoOrigen);
enum TipoDato verificar_expresion_aritmetica(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2);
enum TipoDato verificar_expresion_comparacion(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2);
enum TipoDato verificar_expresion_logica(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2);
enum TipoDato verificar_negacion_unaria(int renglon, int columna, enum TipoDato tipo);
enum TipoDato verificar_negacion_logica(int renglon, int columna, enum TipoDato tipo);
void inicializarTablaSimbolos();
const char *tipoDatoToString(enum TipoDato tipo);

#endif // ANALIZADOR_SEMANTICO_H