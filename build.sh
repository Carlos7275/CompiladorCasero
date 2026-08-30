#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="${1:-$ROOT_DIR/build}"

install_nasm() {
  if command -v nasm >/dev/null 2>&1; then
    return 0
  fi

  OS_NAME=$(uname -s)
  echo "NASM no está instalado. Intentando instalarlo..."

  case "$OS_NAME" in
    Darwin)
      if command -v brew >/dev/null 2>&1; then
        brew install nasm
      else
        echo "Homebrew no está instalado. Instálalo primero: https://brew.sh" >&2
        exit 1
      fi
      ;;
    Linux)
      if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update && sudo apt-get install -y nasm
      elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y nasm
      elif command -v yum >/dev/null 2>&1; then
        sudo yum install -y nasm
      elif command -v zypper >/dev/null 2>&1; then
        sudo zypper install -y nasm
      elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -Sy --noconfirm nasm
      else
        echo "No se encontró un gestor de paquetes para Linux." >&2
        exit 1
      fi
      ;;
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
      if command -v winget >/dev/null 2>&1; then
        winget install --id NASM.NASM -e --accept-source-agreements --accept-package-agreements
      elif command -v choco >/dev/null 2>&1; then
        choco install nasm -y
      elif command -v pacman >/dev/null 2>&1; then
        pacman -Sy --noconfirm nasm
      else
        echo "No se encontró un instalador de NASM para Windows." >&2
        exit 1
      fi
      ;;
    *)
      echo "Sistema operativo no soportado para instalar NASM: $OS_NAME" >&2
      exit 1
      ;;
  esac
}

install_nasm

OS_NAME=$(uname -s)
case "$OS_NAME" in
  Darwin)
    PRESET="macos"
    ;;
  Linux)
    PRESET="linux"
    ;;
  MINGW*|MSYS*|CYGWIN*|Windows_NT)
    PRESET="windows"
    ;;
  *)
    PRESET="default"
    ;;
 esac

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release

if [ -x "$BUILD_DIR/bin/compilador" ]; then
  cp "$BUILD_DIR/bin/compilador" "$ROOT_DIR/compilador"
  chmod +x "$ROOT_DIR/compilador"
fi

printf '\nBuild completado. Ejecutable disponible en: %s/bin/compilador\n' "$BUILD_DIR"
printf 'Ejecución rápida: %s/compilador <archivo.mx>\n' "$ROOT_DIR"
