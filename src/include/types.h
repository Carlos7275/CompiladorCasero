#ifndef TYPES_H
#define TYPES_H

enum TipoToken
{
    PalRes,     // 0
    ID,         // 1
    NUM,        // 2
    SIM,        // 3
    OPAR,       // 4
    OPCOMP,     // 5
    OPASIGN,    // 6
    OPLOG,      // 7
    ESPECIAL,   // 8
    UNARIO,
    CAD,        // 9
    SEPARADOR,  //10
    DESCONOCIDO // 11
};

enum TipoDato
{
    INT,    // 0
    STRING, // 1
    CHAR,   // 2
    FLOAT,  // 3
    BOOL,
    TIPO_VOID,
    TIPO_ERROR,
    OTRO    // 4
};

struct Token
{
    enum TipoToken TipoToken;
    char *Lexema;
    int Renglon, Columna;
    union
    {
        double valor;
    };
    enum TipoDato tipoDato;
};

// Estructura del nodo
struct nodo
{
    struct Token info;
    struct nodo *izq;
    struct nodo *der;
};

struct ErrorSemantico
{
    char mensaje[256];
    int renglon;
    int columna;
    struct ErrorSemantico *sig;
};



void Insertar(struct Token token);
void generarToken(int tipoToken, const char *lexema, int tipoDato, int Col, int Renglon);
void Imprimir(struct nodo *Nodo);
#endif
