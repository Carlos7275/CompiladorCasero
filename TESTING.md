# Guía de Pruebas

## 🧪 Suite de Pruebas Integral

El compilador Casero incluye una suite que ejecuta todos los programas `.mx`
de `tests/` en Linux/macOS y Windows.

## 🚀 Ejecución Rápida

### Ejecutar Todas las Pruebas
```bash
./run_tests.sh
```

En Windows PowerShell:
```powershell
.\run_tests.ps1
```

### Stress de memoria del lenguaje

En macOS, ejecuta `sh ./run_memory_tests.sh`; el script usa `leaks` para
inspeccionar por separado el proceso del compilador y el programa Mx generado.
En Linux ejecuta el mismo script con Valgrind instalado. En Windows usa
`.\run_memory_tests.ps1` con Dr. Memory disponible en `PATH`. Los runners
fallan si no encuentran su herramienta de análisis, si el stress no llega al
marcador final o si el analizador reporta errores/fugas.

`tests/memoria_extensa.mx` estresa funciones normales y recursivas, bucles,
arrays enteros y flotantes, tipos personalizados, conversiones booleano/entero,
entrada tipada, funciones matemáticas y de cadenas, variables de entorno,
apertura/escritura/lectura/cierre/eliminación repetida de archivos y excepciones.
No inicia sockets, HTTP/TLS ni el debugger interactivo: esos casos requieren
servicios externos, dependencias opcionales o entrada de control propia.

Salida esperada:
```
=====================================
VALIDACION INTEGRAL DEL COMPILADOR
Versión 1.0 - Suite de Pruebas
=====================================

─────────────────────────────────────
Prueba 1: adivinanza
─────────────────────────────────────
✓ Compilación exitosa
Output:
─────────────────────────────────────
[output del programa]
─────────────────────────────────────
✓ Ejecución exitosa

[... resto de los programas de prueba ...]

=====================================
RESUMEN DE RESULTADOS
=====================================
Total de pruebas:    [cantidad de archivos .mx]
Exitosas:            [cantidad de archivos .mx]
Fallidas:            0

✓ TODAS LAS PRUEBAS PASARON CORRECTAMENTE
```

## 📋 Descripción de Programas de Prueba

### 1. adivinanza.mx - Juego de Adivinanza

**Propósito**: Demostrar bucles interactivos y condicionales.

**Características probadas**:
- ✅ Bucle Mientras
- ✅ Condicionales Si/Sino
- ✅ Lectura de entrada (Leer)
- ✅ Salida formateada (Mostrar)
- ✅ Contador de intentos

**Entrada**: Número entre 1 y 100 (proporciona automáticamente 42)
**Líneas**: ~25

### sockets_chat.mx - Regresion TCP local

**Propósito**: Verificar intercambios cliente/servidor TCP bidireccionales y
el cierre repetido de recursos de red.

**Características probadas**:
- ✅ Listener en puerto efímero y conexión por loopback
- ✅ Intercambio de mensajes en ambos sentidos
- ✅ Verificación del contenido recibido
- ✅ 16 rondas consecutivas cerrando cliente, conexión y listener

En macOS, puede revisarse memoria dinámica al salir con:

```bash
leaks --atExit -- /tmp/compilador_test_sockets_chat
```

El chat interactivo para dos terminales está en
`examples/sockets_chat_servidor.mx` y `examples/sockets_chat_cliente.mx`.
Usa `SocketRecibirLinea` para delimitar mensajes en el flujo TCP. Consulta la
sección “Chat TCP interactivo de 16 mensajes” del README para compilarlo e
iniciarlo.

### 2. calculadora.mx - Operaciones Aritméticas

**Propósito**: Verificar operadores aritméticos básicos.

**Características probadas**:
- ✅ Suma (+)
- ✅ Resta (-)
- ✅ Multiplicación (*)
- ✅ División (/)
- ✅ Módulo (%)

**Entrada**: Números para operandos
**Líneas**: ~20

### 3. condicionales.mx - Sistema de Clasificación

**Propósito**: Validar condicionales anidados (Si/Sino).

**Características probadas**:
- ✅ Condicionales anidados
- ✅ Rangos de valores
- ✅ Strings literales
- ✅ Salida con múltiples Mostrar

**Entrada**: Calificación numérica
**Líneas**: ~30

### 4. ejemplo1.mx - I/O Básico

**Propósito**: Ejemplo introductorio de lectura y escritura.

**Características probadas**:
- ✅ Declaración de variables
- ✅ Lectura (Leer)
- ✅ Escritura (Mostrar)
- ✅ Operaciones simples

