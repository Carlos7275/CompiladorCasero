#ifndef LEXER_H
#define LEXER_H
#include <stdio.h>

void EsID(FILE *archivo, char car, int Col, int Renglon);
int EsPalabraReservada(const char *car);
void EsSimbolo(FILE *archivo, char car_inicial, int Col, int Renglon);
void EsCadena(FILE *archivo, const char car, int Col, int Renglon);
void EsNum(FILE *archivo, const char car, int Col, int Renglon);

#endif