#include "lexer.h"
#include "types.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

const char palabReserv[][100] = {"Leer", "Mostrar", "Mientras", "Continuar", "Romper", "Para", "Si", "Sino", "Encambio", "Cadena", "Entero", "Flotante", "Caracter", "Booleano", "Paso", "Verdadero", "Falso", "Constante"};

void EsID(FILE *archivo, char car_inicial, int Col, int Renglon)
{
    char lexema[100];
    int i = 0;
    lexema[i++] = car_inicial; // El primer carácter ya fue validado como 'alpha' por AnalizarArchivo

    int car_siguiente; // Usamos un nombre diferente para evitar confusiones con 'car' inicial

    while ((car_siguiente = fgetc(archivo)) != EOF && i < sizeof(lexema) - 1)
    {
        if (isalnum(car_siguiente) || car_siguiente == '_')
        {
            lexema[i++] = car_siguiente;
        }
        else
        {
            ungetc(car_siguiente, archivo);
            break;
        }
    }
    lexema[i] = '\0';

    enum TipoToken token_type = ID;
    enum TipoDato data_type = OTRO;

    data_type = EsPalabraReservadaConTipo(lexema, &token_type);
    generarToken(token_type, lexema, data_type, Col, Renglon);
}

void EsCadena(FILE *archivo, char car, int Col, int Renglon)
{
    char lexema[1000];
    int i = 0;

    // Guardamos la primera comilla
    lexema[i++] = car;

    int FlagError = 1; // Bandera para detectar cadena no cerrada

    while ((car = fgetc(archivo)) != EOF && i < (int)(sizeof(lexema) - 1))
    {
        if (isascii(car) && car != '"')
        {
            lexema[i++] = car;
        }
        else if (car == '"')
        {
            lexema[i++] = '"';
            FlagError = 0;
            break;
        }
    }

    lexema[i] = '\0'; // Terminar cadena

    if (FlagError == 0)
    {
        generarToken(CAD, lexema, STRING, Col, Renglon);
    }
    else
    {
        printf("Error en la cadena: falta cerrar la comilla doble (\")\n");
        exit(1);
    }
}

void EsNum(FILE *arch, char car_inicial, int Col, int Renglon)
{
    char lexema[100] = {0};
    int i = 0;
    int cantPuntos = 0;

    // Guardamos el primer caracter
    lexema[i++] = car_inicial;

    char next_char; // Para el carácter leído del flujo

    while (i < (int)(sizeof(lexema) - 1))
    {
        next_char = fgetc(arch); // Usar fgetc o tu readNextChar() aquí
        if (next_char == EOF)
        {
            break; // Fin de archivo, salir
        }

        if (isdigit(next_char))
        {
            lexema[i++] = next_char;
        }
        else if (next_char == '.' && cantPuntos == 0)
        {
            lexema[i++] = next_char;
            cantPuntos++;
        }
        else if (next_char == '.' && cantPuntos == 1)
        {
            fprintf(stderr, "Error (R%d, C%d): un número no puede tener más de un punto decimal\n", Renglon, Col);
            exit(EXIT_FAILURE);
        }
        // Caracteres que DEBEN terminar el número pero NO son parte de él
        else if (isspace(next_char) || next_char == ';' ||
                 next_char == '+' || next_char == '-' ||
                 next_char == '*' || next_char == '/' || next_char == '%' ||
                 next_char == '=' || next_char == '<' || next_char == '>' ||
                 next_char == '!' || next_char == '&' || next_char == '|' ||
                 next_char == '(' || next_char == ')' ||
                 next_char == '[' || next_char == ']' ||
                 next_char == '{' || next_char == '}')
        {
            ungetc(next_char, arch); // Devolver el carácter al flujo
            break;                   // Terminar el procesamiento del número
        }
        else
        {
            fprintf(stderr, "Error (R%d, C%d): carácter inválido '%c' en número\n", Renglon, Col, next_char);
            exit(EXIT_FAILURE);
        }
    }

    lexema[i] = '\0';

    int tipoDato = (cantPuntos == 0) ? INT : FLOAT;
    generarToken(NUM, lexema, tipoDato, Col, Renglon);
}

