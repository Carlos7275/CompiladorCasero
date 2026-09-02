#ifndef TYPES_H
#define TYPES_H

enum TipoToken
{
    PalRes,
    ID,
    NUM,
    SIM,
    OPAR,
    OPCOMP,
    OPASIGN,
    OPLOG,
    ESPECIAL,
    UNARIO,
    CAD,
    SEPARADOR,
    T_ROMPER,
    T_CONTINUAR,
    DESCONOCIDO
};

enum TipoDato
{
    INT,
    STRING,
    CHAR,
    FLOAT,
    BOOL,
    TIPO_VOID,
    TIPO_ERROR,
    OTRO
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
void reiniciar_tokens(void);
void generarToken(int tipoToken, const char *lexema, int tipoDato, int Col, int Renglon);
void imprimir_lexico(struct nodo *Nodo);
#endif
