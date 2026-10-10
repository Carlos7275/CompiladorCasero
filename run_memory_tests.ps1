$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root 'build'
$source = Join-Path $root 'tests\memoria_extensa.mx'
$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("mx-memory-tests-" + [guid]::NewGuid().ToString('N'))
$inputFile = Join-Path $tempDir 'input.txt'
$outputBase = Join-Path $tempDir 'memoria_extensa'
$compilerLog = Join-Path $tempDir 'compiler-leaks.log'
$programLog = Join-Path $tempDir 'program-leaks.log'

New-Item -ItemType Directory -Path $tempDir | Out-Null

try {
    $drMemory = Get-Command drmemory -ErrorAction SilentlyContinue
    if (-not $drMemory) {
        throw 'Instala Dr. Memory y agrega drmemory.exe al PATH para analizar fugas en Windows.'
    }

    & cmake -S $root -B $buildDir -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        throw 'La configuracion CMake fallo.'
    }

    & cmake --build $buildDir --config Release --target compilador
    if ($LASTEXITCODE -ne 0) {
        throw 'La compilacion del compilador fallo.'
    }

    $compilerCandidates = @(
        (Join-Path $buildDir 'bin\Release\compilador.exe'),
        (Join-Path $buildDir 'bin\compilador.exe')
    )
    $compiler = $compilerCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $compiler) {
        throw 'No se encontro build\bin\Release\compilador.exe ni build\bin\compilador.exe.'
    }

    @(
        '12345'
        '19.55'
        '1'
        'texto con espacios'
    ) | Set-Content -LiteralPath $inputFile -Encoding ascii

    $previousStressValue = $env:MX_STRESS_ENV
    $env:MX_STRESS_ENV = 'stress-activo'
    Push-Location $root
    try {
        Write-Host 'Analizando fugas del compilador con Dr. Memory...'
        $compilerOutput = & $drMemory.Source -batch -exit_code_if_errors 1 -- `
            $compiler $source $outputBase -no-run 2>&1
        $compilerStatus = $LASTEXITCODE
        $compilerOutput | Set-Content -LiteralPath $compilerLog
        if ($compilerStatus -ne 0 -or -not ($compilerOutput -match '\[OK\] build:')) {
            $compilerOutput | Write-Host
            throw 'El compilador reporto errores/fugas o no genero el programa de stress.'
        }
    }
    finally {
        Pop-Location
        $env:MX_STRESS_ENV = $previousStressValue
    }

    $program = "$outputBase.exe"
    if (-not (Test-Path -LiteralPath $program)) {
        $program = $outputBase
    }
    if (-not (Test-Path -LiteralPath $program)) {
        throw 'No se encontro el programa Mx generado.'
    }

    Write-Host 'Analizando fugas del programa Mx generado con Dr. Memory...'
    Push-Location $tempDir
    try {
        $programOutput = Get-Content -LiteralPath $inputFile -Raw | & $drMemory.Source `
            -batch -exit_code_if_errors 1 -- $program 2>&1
        $programStatus = $LASTEXITCODE
        $programOutput | Set-Content -LiteralPath $programLog
        if ($programStatus -ne 0 -or -not ($programOutput -match 'MEMORIA_EXTENSA_OK:')) {
            $programOutput | Write-Host
            throw 'El programa Mx reporto errores/fugas o no completo el stress.'
        }
    }
    finally {
        Pop-Location
    }

    Write-Host 'OK: compilador y programa Mx completaron el stress sin errores de Dr. Memory.'
}
finally {
    foreach ($path in @(
        $inputFile,
        $outputBase,
        "$outputBase.asm",
        "$outputBase.o",
        "$outputBase.obj",
        "$outputBase.exe",
        $compilerLog,
        $programLog,
        (Join-Path $tempDir 'memory_stress.tmp')
    )) {
        Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $tempDir -Force -ErrorAction SilentlyContinue
}
