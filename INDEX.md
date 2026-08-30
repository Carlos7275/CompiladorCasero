# Índice de Documentación

## 📚 Documentos Principales

### Para Empezar
- **[README.md](README.md)** ⭐ COMIENCE AQUÍ
  - Introducción general del proyecto
  - Características principales
  - Cómo compilar (rápido)
  - Cómo usar (ejemplos básicos)
  - Estructura del proyecto
  - Información de contacto

### Para Desarrolladores
- **[ARCHITECTURE.md](ARCHITECTURE.md)** 🏗️
  - Diseño interno del compilador
  - 7 componentes principales explicados
  - Flujo de compilación completo
  - Estructuras de datos
  - Decisiones de diseño
  - Complejidad computacional
  - Roadmap futuro

### Para Construir el Proyecto
- **[BUILDING.md](BUILDING.md)** 🔧
  - Requisitos previos (macOS, Linux, Windows)
  - 3 métodos de compilación
  - Instalación de dependencias
  - Opciones de CMake
  - Compilación cruzada
  - Solución de problemas

### Para Pruebas
- **[TESTING.md](TESTING.md)** 🧪
  - Descripción de 13 programas de prueba
  - Cómo ejecutar pruebas
  - Interpretación de resultados
  - Debugging manual
  - Crear nuevas pruebas
  - Matriz de características

### Información de Versión
- **[VERSION.md](VERSION.md)** 📋
  - Información de v1.0.0
  - Características implementadas
  - Cambios desde beta
  - Requisitos mínimos
  - Rendimiento y estadísticas
  - Seguridad
  - Problemas conocidos

### Historial de Cambios
- **[CHANGELOG.md](CHANGELOG.md)** 📝
  - Historial completo de versiones
  - Cambios en 1.0.0
  - Versiones anteriores
  - Roadmap futuro
  - Notas de compatibilidad

---

## 🎯 Guías Rápidas por Caso de Uso