**Entrada**: Dos números
**Líneas**: ~7 (más simple)

### 5. fibonacci.mx - Secuencia Matemática

**Propósito**: Validar bucles con acumuladores.

**Características probadas**:
- ✅ Bucle Para
- ✅ Incremento de variables
- ✅ Generación de secuencia
- ✅ Salida iterativa

**Líneas**: ~15

### 6. palindromo.mx - Validación de Strings

**Propósito**: Verificar procesamiento de cadenas.

**Características probadas**:
- ✅ Lectura de strings (Leer)
- ✅ Comparación de strings
- ✅ Condicionales con strings

**Entrada**: Palabra para verificar si es palíndromo
**Líneas**: ~20

### 7. paridad.mx - Números Pares/Impares

**Propósito**: Validar operador módulo (%).

**Características probadas**:
- ✅ Operador módulo (%)
- ✅ Condicionales simples
- ✅ Bucle Para
- ✅ Salida condicional

**Líneas**: ~15

### 8. programa.mx - Factorial Recursivo (Simulado)

**Propósito**: Cálculo iterativo de factorial (simula recursión con loop).

**Características probadas**:
- ✅ Multiplicación acumulativa
- ✅ Bucle controlado
- ✅ Variable acumulador

**Líneas**: ~15

### 9. prueba.mx - Suite de 10 Pruebas Unitarias

**Propósito**: Validar 10 características fundamentales.

**Características probadas**:
1. ✅ Declaración de variables
2. ✅ Asignación simple
3. ✅ Suma
4. ✅ Resta
5. ✅ Multiplicación
6. ✅ División
7. ✅ Operador módulo
8. ✅ Condicional Si
9. ✅ Bucle Para
10. ✅ Bucle Mientras

**Líneas**: ~40

### 10. prueba_integral.mx - Suite Completa de 13 Pruebas

**Propósito**: Validación integral de todas las características.

**Características probadas**:
1. ✅ Declaración de variables
2. ✅ Strings literales
3. ✅ Operaciones aritméticas
4. ✅ Condicionales Si/Sino
5. ✅ Bucles Para
6. ✅ Bucles Mientras
7. ✅ Operadores de comparación
8. ✅ Acumuladores
9. ✅ Factorial
10. ✅ Identificación de pares
11. ✅ Condicionales anidados
12. ✅ Múltiples operaciones
13. ✅ Estructura del programa

**Líneas**: ~52 (más completo)

### 11. suma.mx - Suma de 1 a N

**Propósito**: Bucle acumulativo simple.

**Características probadas**:
- ✅ Bucle Para
- ✅ Acumulador
- ✅ Salida de resultado

**Líneas**: ~10

### 12. tablas.mx - Tabla de Multiplicación

**Propósito**: Bucles anidados.

**Características probadas**:
- ✅ Bucle Para anidado
- ✅ Multiplicación en loop
- ✅ Formateo de salida

**Líneas**: ~12

### 13. temperatura.mx - Cálculo de Área

**Propósito**: Operaciones aritméticas con decimales.

**Características probadas**:
- ✅ Tipo Flotante
- ✅ Multiplicación de decimales
- ✅ Mostrar decimales

**Líneas**: ~10

## 🧬 Ejecución Individual

### Compilar sin Ejecutar
```bash
./build/bin/compilador tests/ejemplo1.mx mi_programa -no-run
```

### Compilar con Debug
```bash
./build/bin/compilador tests/ejemplo1.mx debug_test -debug
# Muestra: AST, Tabla de Símbolos, IR, ASM
```

### Depuración Interactiva
```bash
./build/bin/compilador tests/operadores_nuevos.mx debug_test -debugger
# s: siguiente sentencia, c: continuar, b LINEA: breakpoint, q: salir
```

### Compilar y Conservar ASM
```bash
./build/bin/compilador tests/ejemplo1.mx test_result -asm
# Genera: test_result, test_result.asm, test_result.o
```

### Diagnosticos para IDE
```bash
./build/bin/compilador tests/ejemplo1.mx --diagnostics=json
```

La opcion valida lexer, parser y semantica, y devuelve un objeto JSON en
`stdout`; sus rangos usan indices desde cero. En exito, `diagnostics` es una
lista vacia.

### Compilar y Ejecutar
```bash
./build/bin/compilador tests/ejemplo1.mx test_result
# Compila y ejecuta automáticamente
```

## 📊 Matriz de Características

