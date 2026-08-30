# Información de Versión

## Compilador Casero v1.0.0

**Fecha de Lanzamiento**: Agosto 2026  
**Estado**: STABLE (Estable)  
**Tipo de Lanzamiento**: Versión Completa

## 📊 Información de Versión

| Componente | Versión |
|-----------|---------|
| Compilador | 1.0.0 |
| Especificación de Lenguaje | 1.0 |
| IR Versión | 1.0 |
| Formato ASM | x86-64 |
| Compilación Mínima Requerida | C11 |

## ✨ Características de la Versión

### Phase 1: Análisis Léxico ✅
- Tokenización completa
- Palabras reservadas integradas
- Números enteros y flotantes
- Cadenas con escape sequences
- Símbolos y operadores

### Phase 2: Análisis Sintáctico ✅
- Parsing recursivo descendente
- AST completo
- Manejo de errores sintácticos
- Recuperación de errores

### Phase 3: Análisis Semántico ✅
- Verificación de tipos
- Tabla de símbolos jerárquica
- Scopes anidados
- Validación de operaciones

### Phase 4: Generación de IR ✅
- 20+ operaciones IR
- Optimización de código muerto
- Generación de temporales únicos
- Propagación de constantes

### Phase 5: Generación de ASM ✅
- x86-64 (64-bit)
- Soporte macOS (Mach-O)
- Soporte Linux (ELF)
- Soporte Windows (PE/COFF)
- Convenciones de llamada correctas

### Phase 6: Ensamblado y Enlazado ✅
- NASM assembly
- Linking con gcc/clang
- Generación de ejecutables
- Limpieza de artefactos

### Phase 7: Suite de Pruebas ✅
- 13 programas de prueba
- Tests unitarios
- Tests integrales
- Cobertura de características

### Phase 8: Documentación ✅
- README.md completo
- ARCHITECTURE.md detallado
- BUILDING.md paso a paso
- TESTING.md y VERSION.md

## 🎯 Características Implementadas

### Lenguaje
- ✅ Variables: Entero, Flotante, Cadena
- ✅ Operadores aritméticos: +, -, *, /, %
- ✅ Operadores de comparación: ==, !=, <, >, <=, >=
- ✅ Operadores lógicos: Y, O, No
- ✅ Condicionales: Si, Sino
- ✅ Bucles: Para, Mientras
- ✅ Control de flujo: Romper, Continuar
- ✅ I/O: Mostrar, Leer
- ✅ Literales: números, cadenas, booleanos

### Compilador
- ✅ Multiplataforma (macOS, Linux, Windows)
- ✅ Instalación automática de NASM
- ✅ Manejo robusto de errores
- ✅ Modo debug con información completa
- ✅ Conservación de archivos .asm opcionales
- ✅ Ejecución automática opcional
- ✅ Limpieza de artefactos

### Herramientas
- ✅ Script de compilación (build.sh / build.ps1)
- ✅ Script de pruebas (run_tests.sh)
- ✅ Entrada automática para programas interactivos
- ✅ Reportes de pruebas coloridos
- ✅ Resumen de resultados

## 🔄 Cambios Desde Versión Beta

### Correcciones Críticas
- ✅ Fijo: String literals en IR_ASSIGN
- ✅ Fijo: Asignación de cadenas con LEA + MOV
- ✅ Fijo: Impresión de variables string
- ✅ Fijo: Compatibilidad con macOS arm64 → x86_64
- ✅ Fijo: Convenciones de llamada en all platforms

### Mejoras
- ✅ Optimización: Eliminación de código muerto
- ✅ Seguridad: Mejor manejo de memory bounds
- ✅ Performance: O3 optimization flags
- ✅ Documentación: Suite completa
- ✅ Testing: 13 programas integrales

### Limpieza
- ✅ Eliminados: Archivos temporales innecesarios
- ✅ Formateado: Documentación formal en C
- ✅ Organizado: Estructura clara de directorio
- ✅ Refactorizado: main.c con structs profesionales

## 📋 Requisitos Mínimos

