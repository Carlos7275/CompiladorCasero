# Compilador Casero v1.0.0

Un compilador de propósito educativo para el lenguaje **Mx**, compilado completamente desde cero en C11 con arquitectura modular y soporte multiplataforma.

## 🚀 Características Principales

- **Lenguaje Mx Personalizado**: Sintaxis clara y expresiva basada en pseudocódigo estructurado
- **Arquitectura Modular**: Separación clara de responsabilidades (lexer → parser → semántica → código)
- **Multiplataforma**: Compilación y ejecución en macOS, Linux y Windows
- **x86-64 Assembly**: Genera código ASM optimizado para arquitectura x86-64
- **Gestión de Errores Robusta**: Reportes de error estructurados en todas las fases
- **Sistema de Símbolos Jerarquizado**: Soporte para scopes anidados y resolución de variables
- **Optimización de IR**: Eliminación de código muerto en el código intermedio

## 📋 Requisitos

### Mínimos
- **CMake** 3.10+
- **C11 Compiler**: GCC, Clang o MSVC
- **NASM** (Netwide Assembler)
- **make** o plataforma de build nativa

### Sistema Operativo
- macOS 10.15+
- Ubuntu 18.04+ / Debian 10+
- Windows 10+ (con MSVC o MinGW)

## 🔧 Compilación

### Opción 1: Script Automatizado
```bash
./build.sh                    # Compila e instala NASM si es necesario
```

### Opción 2: CMake Manual
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/bin/compilador --version
```

### Opción 3: PowerShell en Windows
```powershell
.\build.ps1
```

## 📖 Uso Básico

### Compilar un programa Mx
```bash
./build/bin/compilador programa.mx programa_compilado
```

### Opciones de Compilación
```bash
./build/bin/compilador archivo.mx [salida] [opciones]

Opciones:
  -asm          conserva el archivo .asm generado
  -debug        muestra AST, tabla de símbolos, IR y ASM
  -no-run       compila sin ejecutar el ejecutable
  -version      muestra la versión del compilador
  -help         muestra esta ayuda
```

### Ejemplo Completo
```bash
# Compilar solo
./build/bin/compilador tests/ejemplo1.mx mi_programa -no-run

# Compilar con debug
./build/bin/compilador tests/ejemplo1.mx mi_programa -debug

# Conservar el ASM generado
./build/bin/compilador tests/ejemplo1.mx mi_programa -asm
```

## 🧪 Ejecución de Pruebas

```bash
# Ejecutar todas las pruebas (13 programas)
./run_tests.sh

# Ejecutar prueba individual
./build/bin/compilador tests/ejemplo1.mx -no-run
```

### Programas de Prueba Incluidos
- `adivinanza.mx` - Juego interactivo de adivinanza
- `calculadora.mx` - Calculadora básica
- `condicionales.mx` - Demostración de if/else
- `ejemplo1.mx` - Lectura y escritura I/O
- `fibonacci.mx` - Secuencia de Fibonacci
- `palindromo.mx` - Validación de palíndromos
- `paridad.mx` - Detección de números pares/impares
- `programa.mx` - Cálculo de factorial
- `prueba.mx` - Suite de 10 pruebas unitarias
- `prueba_integral.mx` - Suite completa con 13 características
- `suma.mx` - Suma acumulativa
- `tablas.mx` - Tabla de multiplicación
- `temperatura.mx` - Cálculo de área y conversión

## 💡 Ejemplo de Programa Mx

```mx
// Cálculo del factorial
Entero n = 5;
Entero fact = 1;
Entero i;

Para(i = 1; i <= n; i = i + 1) {
    fact = fact * i;
}

