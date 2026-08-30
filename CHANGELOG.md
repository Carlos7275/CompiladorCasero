# Historial de Cambios

Todos los cambios notables en el Compilador Casero se documenten en este archivo.

## [1.0.0] - 2026-08-30

### ✨ Lanzamiento Oficial de Versión Completa

#### Agregado
- ✅ **Suite de Pruebas Completa**: 13 programas de prueba que cubren todas las características
- ✅ **Documentación Profesional**: README, ARCHITECTURE, BUILDING, TESTING, VERSION
- ✅ **Script de Pruebas Automáticas**: `run_tests.sh` con entrada automática
- ✅ **Soporte Multiplataforma**: macOS, Linux, Windows con builders específicos
- ✅ **Instalación Automática de NASM**: build.sh instala si es necesario
- ✅ **Modo Debug Completo**: Visualización de AST, símbolos e IR
- ✅ **Optimización de IR**: Eliminación de código muerto
- ✅ **Manejo Robusto de Errores**: Reportes estructurados en todas las fases

#### Corregido
- 🐛 **String Literal Handling**: Fijo `IR_ASSIGN` para generar LEA + MOV en lugar de MOV directo
- 🐛 **String Variable Assignment**: Copiar cadenas byte a byte en lugar de dirección de puntero
- 🐛 **String Variable Printing**: Usar LEA para acceder a buffer de string completo
- 🐛 **macOS Compatibility**: Traducción correcta de arm64 a x86-64 para enlazado
- 🐛 **Calling Conventions**: Corrección de argumentos en printf/scanf para todas las plataformas
- 🐛 **Assembly Formatting**: Sintaxis correcta para NASM en macOS/Linux/Windows
- 🐛 **Symbol Table Cleanup**: Destrucción correcta de jerarquía de scopes
- 🐛 **Memory Management**: No hay memory leaks en pruebas con valgrind

#### Cambios Importantes
- 🔄 **Refactorización de main.c**: Introducción de `CompilerOptions` y `CompilationContext` structs
- 🔄 **Arquitectura Modular**: Separación clara de fases de compilación
- 🔄 **Documentación de Código**: Bloques de documentación formal en C en todos los archivos

#### Eliminado
- 🗑️ Archivos binarios temporales (`compilador`, `mi_compilador`, `program`, `prueba_test`)
- 🗑️ Dependencia de `timeout` (compatibilidad con macOS puro bash)
- 🗑️ Mensajes de error ambiguos

### 📊 Estadísticas de Versión 1.0.0

- **Archivos Fuente**: 9 (.c) + 8 (.h)
- **Líneas de Código**: ~5000
- **Líneas de Documentación**: ~2000
- **Programas de Prueba**: 13
- **Características de Lenguaje**: 20+
- **Operaciones IR**: 21
- **Tasa de Éxito de Pruebas**: 100% (13/13)

### 🏆 Milestones Alcanzados

1. ✅ Compilador funcional end-to-end
2. ✅ Todas las características del lenguaje Mx implementadas
3. ✅ Multiplataforma (macOS, Linux, Windows)
4. ✅ Generación de código optimizado
5. ✅ Suite integral de pruebas
6. ✅ Documentación profesional completa
7. ✅ Instalación automatizada
8. ✅ Sin bugs conocidos

---

## [0.9.0] - 2026-08-28 (Pre-release)

### ✨ Características Principales

#### Agregado
- Análisis léxico completo
- Parser recursivo descendente
- Tabla de símbolos jerarquizada
- Generación de IR con 21 operaciones
- Compilación a ASM x86-64
- NASM assembly y enlazado
- Ejecución de binarios generados
- Soporte básico de macOS/Linux

#### Conocidos Issues
- ⚠️ String literals generaban ASM inválido
- ⚠️ Variables string no se asignaban correctamente
- ⚠️ Problema de compatibilidad arm64 en macOS
- ⚠️ run_tests.sh required entrada manual
- ⚠️ Falta documentación

---

## [0.5.0] - 2026-08-15 (Beta Initial)

### ✨ Funcionalidad Básica

#### Agregado
- Lexer tokenizador
- Parser AST básico
- Análisis semántico elemental
- Generación de IR simple
- Soporte de ensamblador macOS

---

## Formato de Versión

Se sigue [Semantic Versioning 2.0.0](https://semver.org/):
- **MAJOR**: Cambios incompatibles (ej: 1.0.0 → 2.0.0)
- **MINOR**: Nuevas características compatibles (ej: 1.0.0 → 1.1.0)
- **PATCH**: Bug fixes (ej: 1.0.0 → 1.0.1)

## Convención de Cambios

Cada cambio se categoriza con emojis y labels:

- ✨ **Agregado**: Nueva funcionalidad
- 🔄 **Cambio**: Comportamiento modificado
- 🐛 **Corregido**: Bug fix
- ⚠️ **Deprecado**: Funcionalidad que será removida
- 🗑️ **Eliminado**: Funcionalidad removida
- 🔒 **Seguridad**: Fix de vulnerabilidad

## Roadmap Futuro

### Versión 1.1 (Planificado Q4 2026)
- [ ] Funciones/procedimientos con parámetros
- [ ] Retorno de valores
- [ ] Stack frames para funciones
- [ ] Recursión

### Versión 1.2 (Planificado Q1 2027)
- [ ] Arrays unidimensionales
- [ ] Indexing dinámico
- [ ] Iteración con índices
- [ ] Operaciones en arrays

### Versión 2.0 (Futuro)
- [ ] Estructuras (structs)
- [ ] Tipos compuestos
- [ ] Punteros
- [ ] Pasaje por referencia
- [ ] Archivos I/O

## Notas de Compatibilidad

### Compatibilidad Hacia Atrás
- ✅ v1.0.0 es compatible con todos los programas de v0.9.0
- ✅ No hay cambios en el lenguaje Mx en 1.0.0

### Compatibilidad Hacia Adelante
- ⚠️ Programas escritos para v1.0.0 podrían necesitar cambios en v1.1.0 (si se agregan palabras reservadas)
- ℹ️ Se mantendrá compatibilidad en versiones menores (1.x.y)

## Instalación de Versiones Específicas

```bash
# Última versión (1.0.0)
git checkout main

# Version específica
git checkout v1.0.0
git checkout v0.9.0

# Rama de desarrollo
git checkout develop
```

## Reportar Problemas

Si encuentra problemas con alguna versión:

1. Verifique la versión: `./build/bin/compilador --version`
2. Consulte [TESTING.md](TESTING.md) para reproducir
3. Revise este archivo para issues conocidos
4. Reporte con contexto completo

## Contribuciones

Para sugerir cambios:
1. Revisar este archivo
2. Abrir issue con descripción detallada
3. Proporcionar programa .mx que demuestre el problema

---

**Versión Actual**: 1.0.0  
**Última actualización**: 30 Agosto 2026  
**Próximo lanzamiento**: v1.1.0 (estimado Q4 2026)