| Característica | Tests que lo verifican |
|---|---|
| Declaración de variables | prueba.mx, prueba_integral.mx |
| Asignación simple | todos |
| Enteros | todos |
| Flotantes | temperatura.mx |
| Strings | ejemplo1.mx, condicionales.mx, palindromo.mx |
| Suma | prueba.mx, suma.mx, todos |
| Resta | prueba.mx, condicionales.mx |
| Multiplicación | prueba.mx, programa.mx, tablas.mx |
| División | prueba.mx, calculadora.mx |
| Módulo | prueba.mx, paridad.mx |
| Comparación == | prueba_integral.mx |
| Comparación != | prueba_integral.mx |
| Comparación < | todos con Si |
| Comparación > | todos con Si |
| Si/Sino | casi todos |
| Para | prueba.mx, fibonacci.mx, tablas.mx, etc |
| Mientras | adivinanza.mx, prueba.mx |
| Romper | (implícito en bucles) |
| Mostrar | todos |
| Leer | ejemplo1.mx, adivinanza.mx, etc |
| Archivos | archivos_basico.mx |
| Bitwise y asignaciones compuestas | operadores_nuevos.mx |
| Entero a Booleano y condiciones | operadores_nuevos.mx |
| Intentar/Capturar/Lanzar | operadores_nuevos.mx |
| Variable de captura, ObtenerMensaje y excepciones anidadas | excepciones_modernas.mx |
| Sockets UDP, timeout y puerto efimero | sockets_avanzados.mx |

## 🔍 Interpretación de Resultados

### ✓ Prueba Exitosa
```
✓ Compilación exitosa
✓ Ejecución exitosa
```

Significa que:
1. El programa compiló sin errores
2. El ejecutable se generó
3. El programa ejecutó sin crashes (exit code 0)

### ✗ Fallo de Compilación
```
✗ Error de compilación:
[mensajes de error]
```

Revisar:
1. Sintaxis del programa .mx
2. Tipos de datos compatibles
3. Variables declaradas

### ✗ Fallo de Ejecución
```
✗ Error de ejecución (código: X)
```

Significa que el programa compiló pero falló en runtime.

## 🛠️ Debugging Manual

### Ver ASM Generado
```bash
./build/bin/compilador tests/ejemplo1.mx test -no-run -asm
cat test.asm | less
```

### Ver AST y Símbolos
```bash
./build/bin/compilador tests/ejemplo1.mx test -debug 2>&1 | head -100
```

### Ejecutar Manualmente
```bash
# Compilar
./build/bin/compilador tests/ejemplo1.mx test -no-run

# Ejecutar directamente
./test
```

### Usar gdb (si está disponible)
```bash
gdb -args ./test
(gdb) run
(gdb) backtrace
```

## 📈 Estadísticas de Pruebas

### Resumen Actual
- **Total de programas**: 13
- **Líneas totales de código Mx**: ~300
- **Características cubiertas**: 20+
- **Tasa de éxito**: 100% (13/13)

### Por Categoría
| Categoría | Programas | Cobertura |
|---|---|---|
| I/O | 3 | 100% |
| Condicionales | 5 | 100% |
| Bucles | 7 | 100% |
| Operadores | 6 | 100% |
| Strings | 3 | 100% |
| Floats | 1 | 100% |

## 🔧 Crear Nuevas Pruebas

### Plantilla Básica

```mx
// Descripción de la prueba
// Probada: Características X, Y, Z

Entero resultado;

// Tu código aquí
resultado = 42;

Mostrar("Resultado: ", resultado, "\n");
```

### Convención de Nombres
- `nombreCaracteristica.mx` - Una característica
- `prueba_caracteristica.mx` - Conjunto de pruebas

### Agregar a run_tests.sh

El script detecta automáticamente archivos `.mx` en `tests/` y los ejecuta.

## ✅ Checklist de Pruebas

- [ ] Compilador compila sin errores
- [ ] ./build/bin/compilador --version funciona
- [ ] ./run_tests.sh ejecuta todas las pruebas
- [ ] Resultado: 13/13 exitosas
- [ ] Ningún mensaje de error durante compilación
- [ ] Archivos temporales se limpian
- [ ] Salida es legible y coherente

## 📚 Documentación Relacionada

- [README.md](README.md) - Introducción general
- [ARCHITECTURE.md](ARCHITECTURE.md) - Diseño interno
- [BUILDING.md](BUILDING.md) - Compilación
- [VERSION.md](VERSION.md) - Información de versión

---

**Versión**: 1.0.0  
**Última actualización**: Agosto 2026
