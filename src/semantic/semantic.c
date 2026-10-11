#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "types.h"
#include "parser.h"
#include "symbols.h"
#include "semantic.h"
#include "errors.h"
extern const char *DataTypeNames[];

TablaSimbolos *ambito_actual;
struct ErrorSemantico *cabeza_errores = NULL;
int profundidad_loop = 0;

int contador_errores_semanticos = 0;
static ASTNode *raiz_semantica = NULL;
static const char *clase_semantica_actual = NULL;

typedef struct
{
    const char *nombre;
    enum TipoDato retorno;
    enum TipoDato primero;
    enum TipoDato segundo;
    int argumentos;
    enum TipoDato tercero;
    enum TipoDato cuarto;
} FuncionNativa;

static const FuncionNativa funciones_nativas[] = {
    {"Http", INT, STRING, STRING, 4, STRING, STRING},
    {"HttpTLS", INT, STRING, STRING, 4, STRING, STRING},
    {"Abs", INT, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Absoluto", INT, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Min", INT, INT, INT, 2, TIPO_ERROR, TIPO_ERROR},
    {"Max", INT, INT, INT, 2, TIPO_ERROR, TIPO_ERROR},
    {"Potencia", FLOAT, FLOAT, FLOAT, 2, TIPO_ERROR, TIPO_ERROR},
    {"RaizCuadrada", FLOAT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Seno", FLOAT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Coseno", FLOAT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Tangente", FLOAT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Logaritmo", FLOAT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Exponencial", FLOAT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Piso", FLOAT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Techo", FLOAT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Redondear", INT, FLOAT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Longitud", INT, STRING, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"Comparar", INT, STRING, STRING, 2, TIPO_ERROR, TIPO_ERROR},
    {"Contiene", BOOL, STRING, STRING, 2, TIPO_ERROR, TIPO_ERROR},
    {"Aleatorio", INT, TIPO_ERROR, TIPO_ERROR, 0, TIPO_ERROR, TIPO_ERROR},
    {"Aleatorio", INT, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"AleatorioEntre", INT, INT, INT, 2, TIPO_ERROR, TIPO_ERROR},
    {"Entorno", STRING, STRING, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"ExisteEntorno", BOOL, STRING, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"HttpCuerpo", STRING, TIPO_ERROR, TIPO_ERROR, 0, TIPO_ERROR, TIPO_ERROR},
    {"HttpCabeceras", STRING, TIPO_ERROR, TIPO_ERROR, 0, TIPO_ERROR, TIPO_ERROR},
    {"AbrirArchivo", INT, STRING, STRING, 2, TIPO_ERROR, TIPO_ERROR},
    {"LeerLinea", STRING, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"LeerTodoArchivo", STRING, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"EscribirArchivo", INT, INT, STRING, 2, TIPO_ERROR, TIPO_ERROR},
    {"FinArchivo", BOOL, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"CerrarArchivo", BOOL, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"UltimoError", STRING, TIPO_ERROR, TIPO_ERROR, 0, TIPO_ERROR, TIPO_ERROR},
    {"ObtenerMensaje", STRING, STRING, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"EliminarArchivo", BOOL, STRING, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"SocketTCPConectar", INT, STRING, INT, 2, TIPO_ERROR, TIPO_ERROR},
    {"SocketTLSCliente", BOOL, INT, STRING, 2, TIPO_ERROR, TIPO_ERROR},
    {"SocketTLSServidor", BOOL, INT, STRING, 3, STRING, TIPO_ERROR},
    {"SocketTCPEscuchar", INT, INT, INT, 2, TIPO_ERROR, TIPO_ERROR},
    {"SocketTCPAceptar", INT, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"SocketEnviar", INT, INT, STRING, 2, TIPO_ERROR, TIPO_ERROR},
    {"SocketRecibir", STRING, INT, INT, 2, TIPO_ERROR, TIPO_ERROR},
    {"SocketRecibirLinea", STRING, INT, INT, 2, TIPO_ERROR, TIPO_ERROR},
    {"SocketUDP", INT, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"SocketEnviarUDP", INT, INT, STRING, 4, INT, STRING},
    {"SocketRecibirUDP", STRING, INT, INT, 2, TIPO_ERROR, TIPO_ERROR},
    {"SocketDireccionRemota", STRING, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"SocketPuertoLocal", INT, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
    {"SocketTimeout", BOOL, INT, INT, 3, INT, TIPO_ERROR},
    {"SocketCerrar", BOOL, INT, TIPO_ERROR, 1, TIPO_ERROR, TIPO_ERROR},
     {"Decimales", FLOAT, FLOAT, INT, 2, TIPO_ERROR, TIPO_ERROR}
};

typedef struct
{
    const char *tipo;
    const char *metodo;
    const char *nativa;
    const char *modo;
    enum TipoDato retorno;
    int argumentos;
    enum TipoDato tipos[4];
} MetodoNativoOO;

static const MetodoNativoOO metodos_nativos_oo[] = {
    {"Socket", "conectar", "SocketTCPConectar", "static", INT, 2, {STRING, INT}},
    {"Socket", "connect", "SocketTCPConectar", "static", INT, 2, {STRING, INT}},
    {"Socket", "escuchar", "SocketTCPEscuchar", "static", INT, 2, {INT, INT}},
    {"Socket", "listen", "SocketTCPEscuchar", "static", INT, 2, {INT, INT}},
    {"Socket", "udp", "SocketUDP", "static", INT, 1, {INT}},
    {"Socket", "aceptar", "SocketTCPAceptar", "receiver", INT, 0, {TIPO_ERROR}},
    {"Socket", "accept", "SocketTCPAceptar", "receiver", INT, 0, {TIPO_ERROR}},
    {"Socket", "enviar", "SocketEnviar", "receiver", INT, 1, {STRING}},
    {"Socket", "send", "SocketEnviar", "receiver", INT, 1, {STRING}},
    {"Socket", "recibir", "SocketRecibir", "receiver", STRING, 1, {INT}},
    {"Socket", "receive", "SocketRecibir", "receiver", STRING, 1, {INT}},
    {"Socket", "recibirLinea", "SocketRecibirLinea", "receiver", STRING, 1, {INT}},
    {"Socket", "receiveLine", "SocketRecibirLinea", "receiver", STRING, 1, {INT}},
    {"Socket", "enviarUDP", "SocketEnviarUDP", "receiver", INT, 3, {STRING, INT, STRING}},
    {"Socket", "sendUDP", "SocketEnviarUDP", "receiver", INT, 3, {STRING, INT, STRING}},
    {"Socket", "recibirUDP", "SocketRecibirUDP", "receiver", STRING, 1, {INT}},
    {"Socket", "receiveUDP", "SocketRecibirUDP", "receiver", STRING, 1, {INT}},
    {"Socket", "direccionRemota", "SocketDireccionRemota", "receiver", STRING, 0, {TIPO_ERROR}},
    {"Socket", "remoteAddress", "SocketDireccionRemota", "receiver", STRING, 0, {TIPO_ERROR}},
    {"Socket", "puertoLocal", "SocketPuertoLocal", "receiver", INT, 0, {TIPO_ERROR}},
    {"Socket", "localPort", "SocketPuertoLocal", "receiver", INT, 0, {TIPO_ERROR}},
    {"Socket", "timeout", "SocketTimeout", "receiver", BOOL, 2, {INT, INT}},
    {"Socket", "cerrar", "SocketCerrar", "receiver", BOOL, 0, {TIPO_ERROR}},
    {"Socket", "close", "SocketCerrar", "receiver", BOOL, 0, {TIPO_ERROR}},
    {"Socket", "tlsCliente", "SocketTLSCliente", "receiver", BOOL, 1, {STRING}},
    {"Socket", "tlsServidor", "SocketTLSServidor", "receiver", BOOL, 2, {STRING, STRING}},
    {"HttpClient", "request", "Http", "http_request", INT, 4, {STRING, STRING, STRING, STRING}},
    {"HttpClient", "requestTLS", "HttpTLS", "http_tls", INT, 4, {STRING, STRING, STRING, STRING}},
    {"HttpClient", "get", "Http", "http_get", INT, 1, {STRING}},
    {"HttpClient", "post", "Http", "http_post", INT, 2, {STRING, STRING}},
    {"HttpClient", "put", "Http", "http_put", INT, 2, {STRING, STRING}},
    {"HttpClient", "delete", "Http", "http_delete", INT, 1, {STRING}},
    {"HttpClient", "body", "HttpCuerpo", "http_body", STRING, 0, {TIPO_ERROR}},
    {"HttpClient", "cuerpo", "HttpCuerpo", "http_body", STRING, 0, {TIPO_ERROR}},
    {"HttpClient", "headers", "HttpCabeceras", "http_headers", STRING, 0, {TIPO_ERROR}},
    {"HttpClient", "cabeceras", "HttpCabeceras", "http_headers", STRING, 0, {TIPO_ERROR}}
};

static const MetodoNativoOO *buscar_metodo_nativo_oo(const char *tipo, const char *metodo)
{
    for (size_t i = 0; i < sizeof(metodos_nativos_oo) / sizeof(metodos_nativos_oo[0]); ++i)
        if (strcmp(metodos_nativos_oo[i].tipo, tipo) == 0 &&
            strcmp(metodos_nativos_oo[i].metodo, metodo) == 0)
            return &metodos_nativos_oo[i];
    return NULL;
}

static int es_tipo_nativo_oo(const char *tipo)
{
    for (size_t i = 0; i < sizeof(metodos_nativos_oo) / sizeof(metodos_nativos_oo[0]); ++i)
        if (strcmp(metodos_nativos_oo[i].tipo, tipo) == 0)
            return 1;
    return 0;
}

static const FuncionNativa *buscar_funcion_nativa(const char *nombre, int argumentos)
{
    for (size_t i = 0; i < sizeof(funciones_nativas) / sizeof(funciones_nativas[0]); i++)
        if (strcmp(funciones_nativas[i].nombre, nombre) == 0 &&
            funciones_nativas[i].argumentos == argumentos)
            return &funciones_nativas[i];
    return NULL;
}

static ASTNode *buscar_tipo(ASTNode *node, const char *nombre)
{
    for (ASTNode *actual = node; actual; actual = actual->siguiente_hermano)
    {
        if (actual->type == AST_DECLARACION_TIPO && actual->hijo_izq &&
            strcmp(actual->hijo_izq->valor.nombre_id, nombre) == 0)
            return actual;
    }
    return NULL;
}

static int es_subtipo_de(const char *tipo, const char *base, int profundidad)
{
    if (!tipo || !base || profundidad > 64)
        return 0;
    if (strcmp(tipo, base) == 0)
        return 1;
    ASTNode *declaracion = buscar_tipo(raiz_semantica->hijo_izq, tipo);
    return declaracion && declaracion->tipo_nombre
               ? es_subtipo_de(declaracion->tipo_nombre, base, profundidad + 1)
               : 0;
}

static int miembro_accesible(enum AccesoMiembro acceso, const char *propietario)
{
    if (acceso == ACCESO_PUBLICO)
        return 1;
    if (!clase_semantica_actual || !propietario)
        return 0;
    if (acceso == ACCESO_PRIVADO)
        return strcmp(clase_semantica_actual, propietario) == 0;
    return es_subtipo_de(clase_semantica_actual, propietario, 0);
}

static ASTNode *buscar_metodo_tipo(ASTNode *tipo, const char *nombre,
                                  const char **propietario, int profundidad)
{
    if (!tipo || !nombre || profundidad > 64)
        return NULL;

    for (ASTNode *metodo = tipo->parametros; metodo; metodo = metodo->siguiente_hermano)
        if (metodo->type == AST_FUNCION && metodo->hijo_izq &&
            strcmp(metodo->hijo_izq->valor.nombre_id, nombre) == 0)
        {
            if (propietario && tipo->hijo_izq)
                *propietario = tipo->hijo_izq->valor.nombre_id;
            return metodo;
        }

    if (tipo->tipo_nombre)
        return buscar_metodo_tipo(
            buscar_tipo(raiz_semantica->hijo_izq, tipo->tipo_nombre), nombre,
            propietario, profundidad + 1);
    return NULL;
}

static ASTNode *buscar_campo_tipo(ASTNode *tipo, const char *nombre,
                                 const char **propietario, int profundidad)
{
    if (!tipo || !nombre || profundidad > 64)
        return NULL;

    for (ASTNode *campo = tipo->hijo_der; campo; campo = campo->siguiente_hermano)
        if (campo->type == AST_DECLARACION_VAR && campo->hijo_izq &&
            strcmp(campo->hijo_izq->valor.nombre_id, nombre) == 0)
        {
            if (propietario && tipo->hijo_izq)
                *propietario = tipo->hijo_izq->valor.nombre_id;
            return campo;
        }

    if (tipo->tipo_nombre)
        return buscar_campo_tipo(
            buscar_tipo(raiz_semantica->hijo_izq, tipo->tipo_nombre), nombre,
            propietario, profundidad + 1);
    return NULL;
}

static void registrar_campos_tipo(ASTNode *tipo, const char *instancia, int profundidad)
{
    if (!tipo || !instancia || profundidad > 64)
        return;

    if (tipo->tipo_nombre)
    {
        ASTNode *base = buscar_tipo(raiz_semantica->hijo_izq, tipo->tipo_nombre);
        registrar_campos_tipo(base, instancia, profundidad + 1);
    }

    for (ASTNode *campo = tipo->hijo_der; campo; campo = campo->siguiente_hermano)
    {
        if (campo->type != AST_DECLARACION_VAR || !campo->hijo_izq)
            continue;

        char nombre_campo[256];
        snprintf(nombre_campo, sizeof(nombre_campo), "%s__%s",
                 instancia, campo->hijo_izq->valor.nombre_id);
        agregar_simbolo(ambito_actual, nombre_campo,
                        campo->declared_type_info, campo->renglon, campo->columna);
    }
}

/**
 * Registra un error semántico con información de ubicación y mensaje formateado.
 *
 * @param renglon Línea donde ocurrió el problema.
 * @param columna Columna donde ocurrió el problema.
 * @param formato Texto del mensaje con formato printf.
 */
void reportar_error_semantico(int renglon, int columna, const char *formato, ...)
{
    struct ErrorSemantico *nuevo_error = (struct ErrorSemantico *)malloc(sizeof(struct ErrorSemantico));
    if (nuevo_error == NULL)
    {
        perror("Error al asignar memoria para el error semantico");
        exit(EXIT_FAILURE);
    }

    nuevo_error->renglon = renglon;
    nuevo_error->columna = columna;

    va_list args;
    va_start(args, formato);
    vsnprintf(nuevo_error->mensaje, sizeof(nuevo_error->mensaje), formato, args);
    va_end(args);

    report_diagnostic("MX3000", "error", "semantic", renglon, columna,
                      nuevo_error->mensaje);
    nuevo_error->sig = cabeza_errores;
    cabeza_errores = nuevo_error;
    contador_errores_semanticos++;
}

/**
 * Muestra los errores semánticos ordenados por posición en el archivo fuente.
 */
void imprimir_errores_semanticos(void)
{
    if (diagnostics_json_enabled())
    {
        emit_diagnostics_json();
        struct ErrorSemantico *actual_json = cabeza_errores;
        while (actual_json)
        {
            struct ErrorSemantico *next = actual_json->sig;
            free(actual_json);
            actual_json = next;
        }
        cabeza_errores = NULL;
        contador_errores_semanticos = 0;
        return;
    }

    if (cabeza_errores == NULL)
    {
        printf("No se detectaron errores semanticos.\n");
        return;
    }

    if (contador_errores_semanticos == 0)
    {
        printf("No se detectaron errores semanticos.\n");
        printf("-------------------------------------------\n");
        return;
    }

    struct ErrorSemantico **errores_array = (struct ErrorSemantico **)malloc(
        sizeof(struct ErrorSemantico *) * contador_errores_semanticos);

    if (errores_array == NULL)
    {
        perror("Error: No se pudo asignar memoria para ordenar los errores semanticos. Imprimiendo sin ordenar.");
        struct ErrorSemantico *actual_fallback = cabeza_errores;
        while (actual_fallback != NULL)
        {
            fprintf(stderr, "Error (R%d, C%d): %s\n", actual_fallback->renglon, actual_fallback->columna, actual_fallback->mensaje);
            actual_fallback = actual_fallback->sig;
        }
        printf("-------------------------------------------\n");
        actual_fallback = cabeza_errores;
        while (actual_fallback != NULL)
        {
            struct ErrorSemantico *temp = actual_fallback;
            actual_fallback = actual_fallback->sig;
            free(temp);
        }
        cabeza_errores = NULL;
        return;
    }

    struct ErrorSemantico *actual = cabeza_errores;
    int i = 0;
    while (actual != NULL && i < contador_errores_semanticos)
    {
        errores_array[i++] = actual;
        actual = actual->sig;
    }

    int n = contador_errores_semanticos;
    for (int k = 0; k < n - 1; k++)
    {
        for (int j = 0; j < n - k - 1; j++)
        {
            if (errores_array[j]->renglon > errores_array[j + 1]->renglon ||
                (errores_array[j]->renglon == errores_array[j + 1]->renglon &&
                 errores_array[j]->columna > errores_array[j + 1]->columna))
            {
                struct ErrorSemantico *temp = errores_array[j];
                errores_array[j] = errores_array[j + 1];
                errores_array[j + 1] = temp;
            }
        }
    }

    for (i = 0; i < contador_errores_semanticos; i++)
    {
        fprintf(stderr, "Error (R%d, C%d): %s\n",
                errores_array[i]->renglon, errores_array[i]->columna, errores_array[i]->mensaje);
    }

    printf("-------------------------------------------\n");

    free(errores_array);

    actual = cabeza_errores;
    while (actual != NULL)
    {
        struct ErrorSemantico *temp = actual;
        actual = actual->sig;
        free(temp);
    }
    cabeza_errores = NULL;
}

/**
 * Convierte un valor del enumerador de tipos a una cadena legible.
 *
 * @param tipo Tipo de dato a convertir.
 * @return Nombre textual del tipo.
 */
const char *tipoDatoToString(enum TipoDato tipo)
{
    switch (tipo)
    {
    case INT:
        return "Entero";
    case FLOAT:
        return "Flotante";
    case STRING:
        return "Cadena";
    case CHAR:
        return "Caracter";
    case BOOL:
        return "Booleano";
    case TIPO_VOID:
        return "Void";
    case TIPO_ERROR:
        return "Error";
    default:
        return "Desconocido";
    }
}

/**
 * Valida si una asignación es compatible entre tipos.
 *
 * @param renglon Línea de la asignación.
 * @param columna Columna de la asignación.
 * @param tipo_destino Tipo del destino.
 * @param tipo_origen Tipo del valor asignado.
 */
void verificar_asignacion(int renglon, int columna, enum TipoDato tipo_destino, enum TipoDato tipo_origen)
{
    if (tipo_destino == TIPO_ERROR || tipo_origen == TIPO_ERROR)
    {
        return;
    }

    if (tipo_destino == tipo_origen)
    {
        return;
    }

    if (tipo_destino == FLOAT && tipo_origen == INT)
    {
        return;
    }

    if (tipo_destino == INT && tipo_origen == FLOAT)
    {
        return;
    }

    /*
     * Booleano e Entero comparten representación lógica:
     * Booleano -> Entero: Verdadero/Falso se representan como 1/0.
     * Entero 0/1 -> Booleano: conversión segura.
     *
     * La conversión general Entero -> Booleano se valida con la expresión
     * para poder rechazar literales distintos de 0 y 1.
     */
    if (tipo_destino == INT && tipo_origen == BOOL)
    {
        return;
    }

    if (tipo_destino == BOOL && tipo_origen == INT)
    {
        return;
    }

    reportar_error_semantico(renglon, columna,
                             "Tipos incompatibles en asignacion: no se puede asignar %s a %s.",
                             tipoDatoToString(tipo_origen), tipoDatoToString(tipo_destino));
}

/**
 * Valida una asignación considerando la expresión origen para aplicar
 * las reglas especiales de compatibilidad entre Entero y Booleano.
 *
 * @param renglon Línea de la asignación.
 * @param columna Columna de la asignación.
 * @param tipo_destino Tipo del destino.
 * @param expresion Origen completo de la asignación.
 */
static void verificar_asignacion_expresion(int renglon, int columna,
                                           enum TipoDato tipo_destino, ASTNode *expresion)
{
    if (expresion == NULL)
    {
        return;
    }

    enum TipoDato tipo_origen = expresion->resolved_type;

    if (tipo_destino == BOOL && tipo_origen == INT)
    {
        return;
    }

    verificar_asignacion(renglon, columna, tipo_destino, tipo_origen);
}

/**
 * Valida y devuelve el tipo resultante de una operación aritmética.
 *
 * @param renglon Línea del operador.
 * @param columna Columna del operador.
 * @param tipo1 Primer operando.
 * @param tipo2 Segundo operando.
 * @return Tipo del resultado o TIPO_ERROR si la operación es inválida.
 */
enum TipoDato verificar_expresion_aritmetica(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2)
{
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if (tipo1 == FLOAT || tipo2 == FLOAT)
    {
        if ((tipo1 == INT || tipo1 == FLOAT || tipo1 == BOOL) &&
            (tipo2 == INT || tipo2 == FLOAT || tipo2 == BOOL))
        {
            return FLOAT;
        }
    }

    if ((tipo1 == INT || tipo1 == BOOL) &&
        (tipo2 == INT || tipo2 == BOOL))
    {
        return INT;
    }

    if (tipo1 == STRING && tipo2 == STRING)
    {
        return STRING;
    }

    reportar_error_semantico(renglon, columna,
                             "Operacion aritmetica con tipos incompatibles: %s y %s.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

/**
 * Valida y devuelve el tipo del resultado de una comparación.
 *
 * @param renglon Línea del operador de comparación.
 * @param columna Columna del operador de comparación.
 * @param tipo1 Primer operando.
 * @param tipo2 Segundo operando.
 * @return Tipo booleano si la comparación es válida, o TIPO_ERROR en caso contrario.
 */
enum TipoDato verificar_expresion_comparacion(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2)
{
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if ((tipo1 == INT || tipo1 == FLOAT || tipo1 == BOOL) &&
        (tipo2 == INT || tipo2 == FLOAT || tipo2 == BOOL))
    {
        return BOOL;
    }

    if ((tipo1 == BOOL || tipo1 == INT) &&
        (tipo2 == BOOL || tipo2 == INT))
    {
        return BOOL;
    }

    if ((tipo1 == STRING && tipo2 == STRING) ||
        (tipo1 == CHAR && tipo2 == CHAR))
    {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Comparacion de tipos incompatibles: %s y %s.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

/**
 * Valida una operación lógica binaria usando Booleano o Entero como
 * operandos booleanos. En Entero, 0 representa falso y cualquier valor
 * distinto de 0 representa verdadero.
 *
 * @param renglon Línea del operador.
 * @param columna Columna del operador.
 * @param tipo1 Tipo del primer operando.
 * @param tipo2 Tipo del segundo operando.
 * @return BOOL si la operación es válida, o TIPO_ERROR en caso contrario.
 */
enum TipoDato verificar_expresion_logica(int renglon, int columna, enum TipoDato tipo1, enum TipoDato tipo2)
{
    if (tipo1 == TIPO_ERROR || tipo2 == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if ((tipo1 == BOOL || tipo1 == INT) &&
        (tipo2 == BOOL || tipo2 == INT))
    {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Operacion logica con tipos incompatibles: %s y %s. "
                             "Se esperaban Booleanos o Enteros.",
                             tipoDatoToString(tipo1), tipoDatoToString(tipo2));
    return TIPO_ERROR;
}

enum TipoDato verificar_negacion_unaria(int renglon, int columna, enum TipoDato tipo)
{
    if (tipo == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if (tipo == INT || tipo == FLOAT || tipo == BOOL)
    {
        return tipo == BOOL ? INT : tipo;
    }

    reportar_error_semantico(renglon, columna,
                             "Operador de negacion unaria (-) aplicado a tipo no numerico: %s.",
                             tipoDatoToString(tipo));
    return TIPO_ERROR;
}

/**
 * Valida una negación lógica usando Booleano o Entero como operando.
 * Para Entero, 0 representa falso y cualquier valor distinto de 0
 * representa verdadero.
 *
 * @param renglon Línea del operador.
 * @param columna Columna del operador.
 * @param tipo Tipo del operando.
 * @return BOOL si el operador es válido, o TIPO_ERROR en caso contrario.
 */
enum TipoDato verificar_negacion_logica(int renglon, int columna, enum TipoDato tipo)
{
    if (tipo == TIPO_ERROR)
    {
        return TIPO_ERROR;
    }

    if (tipo == BOOL || tipo == INT)
    {
        return BOOL;
    }

    reportar_error_semantico(renglon, columna,
                             "Operador de negacion logica (NOT) aplicado a tipo no booleano ni entero: %s.",
                             tipoDatoToString(tipo));
    return TIPO_ERROR;
}

TablaSimbolos *realizar_analisis_semantico(ASTNode *raiz_ast)
{
    ambito_actual = crear_tabla_simbolos(NULL);
    raiz_semantica = raiz_ast;
    inicializarTablaSimbolos();
    visit_ast_semantic(raiz_ast);
    return ambito_actual;
}

void inicializarTablaSimbolos(void)
{
    EntradaSimbolo *entrada;

    if (ambito_actual == NULL)
    {

        fprintf(stderr, "Error: La tabla de símbolos global (ambito_actual) no ha sido inicializada.\n");
        exit(EXIT_FAILURE);
    }

    entrada = agregar_simbolo(ambito_actual, "PI", FLOAT, 0, 0);
    if (entrada)
    {
        entrada->es_constante = 1;
        entrada->valor_constante.valor_float = 3.14159265358979323846;
    }
    else
    {
        fprintf(stderr, "Error al insertar 'PI' en la tabla de símbolos.\n");
    }

    entrada = agregar_simbolo(ambito_actual, "E", FLOAT, 0, 0);
    if (entrada)
    {
        entrada->es_constante = 1;
        entrada->valor_constante.valor_float = 2.71828182845904523536;
    }
    else
    {
        fprintf(stderr, "Error al insertar 'E' en la tabla de símbolos.\n");
    }

    entrada = agregar_simbolo(ambito_actual, "MAX_ENTERO", INT, 0, 0);
    if (entrada)
    {
        entrada->es_constante = 1;
        entrada->valor_constante.valor_int = INT64_MAX;
    }
    else
    {
        fprintf(stderr, "Error al insertar 'MAX_ENTERO' en la tabla de símbolos.\n");
    }

    entrada = agregar_simbolo(ambito_actual, "MAX_FLOTANTE", FLOAT, 0, 0);
    if (entrada)
    {
        entrada->es_constante = 1;
        entrada->valor_constante.valor_float = 3.40282347E+38;
    }
    else
    {
        fprintf(stderr, "Error al insertar 'MAX_FLOTANTE' en la tabla de símbolos.\n");
    }
}

void visit_ast_semantic(ASTNode *node)
{
    if (node == NULL)
    {
        return;
    }

    switch (node->type)
    {
    case AST_IMPORT:
        reportar_error_semantico(node->renglon, node->columna,
                                 "Importacion no resuelta: '%s'", node->valor.valor_cadena);
        break;
    case AST_DECLARACION_TIPO:
        if (node->hijo_izq && !buscar_simbolo_en_ambito_actual(ambito_actual, node->hijo_izq->valor.nombre_id))
            agregar_simbolo(ambito_actual, node->hijo_izq->valor.nombre_id, OTRO, node->renglon, node->columna);
        if (node->tipo_nombre && !buscar_tipo(raiz_semantica->hijo_izq, node->tipo_nombre))
            reportar_error_semantico(node->renglon, node->columna,
                                     "Clase base no declarada: '%s'.", node->tipo_nombre);
        for (ASTNode *member = node->hijo_der; member; member = member->siguiente_hermano)
            if (member->es_virtual || member->es_override)
                reportar_error_semantico(member->renglon, member->columna,
                                         "Virtual/Sobrescribe solo se permite en metodos.");

        for (ASTNode *method = node->parametros; method; method = method->siguiente_hermano)
        {
            ASTNode *base_type = node->tipo_nombre
                                     ? buscar_tipo(raiz_semantica->hijo_izq, node->tipo_nombre)
                                     : NULL;
            const char *base_owner = NULL;
            ASTNode *base_method = base_type && method->hijo_izq
                                       ? buscar_metodo_tipo(base_type,
                                                           method->hijo_izq->valor.nombre_id,
                                                           &base_owner, 0)
                                       : NULL;
            if (method->es_override && (!base_method || !base_method->es_virtual))
                reportar_error_semantico(method->renglon, method->columna,
                                         "Sobrescribe requiere un metodo Virtual heredado.");
            if (base_method && base_method->es_virtual && !method->es_override)
                reportar_error_semantico(method->renglon, method->columna,
                                         "El metodo que sobrescribe uno Virtual debe marcarse Sobrescribe.");
            if (method->es_override && base_method)
            {
                ASTNode *base_parameter = base_method->parametros;
                ASTNode *parameter = method->parametros;
                int signature_matches = method->return_type == base_method->return_type;
                while (base_parameter && parameter)
                {
                    if (base_parameter->declared_type_info != parameter->declared_type_info)
                        signature_matches = 0;
                    base_parameter = base_parameter->siguiente_hermano;
                    parameter = parameter->siguiente_hermano;
                }
                if (base_parameter || parameter)
                    signature_matches = 0;
                if (!signature_matches)
                    reportar_error_semantico(method->renglon, method->columna,
                                             "Sobrescribe debe conservar la firma del metodo virtual.");
            }
            if (method->es_override)
                method->es_virtual = 1;

            const char *previous_class = clase_semantica_actual;
            clase_semantica_actual = node->hijo_izq
                                         ? node->hijo_izq->valor.nombre_id
                                         : NULL;
            TablaSimbolos *method_scope = crear_tabla_simbolos(ambito_actual);
            ambito_actual = method_scope;
            EntradaSimbolo *receiver = agregar_simbolo(ambito_actual, "este", OTRO,
                                                       method->renglon, method->columna);
            if (receiver && node->hijo_izq)
                receiver->tipo_nombre = strdup(node->hijo_izq->valor.nombre_id);
            if (node->hijo_izq)
                registrar_campos_tipo(node, "este", 0);
            visit_ast_semantic(method);
            ambito_actual = method_scope->padre;
            clase_semantica_actual = previous_class;
        }
        break;
    case AST_FUNCION:
        if (node->hijo_izq) agregar_simbolo(ambito_actual, node->hijo_izq->valor.nombre_id,
                                             node->return_type, node->renglon, node->columna);
        for (ASTNode *p = node->parametros; p; p = p->siguiente_hermano)
            if (p->hijo_izq) agregar_simbolo(ambito_actual, p->hijo_izq->valor.nombre_id,
                                              p->declared_type_info, p->renglon, p->columna);
        visit_ast_semantic(node->hijo_der);
        break;
    case AST_LLAMADA:
        if (node->tipo_nombre)
        {
            EntradaSimbolo *receiver = buscar_simbolo(ambito_actual, node->tipo_nombre);
            const char *receiver_type = receiver ? receiver->tipo_nombre : node->tipo_nombre;
            int static_receiver = !receiver && es_tipo_nativo_oo(receiver_type);
            if (receiver && node->parametros)
                visit_ast_semantic(node->parametros);
            if (node->hijo_der)
                for (ASTNode *argument = node->hijo_der; argument; argument = argument->siguiente_hermano)
                    visit_ast_semantic(argument);

            const MetodoNativoOO *native_method =
                (receiver_type && (receiver || static_receiver) && node->hijo_izq)
                    ? buscar_metodo_nativo_oo(receiver_type, node->hijo_izq->valor.nombre_id)
                    : NULL;
            if (native_method)
            {
                int count = 0;
                for (ASTNode *argument = node->hijo_der; argument; argument = argument->siguiente_hermano)
                    ++count;
                if (count != native_method->argumentos ||
                    (strcmp(native_method->modo, "static") == 0 && receiver) ||
                    (strcmp(native_method->modo, "receiver") == 0 && !receiver))
                {
                    reportar_error_semantico(node->renglon, node->columna,
                                             "Uso invalido del metodo '%s' para '%s'.",
                                             node->hijo_izq->valor.nombre_id, receiver_type);
                    node->resolved_type = TIPO_ERROR;
                    break;
                }

                ASTNode *argument = node->hijo_der;
                for (int i = 0; i < native_method->argumentos && argument; ++i, argument = argument->siguiente_hermano)
                    verificar_asignacion(argument->renglon, argument->columna,
                                         native_method->tipos[i], argument->resolved_type);

                size_t marker_length = strlen(native_method->modo) + strlen(native_method->nativa) + 16;
                node->ir_result_name = malloc(marker_length);
                if (node->ir_result_name)
                    snprintf(node->ir_result_name, marker_length, "__mxmethod__%s__%s",
                             native_method->modo, native_method->nativa);
                node->resolved_type = native_method->retorno;
                break;
            }

            ASTNode *type = receiver && receiver_type
                                ? buscar_tipo(raiz_semantica->hijo_izq, receiver_type)
                                : NULL;
            const char *method_owner = NULL;
            ASTNode *method = type && node->hijo_izq
                                  ? buscar_metodo_tipo(type, node->hijo_izq->valor.nombre_id,
                                                       &method_owner, 0)
                                  : NULL;
            if (!receiver || !type || !method)
            {
                reportar_error_semantico(node->renglon, node->columna,
                                         "Metodo no encontrado para el objeto '%s'.",
                                         node->tipo_nombre);
                node->resolved_type = TIPO_ERROR;
                break;
            }
            if (!node->receptor_nombre)
                node->receptor_nombre = strdup(node->tipo_nombre);
            free(node->tipo_nombre);
            node->tipo_nombre = strdup(receiver_type);
            if (!miembro_accesible(method->acceso_miembro, method_owner))
                reportar_error_semantico(node->renglon, node->columna,
                                         "El metodo '%s' no es accesible desde esta clase.",
                                         node->hijo_izq->valor.nombre_id);

            node->ir_result_name = strdup(receiver->tipo_nombre);
            ASTNode *formal = method->parametros;
            ASTNode *argument = node->hijo_der;
            while (formal && argument)
            {
                verificar_asignacion(argument->renglon, argument->columna,
                                     formal->declared_type_info,
                                     argument->resolved_type);
                formal = formal->siguiente_hermano;
                argument = argument->siguiente_hermano;
            }
            if (formal || argument)
                reportar_error_semantico(node->renglon, node->columna,
                                         "Numero incorrecto de argumentos para el metodo '%s'.",
                                         node->hijo_izq->valor.nombre_id);
            node->resolved_type = method->return_type;
            break;
        }
        if (node->hijo_der) for (ASTNode *a=node->hijo_der; a; a=a->siguiente_hermano) visit_ast_semantic(a);
        if (node->hijo_izq) {
            EntradaSimbolo *f = buscar_simbolo(ambito_actual, node->hijo_izq->valor.nombre_id);
            int argc = 0;
            for (ASTNode *a = node->hijo_der; a; a = a->siguiente_hermano) argc++;
            const FuncionNativa *nativa = buscar_funcion_nativa(node->hijo_izq->valor.nombre_id, argc);
            if (nativa) {
                {
                    ASTNode *a = node->hijo_der;
                    enum TipoDato esperados[] = {
                        nativa->primero, nativa->segundo, nativa->tercero, nativa->cuarto
                    };
                    int tipos_invalidos = 0;
                    for (int i = 0; i < nativa->argumentos && a; i++, a = a->siguiente_hermano)
                        if (esperados[i] != TIPO_ERROR && a->resolved_type != esperados[i])
                            tipos_invalidos = 1;
                    if (tipos_invalidos) {
                        reportar_error_semantico(node->renglon, node->columna,
                                                 "Tipos invalidos para la funcion nativa '%s'.",
                                                 nativa->nombre);
                        node->resolved_type = TIPO_ERROR;
                    } else node->resolved_type = nativa->retorno;
                }
            } else if (!f) {
                reportar_error_semantico(node->renglon,node->columna,"Funcion no declarada: '%s'",node->hijo_izq->valor.nombre_id);
                node->resolved_type=TIPO_ERROR;
            } else node->resolved_type = f->tipo;
        }
        break;
    case AST_RETORNAR_STMT:
        if (node->hijo_izq) visit_ast_semantic(node->hijo_izq);
        break;
    case AST_TRY_STMT:
        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        break;
    case AST_THROW_STMT:
        visit_ast_semantic(node->hijo_izq);
        if (node->hijo_izq && node->hijo_izq->resolved_type != STRING &&
            node->hijo_izq->resolved_type != TIPO_ERROR)
            reportar_error_semantico(node->renglon, node->columna,
                                     "Lanzar requiere un mensaje de tipo Cadena.");
        break;
    case AST_PROGRAMA:
    {
        ASTNode *current_child = node->hijo_izq;
        while (current_child != NULL)
        {
            visit_ast_semantic(current_child);
            current_child = current_child->siguiente_hermano;
        }
        break;
    }

    case AST_LISTA_SENTENCIAS:
    case AST_BLOQUE:
    {
        TablaSimbolos *nuevo_ambito = crear_tabla_simbolos(ambito_actual);
        node->symbol_scope = nuevo_ambito;
        ambito_actual = nuevo_ambito;

        ASTNode *current = node->hijo_izq;
        while (current != NULL)
        {
            visit_ast_semantic(current);
            current = current->siguiente_hermano;
        }

        ambito_actual = ambito_actual->padre;
        break;
    }

    case AST_DECLARACION_VAR:
    {
        const char *nombre_var = node->hijo_izq->valor.nombre_id;
        enum TipoDato tipo_declarado = node->declared_type_info;
        if (node->tipo_nombre && strcmp(node->tipo_nombre, "Socket") == 0)
            tipo_declarado = INT;

        EntradaSimbolo *simbolo_agregado = agregar_simbolo(ambito_actual, nombre_var, tipo_declarado, node->renglon, node->columna);
        if (simbolo_agregado != NULL)
        {
            simbolo_agregado->es_constante = 0;
            if (node->tipo_nombre)
                simbolo_agregado->tipo_nombre = strdup(node->tipo_nombre);
        }
        if (node->tipo_nombre != NULL)
        {
            ASTNode *tipo = buscar_tipo(raiz_semantica->hijo_izq, node->tipo_nombre);
            if (tipo != NULL)
                registrar_campos_tipo(tipo, nombre_var, 0);
        }

        if (node->hijo_der != NULL)
        {
            visit_ast_semantic(node->hijo_der);
            verificar_asignacion_expresion(node->renglon, node->columna, tipo_declarado, node->hijo_der);
        }
        break;
    }

    case AST_DECLARACION_CONST:
    {
        const char *nombre_const = node->hijo_izq->valor.nombre_id;
        enum TipoDato tipo_declarado = node->declared_type_info;

        if (node->hijo_der == NULL)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "La constante '%s' debe ser inicializada.", nombre_const);
            node->resolved_type = TIPO_ERROR;
            break;
        }

        visit_ast_semantic(node->hijo_der);
        enum TipoDato tipo_expr = node->hijo_der->resolved_type;

        if (tipo_expr == TIPO_ERROR)
        {
            node->resolved_type = TIPO_ERROR;
            break;
        }

        verificar_asignacion(node->renglon, node->columna, tipo_declarado, tipo_expr);

        EntradaSimbolo *simbolo_agregado = agregar_simbolo(
            ambito_actual,
            nombre_const,
            tipo_declarado,
            node->renglon,
            node->columna);

        if (simbolo_agregado != NULL)
        {
            simbolo_agregado->es_constante = 1;
            if (node->hijo_der->tipoconstante == CONSTANTE_LITERAL)
            {
                if (tipo_declarado == INT)
                {
                    simbolo_agregado->valor_constante.valor_int = node->hijo_der->valor.valor_entero;
                }
                else if (tipo_declarado == FLOAT)
                {
                    simbolo_agregado->valor_constante.valor_float = node->hijo_der->valor.valor_numero;
                }
                else if (tipo_declarado == STRING)
                {
                    if (node->hijo_der->valor.valor_cadena != NULL)
                    {
                        simbolo_agregado->valor_constante.valor_cadena = strdup(node->hijo_der->valor.valor_cadena);
                    }
                    else
                    {
                        simbolo_agregado->valor_constante.valor_cadena = NULL;
                    }
                }
                else if (tipo_declarado == BOOL)
                {
                    simbolo_agregado->valor_constante.valor_bool = node->hijo_der->valor.valor_booleano;
                }
            }
        }
        node->resolved_type = tipo_declarado;
        break;
    }

    case AST_ASIGNACION_STMT:
    {
        visit_ast_semantic(node->hijo_izq);

        if (node->hijo_izq == NULL)
        {
            node->resolved_type = TIPO_ERROR;
            break;
        }

        if (node->hijo_izq->type != AST_IDENTIFICADOR &&
            node->hijo_izq->type != AST_ACCESO_ARRAY)
        {
            reportar_error_semantico(
                node->renglon,
                node->columna,
                "El lado izquierdo de la asignacion debe ser un identificador o un acceso a array."
            );
            node->resolved_type = TIPO_ERROR;
        }

        visit_ast_semantic(node->hijo_der);

        enum TipoDato tipo_destino = node->hijo_izq->resolved_type;
        enum TipoDato tipo_origen = node->hijo_der
                                      ? node->hijo_der->resolved_type
                                      : TIPO_ERROR;

        if (tipo_destino != TIPO_ERROR && tipo_origen != TIPO_ERROR)
        {
            verificar_asignacion_expresion(
                node->renglon,
                node->columna,
                tipo_destino,
                node->hijo_der
            );
        }

        node->resolved_type =
            (tipo_destino != TIPO_ERROR && tipo_origen != TIPO_ERROR)
                ? tipo_destino
                : TIPO_ERROR;

        break;
    }

    case AST_MOSTRAR_STMT:
    case AST_LEER_STMT:
    {
        ASTNode *current_arg = node->hijo_izq;
        while (current_arg != NULL)
        {
            visit_ast_semantic(current_arg);
            current_arg = current_arg->siguiente_hermano;
        }
        break;
    }

    case AST_SI_STMT:
    {
        visit_ast_semantic(node->hijo_izq);
        if (node->hijo_izq->resolved_type != BOOL &&
            node->hijo_izq->resolved_type != INT &&
            node->hijo_izq->resolved_type != TIPO_ERROR)
        {
            reportar_error_semantico(node->hijo_izq->renglon, node->hijo_izq->columna,
                                     "La condicion de la sentencia 'Si' debe ser Booleano o Entero, se encontro %s.",
                                     tipoDatoToString(node->hijo_izq->resolved_type));
        }
        visit_ast_semantic(node->hijo_der);
        if (node->siguiente_hermano != NULL && node->siguiente_hermano->type == AST_SINO_STMT)
        {
            visit_ast_semantic(node->siguiente_hermano);
        }
        break;
    }

    case AST_SINO_STMT:
        visit_ast_semantic(node->hijo_izq);
        break;

    case AST_MIENTRAS_STMT:
    {

        visit_ast_semantic(node->hijo_izq);
        if (node->hijo_izq->resolved_type != BOOL &&
            node->hijo_izq->resolved_type != INT &&
            node->hijo_izq->resolved_type != TIPO_ERROR)
        {
            reportar_error_semantico(node->hijo_izq->renglon, node->hijo_izq->columna,
                                     "La condicion del bucle 'Mientras' debe ser Booleano o Entero, se encontro %s.",
                                     tipoDatoToString(node->hijo_izq->resolved_type));
        }
        profundidad_loop++;
        visit_ast_semantic(node->hijo_der);
        profundidad_loop--;
        break;
    }

    case AST_PARA_STMT:
    {
        TablaSimbolos *nuevo_ambito_para = crear_tabla_simbolos(ambito_actual);
        ambito_actual = nuevo_ambito_para;

        ASTNode *for_params_node = node->hijo_izq;
        ASTNode *inicializacion_node = NULL;
        ASTNode *condicion_node = NULL;
        ASTNode *incremento_node = NULL;
        ASTNode *cuerpo_bucle_node = node->hijo_der;

        if (for_params_node != NULL)
        {
            inicializacion_node = for_params_node->hijo_izq;
            if (inicializacion_node != NULL)
            {
                visit_ast_semantic(inicializacion_node);

                condicion_node = inicializacion_node->siguiente_hermano;
                if (condicion_node != NULL)
                {
                    visit_ast_semantic(condicion_node);

                    if (condicion_node->resolved_type != BOOL &&
                        condicion_node->resolved_type != INT &&
                        condicion_node->resolved_type != TIPO_ERROR)
                    {
                        reportar_error_semantico(condicion_node->renglon, condicion_node->columna,
                                                 "La condicion del bucle 'Para' debe ser Booleano o Entero, se encontro %s.",
                                                 DataTypeNames[condicion_node->resolved_type]);
                    }

                    incremento_node = condicion_node->siguiente_hermano;
                    if (incremento_node != NULL)
                    {
                        visit_ast_semantic(incremento_node);
                    }
                    else
                    {
                        reportar_error_semantico(node->renglon, node->columna, "El bucle 'Para' requiere una expresion de incremento.");
                    }
                }
                else
                {
                    reportar_error_semantico(node->renglon, node->columna, "La condicion del bucle 'Para' no esta definida.");
                }
            }
            else
            {
                reportar_error_semantico(node->renglon, node->columna, "El bucle 'Para' requiere una inicializacion.");
            }
        }
        else
        {
            reportar_error_semantico(node->renglon, node->columna, "El bucle 'Para' requiere parametros.");
        }

        profundidad_loop++;

        if (cuerpo_bucle_node != NULL)
        {
            visit_ast_semantic(cuerpo_bucle_node);
        }
        else
        {
            reportar_error_semantico(node->renglon, node->columna, "El bucle 'Para' debe tener un cuerpo.");
        }

        profundidad_loop--;

        ambito_actual = ambito_actual->padre;

        break;
    }

    case AST_SUMA_EXPR:
    case AST_RESTA_EXPR:
    case AST_MULT_EXPR:
    case AST_DIV_EXPR:
    case AST_MOD_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        node->resolved_type = verificar_expresion_aritmetica(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type,
            node->hijo_der->resolved_type);
        break;
    }
    case AST_BIT_AND_EXPR:
    case AST_BIT_OR_EXPR:
    case AST_BIT_XOR_EXPR:
    case AST_SHIFT_LEFT_EXPR:
    case AST_SHIFT_RIGHT_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        enum TipoDato left = node->hijo_izq ? node->hijo_izq->resolved_type : TIPO_ERROR;
        enum TipoDato right = node->hijo_der ? node->hijo_der->resolved_type : TIPO_ERROR;
        if (left == TIPO_ERROR || right == TIPO_ERROR)
            node->resolved_type = TIPO_ERROR;
        else if ((left == INT || left == BOOL) &&
                 (right == INT || right == BOOL))
            node->resolved_type = INT;
        else
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "Los operadores bitwise requieren operandos Entero.");
            node->resolved_type = TIPO_ERROR;
        }
        break;
    }
    case AST_BIT_NOT_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        enum TipoDato operand = node->hijo_izq ? node->hijo_izq->resolved_type : TIPO_ERROR;
        if (operand == INT || operand == BOOL || operand == TIPO_ERROR)
            node->resolved_type = operand == BOOL ? INT : operand;
        else
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "El operador bitwise '~' requiere un operando Entero.");
            node->resolved_type = TIPO_ERROR;
        }
        break;
    }
    case AST_OR_EXPR:
    case AST_AND_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        node->resolved_type = verificar_expresion_logica(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type,
            node->hijo_der->resolved_type);
        break;
    }
    case AST_IGUAL_EXPR:
    case AST_DIFERENTE_EXPR:
    case AST_MENOR_QUE_EXPR:
    case AST_MAYOR_QUE_EXPR:
    case AST_MENOR_IGUAL_EXPR:
    case AST_MAYOR_IGUAL_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        node->resolved_type = verificar_expresion_comparacion(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type,
            node->hijo_der->resolved_type);
        break;
    }

    case AST_NOT_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        node->resolved_type = verificar_negacion_logica(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type);
        break;
    }
    case AST_NEGACION_UNARIA_EXPR:
    {
        visit_ast_semantic(node->hijo_izq);
        node->resolved_type = verificar_negacion_unaria(
            node->renglon, node->columna,
            node->hijo_izq->resolved_type);
        break;
    }

    case AST_ACCESO_ARRAY:
    {
        if (node->hijo_izq == NULL ||
            node->hijo_izq->type != AST_IDENTIFICADOR)
        {
            node->resolved_type = TIPO_ERROR;
            break;
        }

        const char *nombre_array = node->hijo_izq->valor.nombre_id;
        EntradaSimbolo *entrada =
            buscar_simbolo(ambito_actual, nombre_array);

        if (entrada == NULL)
        {
            reportar_error_semantico(
                node->renglon,
                node->columna,
                "Uso de variable/constante no declarada: '%s'",
                nombre_array
            );
            node->resolved_type = TIPO_ERROR;
            break;
        }

        if (node->hijo_der == NULL)
        {
            node->resolved_type = TIPO_ERROR;
            break;
        }

        visit_ast_semantic(node->hijo_der);

        if (node->hijo_der->resolved_type != INT &&
            node->hijo_der->resolved_type != BOOL &&
            node->hijo_der->resolved_type != TIPO_ERROR)
        {
            reportar_error_semantico(
                node->hijo_der->renglon,
                node->hijo_der->columna,
                "El indice de un array debe ser de tipo Entero."
            );
            node->resolved_type = TIPO_ERROR;
            break;
        }

        node->hijo_izq->resolved_type = entrada->tipo;
        node->resolved_type = entrada->tipo;
        break;
    }

    case AST_TERNARIO_EXPR:
    {
        if (node->hijo_izq == NULL ||
            node->hijo_der == NULL ||
            node->siguiente_hermano == NULL)
        {
            node->resolved_type = TIPO_ERROR;
            break;
        }

        visit_ast_semantic(node->hijo_izq);
        visit_ast_semantic(node->hijo_der);
        visit_ast_semantic(node->siguiente_hermano);

        enum TipoDato tipo_condicion = node->hijo_izq->resolved_type;
        enum TipoDato tipo_verdadero = node->hijo_der->resolved_type;
        enum TipoDato tipo_falso = node->siguiente_hermano->resolved_type;

        if (tipo_condicion != BOOL &&
            tipo_condicion != INT &&
            tipo_condicion != TIPO_ERROR)
        {
            reportar_error_semantico(
                node->hijo_izq->renglon,
                node->hijo_izq->columna,
                "La condicion del operador ternario debe ser Booleano o Entero."
            );
        }

        if (tipo_condicion == TIPO_ERROR ||
            tipo_verdadero == TIPO_ERROR ||
            tipo_falso == TIPO_ERROR)
        {
            node->resolved_type = TIPO_ERROR;
            break;
        }

        if (tipo_verdadero == tipo_falso)
            node->resolved_type = tipo_verdadero;
        else if ((tipo_verdadero == INT && tipo_falso == FLOAT) ||
                 (tipo_verdadero == FLOAT && tipo_falso == INT))
            node->resolved_type = FLOAT;
        else if ((tipo_verdadero == BOOL && tipo_falso == INT) ||
                 (tipo_verdadero == INT && tipo_falso == BOOL))
            node->resolved_type = INT;
        else
        {
            reportar_error_semantico(
                node->renglon,
                node->columna,
                "Las expresiones del operador ternario deben tener tipos compatibles: %s y %s.",
                tipoDatoToString(tipo_verdadero),
                tipoDatoToString(tipo_falso)
            );
            node->resolved_type = TIPO_ERROR;
        }

        break;
    }

    case AST_IDENTIFICADOR:
    {
        if (node->tipo_nombre && node->ir_result_name)
        {
            const char *receiver_name = node->receptor_nombre
                                            ? node->receptor_nombre
                                            : node->tipo_nombre;
            EntradaSimbolo *receiver = buscar_simbolo(ambito_actual, receiver_name);
            ASTNode *type = receiver && receiver->tipo_nombre
                                ? buscar_tipo(raiz_semantica->hijo_izq, receiver->tipo_nombre)
                                : NULL;
            const char *field_owner = NULL;
            ASTNode *field = type ? buscar_campo_tipo(type, node->ir_result_name,
                                                      &field_owner, 0)
                                  : NULL;
            if (type && !type->es_clase)
            {
                free(node->receptor_nombre);
                node->receptor_nombre = NULL;
            }
            if (field && !miembro_accesible(field->acceso_miembro, field_owner))
                reportar_error_semantico(node->renglon, node->columna,
                                         "El campo '%s' no es accesible desde esta clase.",
                                         node->ir_result_name);
            if (receiver && receiver->tipo_nombre)
            {
                free(node->tipo_nombre);
                node->tipo_nombre = strdup(receiver->tipo_nombre);
            }
        }

        EntradaSimbolo *entrada = buscar_simbolo(ambito_actual, node->valor.nombre_id);
        if (entrada == NULL)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "Uso de variable/constante no declarada: '%s'", node->valor.nombre_id);
            node->resolved_type = TIPO_ERROR;
        }
        else
        {
            node->resolved_type = entrada->tipo;
            if (!node->tipo_nombre && entrada->tipo_nombre)
                node->tipo_nombre = strdup(entrada->tipo_nombre);
        }
        break;
    }
    case AST_LITERAL_ENTERO:
        node->resolved_type = INT;
        node->tipoconstante = CONSTANTE_LITERAL;
        break;
    case AST_LITERAL_FLOTANTE:
        node->resolved_type = FLOAT;
        node->tipoconstante = CONSTANTE_LITERAL;
        break;
    case AST_LITERAL_CADENA:
        node->resolved_type = STRING;
        node->tipoconstante = CONSTANTE_LITERAL;
        break;
    case AST_LITERAL_BOOLEANO:
        node->resolved_type = BOOL;
        node->tipoconstante = CONSTANTE_LITERAL;
        break;
    case AST_NUEVO:
    {
        ASTNode *type = buscar_tipo(raiz_semantica->hijo_izq, node->tipo_nombre);
        if (!type || !type->es_clase)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "Nuevo requiere una clase declarada: '%s'.",
                                     node->tipo_nombre ? node->tipo_nombre : "?");
            node->resolved_type = TIPO_ERROR;
        }
        else
        {
            node->resolved_type = OTRO;
        }
        break;
    }

    case AST_ROMPER_STMT:
        if (profundidad_loop <= 0)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "'Romper' debe ser utilizado dentro de un bucle.");
        }
        break;

    case AST_CONTINUAR_STMT:
        if (profundidad_loop <= 0)
        {
            reportar_error_semantico(node->renglon, node->columna,
                                     "'Continuar' debe ser utilizado dentro de un bucle.");
        }
        break;
    default:
        if (node->hijo_izq != NULL)
        {
            visit_ast_semantic(node->hijo_izq);
        }
        if (node->hijo_der != NULL)
        {
            visit_ast_semantic(node->hijo_der);
        }
        if (node->siguiente_hermano != NULL)
        {
            visit_ast_semantic(node->siguiente_hermano);
        }
        break;
    }
}