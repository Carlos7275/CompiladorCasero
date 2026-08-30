$ErrorActionPreference = 'Stop'

function Ensure-NASM {
    if (Get-Command nasm -ErrorAction SilentlyContinue) {
        return
    }

    Write-Host "NASM no está instalado. Intentando instalarlo..."

    if (Get-Command winget -ErrorAction SilentlyContinue) {
        winget install --id NASM.NASM -e --accept-source-agreements --accept-package-agreements
    }
    elseif (Get-Command choco -ErrorAction SilentlyContinue) {
        choco install nasm -y
    }
    else {
        throw "No se encontró un instalador de NASM para Windows. Instala NASM manualmente e intenta de nuevo."
    }

    if (-not (Get-Command nasm -ErrorAction SilentlyContinue)) {
        throw "NASM no quedó disponible después de la instalación."
    }
}

Ensure-NASM

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = if ($args.Count -gt 0) { $args[0] } else { Join-Path $root 'build' }

cmake -S $root -B $buildDir -DCMAKE_BUILD_TYPE=Release
cmake --build $buildDir --config Release

if (Test-Path (Join-Path $buildDir 'bin\compilador.exe')) {
    Copy-Item (Join-Path $buildDir 'bin\compilador.exe') (Join-Path $root 'compilador.exe') -Force
}

Write-Host ""
Write-Host "Build completado. Ejecutable disponible en: $buildDir\bin\compilador.exe"
Write-Host "Ejecución rápida: .\compilador.exe <archivo.mx>"
