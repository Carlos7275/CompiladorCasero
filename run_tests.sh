#!/bin/bash

# ========================================
# Script de Validación Integral del Compilador
# Compila y ejecuta todos los programas de ejemplo
# Proporciona entrada automática para programas interactivos
# ========================================

COMPILADOR="./build/bin/compilador"
TESTS_DIR="tests"
PASSED=0
FAILED=0
TOTAL=0

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Mapeo de programas con entrada automática
declare -A INPUT_MAP
INPUT_MAP["adivinanza"]="42\n" # Número secreto
INPUT_MAP["palindromo"]="hola\n"
INPUT_MAP["calculadora"]="10\n5\n"
INPUT_MAP["condicionales"]="25\n"

echo -e "${BLUE}=====================================${NC}"
echo -e "${BLUE}VALIDACION INTEGRAL DEL COMPILADOR${NC}"
echo -e "${BLUE}Versión 1.0 - Suite de Pruebas${NC}"
echo -e "${BLUE}=====================================${NC}\n"

# Verificar que el compilador existe
if [ ! -f "$COMPILADOR" ]; then
    echo -e "${RED}ERROR: Compilador no encontrado en $COMPILADOR${NC}"
    echo "Compilando primero..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
    cmake --build build --config Release > /dev/null 2>&1
    
    if [ ! -f "$COMPILADOR" ]; then
        echo -e "${RED}No se pudo compilar el compilador.${NC}"
        exit 1
    fi
fi

# Función para ejecutar un programa con entrada automática si es necesario
run_with_input() {
    local binary=$1
    local filename=$2
    
    if [ -n "${INPUT_MAP[$filename]}" ]; then
        # Programa requiere entrada automática
        echo -e "${INPUT_MAP[$filename]}" | "$binary" 2>&1
        return $?
    else
        # Programa sin entrada interactiva
        "$binary" 2>&1
        return $?
    fi
}

# Iterar sobre todos los archivos .mx en el directorio de tests
for test_file in "$TESTS_DIR"/*.mx; do
    if [ ! -f "$test_file" ]; then
        continue
    fi
    
    TOTAL=$((TOTAL + 1))
    filename=$(basename "$test_file" .mx)
    
    echo -e "${YELLOW}─────────────────────────────────────${NC}"
    echo -e "${BLUE}Prueba $TOTAL: $filename${NC}"
    echo -e "${YELLOW}─────────────────────────────────────${NC}"
    
    # Compilar el programa
    output_binary="/tmp/compilador_test_${filename}"
    compile_output=$("$COMPILADOR" "$test_file" "$output_binary" -no-run 2>&1)
    compile_status=$?
    
    if [ $compile_status -ne 0 ]; then
        echo -e "${RED}✗ Error de compilación:${NC}"
        echo "$compile_output" | head -20
        FAILED=$((FAILED + 1))
        echo ""
        continue
    fi
    
    echo -e "${GREEN}✓ Compilación exitosa${NC}"
    
    # Ejecutar el programa con entrada automática si es necesario
    if [ -f "$output_binary" ]; then
        echo -e "${BLUE}Output:${NC}"
        echo "─────────────────────────────────────"
        run_with_input "$output_binary" "$filename" | head -50
        execution_status=$?
        echo "─────────────────────────────────────"
        
        if [ $execution_status -eq 0 ]; then
            echo -e "${GREEN}✓ Ejecución exitosa${NC}"
            PASSED=$((PASSED + 1))
        else
            echo -e "${RED}✗ Error de ejecución (código: $execution_status)${NC}"
            FAILED=$((FAILED + 1))
        fi
        
        # Limpiar
        rm -f "$output_binary" "${output_binary}.asm" "${output_binary}.o" "${output_binary}.obj"
    else
        echo -e "${RED}✗ Ejecutable no generado${NC}"
        FAILED=$((FAILED + 1))
    fi
    
    echo ""
done

# Resumen final
echo -e "${BLUE}=====================================${NC}"
echo -e "${BLUE}RESUMEN DE RESULTADOS${NC}"
echo -e "${BLUE}=====================================${NC}"
echo -e "Total de pruebas:    ${YELLOW}$TOTAL${NC}"
echo -e "Exitosas:            ${GREEN}$PASSED${NC}"
echo -e "Fallidas:            ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}✓ TODAS LAS PRUEBAS PASARON CORRECTAMENTE${NC}"
    exit 0
else
    echo -e "\n${RED}✗ Algunas pruebas fallaron${NC}"
    exit 1
fi
