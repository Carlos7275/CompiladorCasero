# Arquitectura del Compilador Casero

## 📋 Vista General

El compilador Casero implementa una arquitectura clásica de múltiples fases con separación clara de responsabilidades. Cada módulo es independiente y se comunica a través de estructuras de datos bien definidas.

## 🏗️ Componentes Principales

### 1. Análisis Léxico (Lexer)

**Archivo**: `src/lexer/lexer.c`

Responsabilidad: Convertir el flujo de caracteres en una secuencia de tokens.

```
Entrada: "Entero x = 10;"
         ↓
Salida: [KEYWORD(Entero), ID(x), OP(=), NUM(10), OP(;)]
```

**Funciones clave**:
- `EsID()` - Reconoce identificadores y palabras reservadas
- `EsNum()` - Reconoce números enteros y flotantes
- `EsCadena()` - Reconoce cadenas literales con escape sequences
- `EsSimbolo()` - Reconoce operadores y símbolos especiales
- `EsPalabraReservadaConTipo()` - Mapea palabras reservadas a tipos

**Tipos de Token**:
```c
enum TipoToken {
    TIPO_ENTERO, TIPO_FLOTANTE, TIPO_CADENA, TIPO_BOOLEANO,
    KW_SI, KW_SINO, KW_PARA, KW_MIENTRAS,
    KW_ROMPER, KW_CONTINUAR,
    KW_LEER, KW_MOSTRAR,
    OP_ASIGNACION, OP_SUMA, OP_RESTA, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LTE, OP_GTE,
    OP_PAREN_OPEN, OP_PAREN_CLOSE,
    OP_LLAVE_OPEN, OP_LLAVE_CLOSE,
    OP_COMA, OP_PUNTO_COMA,
    ID_TOKEN, NUM_TOKEN, CADENA_TOKEN,
    FIN_ARCHIVO
};
```

### 2. Análisis Sintáctico (Parser)

**Archivo**: `src/parser/parser.c`

Responsabilidad: Construir un Árbol de Sintaxis Abstracta (AST) a partir de tokens.

```
Tokens: [KEYWORD(Entero), ID(x), OP(=), NUM(10), OP(;)]
        ↓
AST:    Program
        └─ Declaration
           ├─ type: ENTERO
           ├─ name: "x"
           └─ init: Literal(10)
```

**Estructura del AST**:
```c
typedef struct nodo {
    TipoNodo tipo;
    char *nombre;
    Tipo *tipo_dato;
    struct nodo *izquierda;
    struct nodo *derecha;
    struct nodo *siguiente;
    // ... campos adicionales según tipo
} ASTNode;
```

**Funciones principales**:
- `parsePrograma()` - Punto de entrada, parsea el programa completo
- `parseListaSentencias()` - Parsea una secuencia de sentencias
- `parseSentenciaODeclaracion()` - Parsea declaraciones y sentencias
- `parseBloque()` - Parsea un bloque entre llaves

**Estrategia**:
- Análisis descendente recursivo (Recursive Descent Parser)
- Precedencia de operadores manejada implícitamente en la recursión
- Manejo de errores con recuperación

### 3. Análisis Semántico

**Archivo**: `src/semantic/semantic.c`

Responsabilidad: Validar el AST y verificar restricciones semánticas.

```
AST + Tabla de Símbolos
        ↓
Validación:
  - Tipos de datos consistentes
  - Variables declaradas antes de usar
  - Operaciones válidas para tipos
  - Scopes correctos
        ↓
Salida: AST validado + Tabla de símbolos poblada
```

**Verificaciones**:
- Declaración antes de uso
- Compatibilidad de tipos en asignaciones
- Operaciones válidas (ej: no multiplicar strings)
- Redeclaración de variables
- Existencia de etiquetas (break/continue en loops)

**Funciones**:
- `verificar_programa()` - Entrada, recorre el AST
- `verificar_asignacion()` - Valida compatibilidad de tipos
- `verificar_expresion_aritmetica()` - Valida operandos numéricos
- `reportar_error_semantico()` - Registra errores encontrados

### 4. Generación de Código Intermedio (IR)

**Archivo**: `src/codegen/codegen.c` (Parte 1)

Responsabilidad: Traducir el AST a código intermedio (Quadruples/Triples).

