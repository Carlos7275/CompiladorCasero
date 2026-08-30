# Guía de Compilación

## 📋 Resumen Rápido

```bash
# Opción más simple - Script automático
./build.sh

# Luego ejecutar pruebas
./run_tests.sh
```

## 🔧 Compilación Detallada

### Requisitos Previos

#### macOS
```bash
# Instalar Xcode Command Line Tools
xcode-select --install

# Instalar CMake (si no está)
brew install cmake

# NASM se instala automáticamente con build.sh
```

#### Ubuntu/Debian
```bash
# Instalar dependencias
sudo apt-get update
sudo apt-get install build-essential cmake nasm

# Verificar CMake
cmake --version
```

#### Windows (MSVC)
```cmd
# Descargar e instalar:
# 1. Visual Studio Community (con C++ workload)
# 2. CMake: https://cmake.org/download/
# 3. NASM: https://www.nasm.us/

# Verificar instalación
cmake --version
nasm -version
```

### Método 1: Script Automatizado (Recomendado)

#### En macOS/Linux:
```bash
chmod +x build.sh
./build.sh
```

Este script:
1. Detecta el sistema operativo
2. Instala NASM si es necesario
3. Crea el directorio build
4. Ejecuta CMake con presets optimizados
5. Compila el proyecto en Release
6. Verifica la compilación

#### En Windows:
```powershell
.\build.ps1
```

El script PowerShell:
1. Detecta si está en Visual Studio Command Prompt
2. Instala NASM con chocolatey si es necesario
3. Ejecuta CMake con generador MSVC
4. Compila en Release

### Método 2: CMake Manual

#### Paso 1: Crear directorio de build
```bash
mkdir -p build
cd build
```

#### Paso 2: Generar archivos de build
```bash
# Linux/macOS
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release

# Windows (con Visual Studio)
cmake -S .. -B . -G "Visual Studio 17 2022"
```

#### Paso 3: Compilar
```bash
# Linux/macOS
make -j$(nproc)

# Windows
cmake --build . --config Release

# O con make directo
cmake --build . --config Release -- -j4
```

#### Paso 4: Verificar compilación
```bash
./build/bin/compilador --version
```

### Método 3: Compilación Directa

Si no tienes CMake:

```bash
gcc -c src/lexer/lexer.c -o build/lexer.o -Isrc/include -std=c11 -O3
gcc -c src/parser/parser.c -o build/parser.o -Isrc/include -std=c11 -O3
gcc -c src/semantic/semantic.c -o build/semantic.o -Isrc/include -std=c11 -O3
gcc -c src/codegen/codegen.c -o build/codegen.o -Isrc/include -std=c11 -O3
gcc -c src/symbols/symbols.c -o build/symbols.o -Isrc/include -std=c11 -O3
gcc -c src/utils/file.c -o build/file.o -Isrc/include -std=c11 -O3
gcc -c src/utils/types.c -o build/types.o -Isrc/include -std=c11 -O3
gcc -c src/errors/errors.c -o build/errors.o -Isrc/include -std=c11 -O3
gcc -c src/main.c -o build/main.o -Isrc/include -std=c11 -O3

gcc build/*.o -o build/bin/compilador
```