Mostrar("Factorial de ", n, " es ", fact, "\n");
```

## 📚 Documentación Completa

- [ARCHITECTURE.md](./ARCHITECTURE.md) - Arquitectura interna del compilador
- [BUILDING.md](./BUILDING.md) - Guía detallada de compilación
- [TESTING.md](./TESTING.md) - Protocolo de pruebas
- [VERSION.md](./VERSION.md) - Información de la versión
- [CHANGELOG.md](./CHANGELOG.md) - Historial de cambios

## 🏗️ Estructura del Proyecto

```
.
├── src/
│   ├── main.c              # Punto de entrada principal
│   ├── lexer/              # Análisis léxico
│   ├── parser/             # Análisis sintáctico
│   ├── semantic/           # Análisis semántico
│   ├── codegen/            # Generación de código
│   ├── symbols/            # Tabla de símbolos
│   ├── utils/              # Utilidades (I/O, tipos)
│   ├── errors/             # Manejo de errores
│   └── include/            # Headers públicos
├── tests/                  # Programas de prueba (13 ejemplos)
├── build/                  # Artefactos de compilación
├── CMakeLists.txt          # Configuración CMake
├── build.sh               # Script de compilación (Unix)
├── build.ps1              # Script de compilación (Windows)
└── run_tests.sh           # Script de ejecución de pruebas
```

## 🔄 Pipeline de Compilación

```
programa.mx
    ↓
[LEXER] → Tokens
    ↓
[PARSER] → AST (Abstract Syntax Tree)
    ↓
[SEMANTIC ANALYSIS] → Validación de tipos
    ↓
[IR GENERATION] → Código intermedio (cuádruples)
    ↓
[CODE GENERATION] → ASM x86-64
    ↓
[NASM] → Código objeto (.o/.obj)
    ↓
[LINKER] → Ejecutable final
    ↓
[RUNTIME] → Salida
```

## 🐛 Soporte para Tipos de Datos

| Tipo | Descripción | Rango |
|------|-------------|-------|
| `Entero` | Entero de 64 bits | -2^63 a 2^63-1 |
| `Flotante` | Punto flotante (64-bit) | IEEE 754 |
| `Cadena` | Cadena de caracteres | Hasta 256 caracteres |
| `Booleano` | Valor lógico (0/1) | 0 o 1 |

## 🔐 Operadores Soportados

### Aritméticos
- `+` Suma
- `-` Resta
- `*` Multiplicación
- `/` División entera
- `%` Módulo

### Comparación
- `==` Igual
- `!=` No igual
- `<` Menor que
- `>` Mayor que
- `<=` Menor o igual
- `>=` Mayor o igual

### Lógicos
- `Y` AND lógico
- `O` OR lógico
- `No` NOT lógico

## 📝 Declaraciones Soportadas

- `Entero`, `Flotante`, `Cadena` - Declaración de variables
- `Si`, `Sino` - Condicionales
- `Para` - Bucle for
- `Mientras` - Bucle while
- `Romper` - Break
- `Continuar` - Continue
- `Mostrar` - Salida (printf)
- `Leer` - Entrada (scanf)

## 🛠️ Mantenimiento

### Limpiar Artefactos de Compilación
```bash
rm -rf build/
rm -f *.asm *.o *.obj *.exe
```

### Regenerar Compilación
```bash
./build.sh
```

## 📄 Licencia

Proyecto educativo - Uso libre para fines académicos.

## 👤 Autor

Carlos Sandoval - 2026

## 🤝 Contribuciones

Este es un proyecto educativo. Se aceptan sugerencias y mejoras.

## ✅ Estado de Proyecto

**Versión 1.0.0** - STABLE

- ✅ Fase 1: Análisis léxico y sintáctico
- ✅ Fase 2: Análisis semántico
- ✅ Fase 3: Generación de código IR
- ✅ Fase 4: Compilación a ASM x86-64
- ✅ Fase 5: Enlazado a ejecutables
- ✅ Fase 6: Suite de pruebas (13 programas)
- ✅ Fase 7: Documentación completa
- ✅ Fase 8: Optimización y limpieza

---

**Última actualización**: Agosto 2026