```
AST + Tabla de Símbolos
        ↓
Generación de Cuádruples:
  IR_ASSIGN: (dest, source, _, result)
  IR_ADD:    (arg1, arg2, _, result)
  IR_LABEL:  (_, _, _, label_name)
  IR_JUMP:   (_, _, _, label_name)
  IR_COND_JUMP: (condition, _, _, label)
        ↓
Optimización:
  - Eliminación de código muerto
  - Propagación de constantes
        ↓
Salida: Array de Quadruples optimizado
```

**Estructura de Cuádrupla**:
```c
typedef struct {
    IROperation op;      // Tipo de operación
    char *arg1, *arg2;   // Argumentos
    char *result;        // Resultado
} Quadruple;
```

**Operaciones IR Soportadas**:
```c
enum IROperation {
    IR_ASSIGN,      // Asignación
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,  // Aritméticas
    IR_LT, IR_GT, IR_LE, IR_GE, IR_EQ, IR_NE, // Comparación
    IR_AND, IR_OR, IR_NOT,                     // Lógicas
    IR_PRINT,       // Salida
    IR_READ,        // Entrada
    IR_LABEL,       // Etiqueta
    IR_JUMP,        // Salto incondicional
    IR_COND_JUMP,   // Salto condicional
    IR_HALT         // Fin
};
```

**Generación de temporales**:
- Variables temporales: `t0`, `t1`, ... `tn`
- Etiquetas: `L0`, `L1`, ... `Ln`
- Nomenclatura única garantizada

### 5. Generación de Código Assembly

**Archivo**: `src/codegen/codegen.c` (Parte 2)

Responsabilidad: Traducir el IR a ensamblador x86-64 (NASM).

```
IR Optimizado + Tabla de Símbolos
        ↓
Selección de Código:
  - IR_ASSIGN → mov rax, ...; mov [var], rax
  - IR_ADD → add rax, rbx
  - IR_LABEL → label:
  - IR_COND_JUMP → cmp; je/jne/jl/jg
        ↓
Asignación de Registros:
  - rax, rbx, rcx, rdx, rsi, rdi (de uso general)
  - Convención de llamada del SO (cdecl en Linux/Mac, fastcall en Windows)
        ↓
Generación de Secciones:
  - .data: Constantes y strings literales
  - .bss: Variables sin inicializar
  - .text: Código ejecutable
        ↓
Salida: Archivo .asm en formato NASM
```

**Plataformas Soportadas**:

| Plataforma | Sección de Datos | Convención |
|------------|------------------|-----------|
| macOS | `section __DATA,__data` | x86-64 System V |
| Linux | `section .data` | x86-64 System V |
| Windows | `section .data` | x64 Microsoft |

**Handling de Tipos**:
- **Entero**: 64-bit (resq 1)
- **Flotante**: 64-bit double IEEE 754
- **Cadena**: Buffer 256 bytes (resb 256) + manejo de punteros
- **Booleano**: Entero (0 o 1)

### 6. Tabla de Símbolos

**Archivo**: `src/symbols/symbols.c`

Responsabilidad: Mantener información sobre variables, tipos y scopes.

```
Estructura Jerárquica:
┌─ Scope Global
│  ├─ Symbol: "x" → tipo ENTERO, dirección [rel x]
│  ├─ Symbol: "y" → tipo CADENA, dirección [rel y]
│  └─ Scope Local (función/bloque)
│     ├─ Symbol: "temp" → tipo ENTERO
│     └─ Symbol: "resultado" → tipo FLOTANTE
└─ Otro scope...
```

**Estructura de Entrada**:
```c
typedef struct {
    char *nombre;
    Tipo tipo;
    int linea, columna;
    // Información adicional según contexto
} EntradaSimbolo;
```

**Operaciones**:
- `crear_tabla_simbolos()` - Crea nuevo scope
- `agregar_simbolo()` - Registra variable/símbolo
- `buscar_simbolo()` - Busca en scope actual y padres
- `buscar_simbolo_en_ambito_actual()` - Búsqueda local
- `destruir_jerarquia_tablas_simbolos()` - Libera memoria

### 7. Manejo de Errores

**Archivo**: `src/errors/errors.c`

Responsabilidad: Reportar errores de forma clara y estructurada.

**Tipos de Error**:
1. **Léxicos**: Caracteres inválidos, números malformados
2. **Sintácticos**: Estructura incorrecta
3. **Semánticos**: Tipos incompatibles, variable no declarada
4. **Tiempo de Ejecución**: División por cero, segmentation fault