### "Quiero compilar el proyecto"
1. Leer: [README.md](README.md#compilación) (sección Compilación)
2. Ejecutar: `./build.sh`
3. Verificar: `./build/bin/compilador --version`

### "Quiero entender cómo funciona internamente"
1. Leer: [ARCHITECTURE.md](ARCHITECTURE.md)
2. Revisar: [src/main.c](src/main.c) (líneas 1-50)
3. Seguir el pipeline: ARCHITECTURE.md sección "Flujo de Compilación"

### "Quiero ejecutar las pruebas"
1. Compilar: `./build.sh`
2. Ejecutar: `./run_tests.sh`
3. Revisar: [TESTING.md](TESTING.md) para detalles

### "Quiero compilar un programa Mx"
1. Ver: [README.md](README.md#ejemplo-de-programa-mx)
2. Crear: archivo `miprograma.mx`
3. Compilar: `./build/bin/compilador miprograma.mx salida`
4. Ejecutar: `./salida`

### "Tengo un error de compilación"
1. Revisar: [BUILDING.md](BUILDING.md#solución-de-problemas)
2. Verificar: Requisitos en [BUILDING.md](BUILDING.md#requisitos-previos)
3. Reinstalar: `./build.sh`

### "Mi programa .mx no compila"
1. Ver: [TESTING.md](TESTING.md#descripción-de-programas-de-prueba)
2. Comparar: Con alguno de los ejemplos
3. Revisar: Sintaxis válida en [README.md](README.md#declaraciones-soportadas)

### "Quiero reportar un bug"
1. Reproducir: Con ejemplo mínimo
2. Revisar: [TESTING.md](TESTING.md#debugging-manual)
3. Consultar: [CHANGELOG.md](CHANGELOG.md) - problemas conocidos
4. Proporcionar: Programa .mx + salida esperada vs actual

---

## 📖 Mapa de Conceptos

```
CompiladorCasero v1.0.0
│
├─ Documentación
│  ├─ README.md ..................... Introducción (punto de entrada)
│  ├─ ARCHITECTURE.md ............... Diseño interno (para devs)
│  ├─ BUILDING.md ................... Compilación (todos)
│  ├─ TESTING.md .................... Pruebas (QA/devs)
│  ├─ VERSION.md .................... Versión (info técnica)
│  ├─ CHANGELOG.md .................. Historia de cambios
│  └─ INDEX.md (este archivo) ....... Navegación
│
├─ Código Fuente
│  └─ src/
│     ├─ main.c ..................... Punto de entrada
│     ├─ lexer/lexer.c .............. Análisis léxico
│     ├─ parser/parser.c ............ Análisis sintáctico
│     ├─ semantic/semantic.c ........ Análisis semántico
│     ├─ codegen/codegen.c .......... Generación de código
│     ├─ symbols/symbols.c .......... Tabla de símbolos
│     ├─ errors/errors.c ............ Manejo de errores
│     ├─ utils/ ..................... Utilidades
│     └─ include/ ................... Headers públicos
│
├─ Scripts
│  ├─ build.sh ...................... Compilación (Unix)
│  ├─ build.ps1 ..................... Compilación (Windows)
│  └─ run_tests.sh .................. Suite de pruebas
│
└─ Tests
   └─ tests/
      ├─ adivinanza.mx .............. Juego interactivo
      ├─ calculadora.mx ............ Operaciones
      ├─ ejemplo1.mx ............... I/O básico
      ├─ fibonacci.mx .............. Secuencia
      ├─ paridad.mx ................ Pares/impares
      ├─ programa.mx ............... Factorial
      ├─ prueba.mx ................. 10 tests
      ├─ prueba_integral.mx ........ 13 tests completos
      ├─ suma.mx ................... Acumulador
      ├─ tablas.mx ................. Multiplicación
      ├─ temperatura.mx ............ Área
      ├─ condicionales.mx .......... If/else
      └─ palindromo.mx ............. Strings
```

---

## 🔍 Búsqueda Rápida

### Por Componente del Compilador
| Componente | Archivo | Sección Doc |
|---|---|---|
| Lexer (Tokenizador) | src/lexer/lexer.c | ARCHITECTURE.md §2 |
| Parser (Sintaxis) | src/parser/parser.c | ARCHITECTURE.md §3 |
| Semántica | src/semantic/semantic.c | ARCHITECTURE.md §4 |
| IR (Intermedio) | src/codegen/codegen.c (parte 1) | ARCHITECTURE.md §5 |
| Codegen (ASM) | src/codegen/codegen.c (parte 2) | ARCHITECTURE.md §6 |
| Símbolos | src/symbols/symbols.c | ARCHITECTURE.md §7 |
| Main | src/main.c | ARCHITECTURE.md §8 |

### Por Característica del Lenguaje
| Característica | Documentado en | Test |
|---|---|---|
| Variables | README.md, ARCHITECTURE.md | prueba.mx |
| Operadores | README.md §Operadores | prueba.mx |
| Condicionales | README.md §Declaraciones | condicionales.mx |
| Bucles | README.md §Declaraciones | programa.mx |
| I/O | README.md §Declaraciones | ejemplo1.mx |
| Strings | ARCHITECTURE.md §6 | prueba_integral.mx |
| Floats | TESTING.md §Temperatura | temperatura.mx |

### Por Plataforma
| Plataforma | Info en |
|---|---|
| macOS | README.md, BUILDING.md §macOS, ARCHITECTURE.md |
| Linux | BUILDING.md §Linux |
| Windows | BUILDING.md §Windows, ARCHITECTURE.md |

---

## 📞 Flujos de Soporte

### ❓ Pregunta Común
**P**: ¿Por dónde empiezo?  
**R**: [README.md](README.md) - sección "Uso Básico"

### ❓ Pregunta Común
**P**: ¿Cómo compilo en Windows?  
**R**: [BUILDING.md](BUILDING.md) - sección "Método 1: Script PowerShell"

### ❓ Pregunta Común
**P**: ¿Qué características soporta Mx?  
**R**: [README.md](README.md) - sección "Operadores Soportados"

### ❓ Pregunta Común
**P**: ¿Cómo reporte un bug?  
**R**: [TESTING.md](TESTING.md) - sección "Debugging Manual"

### ❓ Pregunta Común
**P**: ¿Cuál es la arquitectura interna?  
**R**: [ARCHITECTURE.md](ARCHITECTURE.md) - lea secciones 1-7

---

## 📊 Estadísticas de Documentación

| Documento | Líneas | Secciones | Ejemplos |
|---|---|---|---|
| README.md | 350 | 15 | 10 |
| ARCHITECTURE.md | 500 | 20 | 15 |
| BUILDING.md | 400 | 18 | 30 |
| TESTING.md | 350 | 15 | 20 |
| VERSION.md | 300 | 12 | 5 |
| CHANGELOG.md | 200 | 8 | 0 |
| **TOTAL** | **~2100** | **~88** | **~80** |

---

## ✅ Documentación Completa para v1.0

- ✅ Introducción ([README.md](README.md))
- ✅ Arquitectura ([ARCHITECTURE.md](ARCHITECTURE.md))
- ✅ Compilación ([BUILDING.md](BUILDING.md))
- ✅ Pruebas ([TESTING.md](TESTING.md))
- ✅ Versión ([VERSION.md](VERSION.md))
- ✅ Cambios ([CHANGELOG.md](CHANGELOG.md))
- ✅ Índice ([INDEX.md](INDEX.md) - este archivo)

---

## 🚀 Próximos Pasos

1. **Para Usar**: Leer [README.md](README.md)
2. **Para Desarrollar**: Leer [ARCHITECTURE.md](ARCHITECTURE.md)
3. **Para Compilar**: Seguir [BUILDING.md](BUILDING.md)
4. **Para Probar**: Ejecutar [./run_tests.sh](run_tests.sh)
5. **Para Entender Cambios**: Revisar [CHANGELOG.md](CHANGELOG.md)

---

**Versión**: 1.0.0  
**Última actualización**: 30 Agosto 2026  
**Documentación Completa**: ✅ SÍ  
**Estado**: PRODUCCIÓN