## 🏗️ Configuración del Proyecto

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(CompiladorCasero C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Archivos fuente
set(SOURCES
    src/main.c
    src/lexer/lexer.c
    src/parser/parser.c
    src/semantic/semantic.c
    src/codegen/codegen.c
    src/symbols/symbols.c
    src/utils/file.c
    src/utils/types.c
    src/errors/errors.c
)

# Crear ejecutable
add_executable(compilador ${SOURCES})

# Include directories
target_include_directories(compilador PRIVATE src/include)

# Output directory
set_target_properties(compilador PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
```

## 🔍 Verificación de Compilación

### Prueba Básica
```bash
./build/bin/compilador --version
# Salida: Compilador Casero v1.0.0
```

### Compilar un Programa Ejemplo
```bash
./build/bin/compilador tests/ejemplo1.mx ejemplo_compilado -no-run
# Debe crear archivo 'ejemplo_compilado'
```

### Ver Información de Debug
```bash
./build/bin/compilador tests/ejemplo1.mx debug_test -debug | head -50
```

### Ejecutar Suite Completa
```bash
./run_tests.sh
```

## ⚙️ Opciones de Compilación

### Flags de CMake

```bash
# Release con optimizaciones
cmake -DCMAKE_BUILD_TYPE=Release

# Debug con símbolos
cmake -DCMAKE_BUILD_TYPE=Debug

# Con sanitizadores (detección de memory leaks)
cmake -DCMAKE_C_FLAGS="-fsanitize=address,undefined" \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"

# Con cobertura de código
cmake -DCMAKE_C_FLAGS="--coverage" \
      -DCMAKE_EXE_LINKER_FLAGS="--coverage"
```

### Compiler Flags Personalizados

```bash
# Con warnings estrictos
cmake -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic"

# Optimización máxima
cmake -DCMAKE_C_FLAGS="-O3 -march=native"

# Para debugging
cmake -DCMAKE_C_FLAGS="-g -O0"
```

## 📦 Instalación de Dependencias

### NASM (Netwide Assembler)

#### macOS
```bash
# Automático con build.sh
# Manual:
brew install nasm

# Verificar
nasm -version
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt-get install nasm

# Fedora/RHEL
sudo yum install nasm

# Verificar
nasm -version
```

#### Windows
```cmd
# Con chocolatey
choco install nasm

# O descargar de https://www.nasm.us/

# Verificar
nasm -version
```

### CMake

#### macOS
```bash
brew install cmake
```

#### Linux
```bash
sudo apt-get install cmake  # Debian/Ubuntu
sudo yum install cmake      # Fedora/RHEL
```

#### Windows
Descargar de: https://cmake.org/download/

## 🧹 Limpieza

### Limpiar Compilación Actual
```bash
rm -rf build/
```

### Limpiar Archivos Temporales
```bash
rm -f *.asm *.o *.obj *.exe
rm -rf /tmp/compilador_test_*
```

### Limpiar Todo
```bash
rm -rf build/ *.asm *.o *.obj *.exe
```

## 🚀 Compilación Cruzada

### Compilar para Windows en macOS

```bash
# Instalar MinGW
brew install mingw-w64

# Crear toolchain file (mingw-toolchain.cmake)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)

# Compilar
cmake -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -S . -B build_win
```

### Compilar para Linux en macOS

```bash
# Instalar cross-compiler
brew install gcc --with-target-architecture=x86_64-linux-gnu

# Compilar
cmake -DCMAKE_C_COMPILER=x86_64-linux-gnu-gcc \
      -DCMAKE_BUILD_TYPE=Release \
      -S . -B build_linux
```

## 🐛 Solución de Problemas

### "cmake: command not found"
```bash
# Instalar CMake
brew install cmake        # macOS
sudo apt install cmake    # Linux
# O descargar de cmake.org
```

### "nasm: command not found"
```bash
# Instalar NASM
brew install nasm                    # macOS
sudo apt install nasm                # Linux
# Windows: descargar de nasm.us
```

### Error de linker: "undefined reference to `printf`"
```bash
# Asegurar que las librerías estándar están enlazadas
# Esto generalmente se resuelve automáticamente
# Si persiste, verificar que gcc/clang están correctamente instalados
```

### "No such file or directory: compilador"
```bash
# Verificar que compiló correctamente
ls -la build/bin/compilador

# Si no existe, compilar nuevamente
cmake --build build --config Release
```

### Error: "symbol `str_N' not defined"
Esto significa que hay un problema con cadenas literales en el generador de código. Actualizar a la versión 1.0.0 que incluye la corrección.

## ✅ Checklist de Compilación Exitosa

- [ ] CMake 3.10+ instalado
- [ ] Compilador C (gcc/clang/MSVC) instalado
- [ ] NASM instalado
- [ ] `cmake -S . -B build` completa sin errores
- [ ] `cmake --build build --config Release` completa sin errores
- [ ] `./build/bin/compilador --version` funciona
- [ ] `./run_tests.sh` reporta 13/13 pruebas exitosas
- [ ] Opción de `-debug` y `-asm` funcionan

## 📚 Documentación Relacionada

- [README.md](README.md) - Introducción general
- [ARCHITECTURE.md](ARCHITECTURE.md) - Diseño interno
- [TESTING.md](TESTING.md) - Cómo ejecutar pruebas
- [VERSION.md](VERSION.md) - Información de versión

---

**Versión**: 1.0.0  
**Última actualización**: Agosto 2026