**Funciones**:
```c
void log_error(const char *component, const char *format, ...);
void log_warning(const char *component, const char *format, ...);
void log_info(const char *component, const char *format, ...);
void log_success(const char *component, const char *format, ...);
```

**Formato de Salida**:
```
[ERROR] component: Mensaje detallado con contexto
[WARNING] component: Advertencia no fatal
[OK] component: Mensaje de éxito
```

## 🔄 Flujo de Compilación Completo

```
programa.mx
    ↓
┌─────────────────────────────────────────┐
│ main.c - Orquestación Principal         │
│  • parse_arguments() - Procesa opciones │
│  • parse_source_file() - Dispara lexer  │
│  • analyze_semantics() - Valida AST     │
│  • generate_code() - Crea IR y ASM      │
│  • assemble_code() - NASM → .o/.obj     │
│  • link_executable() - Enlaza binary    │
│  • run_executable() - Ejecuta (opt.)    │
│  • cleanup_intermediates() - Limpia     │
└─────────────────────────────────────────┘
    ↓
Ejecutable final

Archivos Intermedios:
├── programa.mx ──→ programa.asm (ASM x86-64)
├──          ──→ programa.o (Objeto, Linux/macOS)
└──          ──→ programa.obj (Objeto, Windows)
```

## 💾 Estructuras de Datos Principales

### AST Node
```c
struct nodo {
    TipoNodo tipo;          // Qué tipo de nodo es
    char *nombre;           // Identificador (var, función, etc)
    Tipo *tipo_dato;        // Tipo de dato
    struct nodo *izquierda; // Subárbol izquierdo
    struct nodo *derecha;   // Subárbol derecho
    struct nodo *siguiente; // Siguiente nodo en lista
};
```

### Symbol Table Entry
```c
struct {
    char *nombre;
    Tipo tipo;
    int linea, columna;
    // Puede incluir más info según necesidad
};
```

### IR Quadruple
```c
typedef struct {
    IROperation op;
    char *arg1;      // Operando izquierdo
    char *arg2;      // Operando derecho
    char *result;    // Destino
} Quadruple;
```

## 🎯 Decisiones de Diseño

### 1. Recursión Descendente para Parsing
- **Ventaja**: Fácil de escribir y entender
- **Desventaja**: No maneja gramáticas ambiguas
- **Justificación**: El lenguaje Mx tiene gramática LL(1)

### 2. IR como Representación Intermedia
- **Ventaja**: Independiente de plataforma, fácil de optimizar
- **Desventaja**: Paso adicional en la compilación
- **Justificación**: Mejor separación de fases, posibilita optimizaciones futuras

### 3. Tabla de Símbolos Jerárquica
- **Ventaja**: Soporta scopes anidados correctamente
- **Desventaja**: Más compleja que tabla plana
- **Justificación**: Necesario para lenguajes con bloques/funciones

### 4. Assembly Generado a Mano
- **Ventaja**: Control total, optimizaciones específicas
- **Desventaja**: Código más largo, mantenimiento complejo
- **Justificación**: Educativo, enseña arquitectura x86-64

## 📊 Complejidad Computacional

| Fase | Complejidad | Memoria |
|------|-------------|---------|
| Lexer | O(n) | O(n) |
| Parser | O(n) | O(n) |
| Semantic | O(n) | O(n) |
| IR Gen | O(n) | O(n) |
| Codegen | O(n) | O(n) |
| Total | **O(n)** | **O(n)** |

Donde `n` es el tamaño del archivo fuente.

## 🔮 Extensiones Futuras

1. **Optimizaciones de Código**:
   - Eliminación más agresiva de código muerto
   - Propagación de constantes
   - Fold de expresiones

2. **Nuevas Características del Lenguaje**:
   - Funciones/procedimientos
   - Arrays/structs
   - Punteros

3. **Mejor Generación de Código**:
   - Asignación de registros más inteligente
   - Vectorización SIMD

4. **Herramientas de Depuración**:
   - Generación de símbolos de debug (DWARF)
   - GDB/LLDB integration

## 📚 Referencias

- Aho et al. "Compilers: Principles, Techniques, and Tools" (Dragon Book)
- x86-64 Application Binary Interface (AMD64)
- NASM Documentation (Netwide Assembler)
- System V AMD64 ABI

---

**Versión**: 1.0.0  
**Última actualización**: Agosto 2026
