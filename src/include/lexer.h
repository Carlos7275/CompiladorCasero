#ifndef LEXER_H
#define LEXER_H
#include <stdio.h>
#include "types.h"
void EsID(FILE *archivo, char car, int Col, int Renglon);
enum TipoDato EsPalabraReservadaConTipo(const char *lexema, enum TipoToken *out_tipo_token);
void EsSimbolo(FILE *archivo, char car_inicial, int Col, int Renglon);
void EsCadena(FILE *archivo, const char car, int Col, int Renglon);
void EsNum(FILE *archivo, const char car, int Col, int Renglon);

#endif