enum TipoDato EsPalabraReservadaConTipo(const char *lexema, enum TipoToken *out_tipo_token)
{
    const int longitud = sizeof(palabReserv) / sizeof(palabReserv[0]);
    for (int i = 0; i < longitud; i++)
    {
        if (strcmp(lexema, palabReserv[i]) == 0)
        {
            *out_tipo_token = PalRes;

            if (strcmp(lexema, "Entero") == 0)
                return INT;
            if (strcmp(lexema, "Cadena") == 0)
                return STRING;
            if (strcmp(lexema, "Caracter") == 0)
                return CHAR;
            if (strcmp(lexema, "Flotante") == 0)
                return FLOAT;
            if (strcmp(lexema, "Verdadero") == 0 || strcmp(lexema, "Falso") == 0)
            {

                return BOOL;
            }

            return TIPO_VOID;
        }
    }
    return OTRO;
}

void EsSimbolo(FILE *archivo, char car_inicial, int Col, int Renglon)
{
    char lexema[100]; // Usamos un array más grande por si es un símbolo doble
    int i = 0;
    lexema[i++] = car_inicial; // El primer carácter ya se añade al lexema

    int next_char_val; // Para mirar el siguiente carácter

    switch (car_inicial) // Usamos car_inicial porque es el que ya tenemos
    {
    case '+':
        next_char_val = fgetc(archivo); // Mira el siguiente carácter
        if (next_char_val == '+')
        {
            lexema[i++] = '+'; // Si es otro '+', lo añade al lexema
            lexema[i] = '\0';
            generarToken(UNARIO, lexema, OTRO, Col, Renglon); // Es '++'
            Col++;                                            // Consumimos un carácter adicional, avanzamos la columna global
        }
        else
        {
            ungetc(next_char_val, archivo);                 // Si no es '+', lo devuelve al stream
            lexema[i] = '\0';                               // Es solo '+'
            generarToken(OPAR, lexema, OTRO, Col, Renglon); // Operador Aritmético simple
        }
        break;

    case '-':
        next_char_val = fgetc(archivo); // Mira el siguiente carácter
        if (next_char_val == '-')
        {
            lexema[i++] = '-'; // Si es otro '-', lo añade al lexema
            lexema[i] = '\0';
            generarToken(OPAR, lexema, OTRO, Col, Renglon); // Es '--'
            Col++;                                          // Consumimos un carácter adicional, avanzamos la columna global
        }
        else
        {
            ungetc(next_char_val, archivo);                 // Si no es '-', lo devuelve al stream
            lexema[i] = '\0';                               // Es solo '-'
            generarToken(OPAR, lexema, OTRO, Col, Renglon); // Operador Aritmético simple
        }
        break;

    case '*':
    case '/':
        lexema[i] = '\0';                               // Aseguramos que sea nulo-terminado después del único carácter
        generarToken(OPAR, lexema, OTRO, Col, Renglon); // Operadores Aritméticos simples
        break;
    case '%':
        lexema[i] = '\0';
        generarToken(OPAR, lexema, OTRO, Col, Renglon);
        break;
    case '=':
        next_char_val = fgetc(archivo); // Miramos el siguiente carácter
        if (next_char_val == '=')
        {
            lexema[i++] = '='; // Si es otro '=', lo añadimos
            lexema[i] = '\0';
            generarToken(OPCOMP, lexema, OTRO, Col, Renglon); // Es '=='
            Col++;                                            // Consumimos un carácter adicional
        }
        else
        {
            ungetc(next_char_val, archivo);                    // Si no es '=', lo devolvemos
            lexema[i] = '\0';                                  // Es solo '='
            generarToken(OPASIGN, lexema, OTRO, Col, Renglon); // Operador de Asignación
        }
        break;

    case '!':
        next_char_val = fgetc(archivo);
        if (next_char_val == '=')
        {
            lexema[i++] = '=';
            lexema[i] = '\0';
            generarToken(OPCOMP, lexema, OTRO, Col, Renglon); // Es '!='
            Col++;                                            // Consumimos un carácter adicional
        }
        else
        {
            ungetc(next_char_val, archivo);
            lexema[i] = '\0';
            generarToken(OPLOG, lexema, OTRO, Col, Renglon); // Es '!' (NOT lógico)
        }
        break;

    case '<':
        next_char_val = fgetc(archivo);
        if (next_char_val == '=')
        {
            lexema[i++] = '=';
            lexema[i] = '\0';
            generarToken(OPCOMP, lexema, OTRO, Col, Renglon); // Es '<='
            Col++;                                            // Consumimos un carácter adicional
        }
        else
        {
            ungetc(next_char_val, archivo);
            lexema[i] = '\0';
            generarToken(OPCOMP, lexema, OTRO, Col, Renglon); // Es '<'
        }
        break;

    case '>':
        next_char_val = fgetc(archivo);
        if (next_char_val == '=')
        {
            lexema[i++] = '=';
            lexema[i] = '\0';
            generarToken(OPCOMP, lexema, OTRO, Col, Renglon); // Es '>='
            Col++;                                            // Consumimos un carácter adicional
        }
        else
        {
            ungetc(next_char_val, archivo);
            lexema[i] = '\0';
            generarToken(OPCOMP, lexema, OTRO, Col, Renglon); // Es '>'
        }
        break;

    case '&':
        next_char_val = fgetc(archivo);
        if (next_char_val == '&')
        {
            lexema[i++] = '&';
            lexema[i] = '\0';
            generarToken(OPLOG, lexema, OTRO, Col, Renglon); // Es '&&'
            Col++;                                           // Consumimos un carácter adicional
        }
        else
        {
            ungetc(next_char_val, archivo);
            fprintf(stderr, "Error (R%d, C%d): Símbolo inesperado '%c'. Se esperaba '&&'.\n", Renglon, Col, car_inicial);
            exit(EXIT_FAILURE);
        }
        break;

    case '|':
        next_char_val = fgetc(archivo);
        if (next_char_val == '|')
        {
            lexema[i++] = '|';
            lexema[i] = '\0';
            generarToken(OPLOG, lexema, OTRO, Col, Renglon); // Es '||'
            Col++;                                           // Consumimos un carácter adicional
        }
        else
        {
            ungetc(next_char_val, archivo);
            fprintf(stderr, "Error (R%d, C%d): Símbolo inesperado '%c'. Se esperaba '||'.\n", Renglon, Col, car_inicial);
            exit(EXIT_FAILURE);
        }
        break;

    case '[':
    case ']':
    case '{':
    case '}':
    case '(':
    case ')':
    case ';':
    case ':':                                               // El caracter ':' se maneja aquí como símbolo especial
    case ',':                                               // La coma también es un símbolo especial
        lexema[i] = '\0';                                   // Aseguramos nulo-terminación
        generarToken(ESPECIAL, lexema, OTRO, Col, Renglon); // Símbolos Especiales
        break;

    case '"': // Las comillas deben ser manejadas por EsCadena, EsSimbolo no debería llegar aquí
        fprintf(stderr, "Error (R%d, C%d): Comilla doble inesperada aquí. Posible error en flujo de lexer.\n", Renglon, Col);
        exit(EXIT_FAILURE);
        break;

    default:
        lexema[i] = '\0';                                      // Aseguramos nulo-terminación
        generarToken(DESCONOCIDO, lexema, OTRO, Col, Renglon); // Carácter desconocido
        break;
    }
}