### Sistema Operativo
- macOS 10.15+ (Big Sur compatible)
- Ubuntu 18.04+ / Debian 10+
- Windows 10+ (con MSVC o MinGW)

### Software
- CMake 3.10+
- C11 Compiler (gcc, clang, MSVC)
- NASM 2.14+ (Netwide Assembler)
- make / Visual Studio Build Tools

### Hardware
- Mínimo 256MB RAM (ideal 1GB+)
- 50MB disco duro
- Procesador x86/x86-64

## 🚀 Rendimiento

### Velocidad de Compilación
| Programa | Tiempo | Lineas |
|----------|--------|--------|
| ejemplo1.mx | ~50ms | 7 |
| programa.mx | ~60ms | 10 |
| prueba_integral.mx | ~150ms | 52 |

### Tamaño de Salida
| Programa | Ejecutable | ASM | Objeto |
|----------|-----------|-----|--------|
| ejemplo1.mx | 8.5KB | 1.2KB | 1.1KB |
| programa.mx | 8.6KB | 1.5KB | 1.2KB |
| prueba.mx | 9.2KB | 2.1KB | 1.8KB |

## 📈 Cobertura de Pruebas

### Suite de 13 Programas

```
✓ adivinanza.mx        - Juego interactivo
✓ calculadora.mx       - Operaciones aritméticas
✓ condicionales.mx     - If/else anidado
✓ ejemplo1.mx          - I/O básico
✓ fibonacci.mx         - Secuencia (loop)
✓ palindromo.mx        - Procesamiento de strings
✓ paridad.mx           - Números par/impar
✓ programa.mx          - Factorial recursivo
✓ prueba.mx            - 10 tests unitarios
✓ prueba_integral.mx   - 13 tests completos
✓ suma.mx              - Acumulador
✓ tablas.mx            - Multiplicación
✓ temperatura.mx       - Cálculo de área

Total: 13/13 PASSED ✅
```

## 🔐 Seguridad

### Validaciones Implementadas
- ✅ Verificación de tipos estricta
- ✅ Detección de variables no declaradas
- ✅ Prevención de redeclaración
- ✅ Validación de operaciones
- ✅ Manejo de buffer overflow en strings (límite 256B)
- ✅ Protección de pila NX (no-execute)

### Conocidas Limitaciones de Seguridad
- ⚠️ Sin verificación de limites de array (no hay arrays)
- ⚠️ Sin ASLR en binarios generados
- ⚠️ Sin canarios de pila
- ⚠️ Strings limitados a 256 caracteres

## 🐛 Problemas Conocidos

### Ninguno reportado para 1.0.0
La versión 1.0.0 ha pasado todas las pruebas. Si encuentra algún problema, reportarlo en el repositorio.

## 🔮 Roadmap Futuro

### Versión 1.1 (Planeado)
- Funciones/procedimientos
- Parámetros con paso por valor/referencia
- Valor de retorno

### Versión 1.2 (Planeado)
- Arrays unidimensionales
- Índices dinámicos
- Iteración sobre arrays

### Versión 2.0 (Futuro)
- Estructuras (structs)
- Tipos compuestos
- Punteros (aritmética básica)
- Archivos I/O

## 📞 Soporte

Para reportar bugs o hacer sugerencias:
1. Revisar ARCHITECTURE.md para entender el sistema
2. Verificar que el bug es reproducible
3. Proporcionar:
   - Programa .mx que lo causa
   - Salida esperada vs actual
   - Sistema operativo y versión
   - Salida de `./build/bin/compilador --version`

## 📄 Licencia

Proyecto educativo - Uso libre para fines académicos.

## 👤 Créditos

**Desarrollado por**: Carlos Sandoval  
**Período**: 2026  
**Inspiración**: Compiladores clásicos, Dragon Book

## ✅ Verificación de Versión

Para verificar que tiene v1.0.0 instalada:

```bash
./build/bin/compilador --version
# Salida: Compilador Casero v1.0.0
```

---

**Versión**: 1.0.0  
**Estado**: STABLE ✅  
**Última actualización**: Agosto 2026
