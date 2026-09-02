$ErrorActionPreference = 'Continue'

# ========================================
# Validacion integral del compilador en Windows
# Compila y ejecuta todos los programas de tests.
# ========================================

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    $compiler = Join-Path $root 'build\bin\Release\compilador.exe'
    $testsDir = Join-Path $root 'tests'
    $tempDir = Join-Path $root 'build\test-output'
    $passed = 0
    $failed = 0
    $total = 0

    $inputMap = @{
        adivinanza = @('42')
        palindromo = @('hola')
        calculadora = @('10', '5')
        condicionales = @('25')
        ejemplo1 = @('Carlos', '30', '70.5')
        fibonacci = @('10')
        paridad = @('7')
        programa = @('5')
        suma = @('5')
        tablas = @('5')
        temperatura = @('10', '2')
    }

    Write-Host '=====================================' -ForegroundColor Cyan
    Write-Host 'VALIDACION INTEGRAL DEL COMPILADOR' -ForegroundColor Cyan
    Write-Host 'Version 1.0 - Suite de Pruebas Windows' -ForegroundColor Cyan
    Write-Host '=====================================' -ForegroundColor Cyan
    Write-Host ''

    if (-not (Test-Path $compiler)) {
        Write-Host "ERROR: Compilador no encontrado en $compiler" -ForegroundColor Red
        Write-Host 'Compilando primero...'

        & cmake -S $root -B (Join-Path $root 'build') -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) {
            throw 'La configuracion de CMake fallo.'
        }

        & cmake --build (Join-Path $root 'build') --config Release --target compilador
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $compiler)) {
            throw 'No se pudo compilar el compilador.'
        }
    }

    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    $testFiles = Get-ChildItem -Path $testsDir -Filter '*.mx' -File | Sort-Object Name
    foreach ($testFile in $testFiles) {
        $total++
        $filename = $testFile.BaseName
        $outputBase = Join-Path $tempDir "compilador_test_$filename"
        $outputExe = "$outputBase.exe"

        Remove-Item "$outputBase.*" -Force -ErrorAction SilentlyContinue

        Write-Host '-------------------------------------' -ForegroundColor DarkYellow
        Write-Host "Prueba ${total}: $filename" -ForegroundColor Cyan
        Write-Host '-------------------------------------' -ForegroundColor DarkYellow

        $compileOutput = @(& $compiler $testFile.FullName $outputBase '-no-run' 2>&1)
        $compileStatus = $LASTEXITCODE
        if ($compileStatus -ne 0) {
            Write-Host 'X Error de compilacion:' -ForegroundColor Red
            $compileOutput | Select-Object -First 20 | ForEach-Object { Write-Host $_ }
            $failed++
            Write-Host ''
            continue
        }

        Write-Host 'OK Compilacion exitosa' -ForegroundColor Green
        if (-not (Test-Path $outputExe)) {
            Write-Host 'X Ejecutable no generado' -ForegroundColor Red
            $failed++
            Write-Host ''
            continue
        }

        Write-Host 'Output:' -ForegroundColor Cyan
        Write-Host '-------------------------------------'
        if ($inputMap.ContainsKey($filename)) {
            $executionOutput = @($inputMap[$filename] | & $outputExe 2>&1)
        }
        else {
            $executionOutput = @(& $outputExe 2>&1)
        }
        $executionStatus = $LASTEXITCODE
        $executionOutput | Select-Object -First 50 | ForEach-Object { Write-Host $_ }
        Write-Host '-------------------------------------'

        if ($executionStatus -eq 0) {
            Write-Host 'OK Ejecucion exitosa' -ForegroundColor Green
            $passed++
        }
        else {
            Write-Host "X Error de ejecucion (codigo: $executionStatus)" -ForegroundColor Red
            $failed++
        }

        Remove-Item "$outputBase.*" -Force -ErrorAction SilentlyContinue
        Write-Host ''
    }

    Write-Host '=====================================' -ForegroundColor Cyan
    Write-Host 'RESUMEN DE RESULTADOS' -ForegroundColor Cyan
    Write-Host '=====================================' -ForegroundColor Cyan
    Write-Host "Total de pruebas:    $total" -ForegroundColor Yellow
    Write-Host "Exitosas:            $passed" -ForegroundColor Green
    Write-Host "Fallidas:            $failed" -ForegroundColor Red

    if ($failed -eq 0) {
        Write-Host ''
        Write-Host 'OK TODAS LAS PRUEBAS PASARON CORRECTAMENTE' -ForegroundColor Green
        exit 0
    }

    Write-Host ''
    Write-Host 'X Algunas pruebas fallaron' -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
