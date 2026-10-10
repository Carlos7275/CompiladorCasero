#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
TEST_SOURCE="$ROOT_DIR/tests/memoria_extensa.mx"
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mx-memory-tests.XXXXXX")

if [ -z "${COMPILER:-}" ]; then
  case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
      COMPILER="$BUILD_DIR/bin/compilador.exe"
      ;;
    *)
      COMPILER="$BUILD_DIR/bin/compilador"
      ;;
  esac
fi

cleanup() {
  rm -f "$TEMP_DIR/input.txt" "$TEMP_DIR/memoria_extensa" \
    "$TEMP_DIR/memoria_extensa.asm" "$TEMP_DIR/memoria_extensa.o" \
    "$TEMP_DIR/memoria_extensa.obj" "$TEMP_DIR/memoria_extensa.exe" \
    "$TEMP_DIR/compiler-leaks.log" "$TEMP_DIR/program-leaks.log" \
    "$TEMP_DIR/program-output.log"
  rmdir "$TEMP_DIR" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM

cd "$ROOT_DIR"

if ! command -v cmake >/dev/null 2>&1; then
  echo "Error: CMake no esta disponible." >&2
  exit 1
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release --target compilador

if [ ! -x "$COMPILER" ]; then
  echo "Error: no se encontro el compilador ejecutable: $COMPILER" >&2
  exit 1
fi

if command -v leaks >/dev/null 2>&1; then
  LEAK_TOOL=macos
elif command -v valgrind >/dev/null 2>&1; then
  LEAK_TOOL=valgrind
elif command -v drmemory >/dev/null 2>&1; then
  LEAK_TOOL=drmemory
else
  echo "Error: instala leaks (macOS), Valgrind (Linux) o Dr. Memory (Windows)." >&2
  exit 1
fi

cat > "$TEMP_DIR/input.txt" <<'EOF'
12345
19.55
1
texto con espacios
EOF

export MX_STRESS_ENV=stress-activo
OUTPUT="$TEMP_DIR/memoria_extensa"
COMPILER_LOG="$TEMP_DIR/compiler-leaks.log"
PROGRAM_LOG="$TEMP_DIR/program-leaks.log"

echo "Analizando fugas del compilador..."
if [ "$LEAK_TOOL" = macos ]; then
  leaks --atExit -- "$COMPILER" "$TEST_SOURCE" "$OUTPUT" -no-run >"$COMPILER_LOG" 2>&1
  if ! grep -F "0 leaks for 0 total leaked bytes" "$COMPILER_LOG" >/dev/null; then
    cat "$COMPILER_LOG"
    echo "Error: el analisis encontro fugas en el compilador." >&2
    exit 1
  fi
else
  if [ "$LEAK_TOOL" = valgrind ]; then
    valgrind --leak-check=full --show-leak-kinds=definite,indirect \
      --errors-for-leak-kinds=definite,indirect --error-exitcode=1 \
      --log-file="$COMPILER_LOG" "$COMPILER" "$TEST_SOURCE" "$OUTPUT" -no-run
    if ! grep -F " All heap blocks were freed -- no leaks are possible" "$COMPILER_LOG" >/dev/null; then
      cat "$COMPILER_LOG"
      echo "Error: el analisis encontro fugas definitivas en el compilador." >&2
      exit 1
    fi
  else
    drmemory -batch -exit_code_if_errors 1 -- \
      "$COMPILER" "$TEST_SOURCE" "$OUTPUT" -no-run >"$COMPILER_LOG" 2>&1
    if ! grep -F "[OK] build:" "$COMPILER_LOG" >/dev/null; then
      cat "$COMPILER_LOG"
      echo "Error: el compilador no completo bajo Dr. Memory." >&2
      exit 1
    fi
  fi
fi

PROGRAM="$OUTPUT"
if [ ! -x "$PROGRAM" ] && [ -x "$OUTPUT.exe" ]; then
  PROGRAM="$OUTPUT.exe"
fi
if [ ! -x "$PROGRAM" ]; then
  echo "Error: no se genero el ejecutable de stress." >&2
  exit 1
fi

echo "Analizando fugas del programa generado..."
if [ "$LEAK_TOOL" = macos ]; then
  (cd "$TEMP_DIR" && leaks --atExit -- "$PROGRAM" <input.txt) >"$PROGRAM_LOG" 2>&1
  if ! grep -F "0 leaks for 0 total leaked bytes" "$PROGRAM_LOG" >/dev/null; then
    cat "$PROGRAM_LOG"
    echo "Error: el analisis encontro fugas en el programa generado." >&2
    exit 1
  fi
else
  if [ "$LEAK_TOOL" = valgrind ]; then
    valgrind --leak-check=full --show-leak-kinds=definite,indirect \
      --errors-for-leak-kinds=definite,indirect --error-exitcode=1 \
      --log-file="$PROGRAM_LOG" "$PROGRAM" <"$TEMP_DIR/input.txt" \
      >"$TEMP_DIR/program-output.log" 2>&1
    if ! grep -F " All heap blocks were freed -- no leaks are possible" "$PROGRAM_LOG" >/dev/null; then
      cat "$PROGRAM_LOG"
      echo "Error: el analisis encontro fugas definitivas en el programa generado." >&2
      exit 1
    fi
  else
    (cd "$TEMP_DIR" && drmemory -batch -exit_code_if_errors 1 -- \
      "$PROGRAM" <input.txt) >"$PROGRAM_LOG" 2>&1
  fi
fi

if [ "$LEAK_TOOL" = valgrind ]; then
  PROGRAM_OUTPUT_LOG="$TEMP_DIR/program-output.log"
else
  PROGRAM_OUTPUT_LOG="$PROGRAM_LOG"
fi
if ! grep -F "MEMORIA_EXTENSA_OK:" "$PROGRAM_OUTPUT_LOG" >/dev/null; then
  cat "$PROGRAM_OUTPUT_LOG"
  echo "Error: el programa de stress no llego al final correctamente." >&2
  exit 1
fi

echo "OK: compilador y programa generado completaron el stress sin fugas detectadas."
