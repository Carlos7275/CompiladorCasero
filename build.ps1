$ErrorActionPreference = 'Stop'

# ============================================================
# CompiladorCasero - BUILD WINDOWS
# Toolchain:
#   - CMake
#   - NASM
#   - MSVC
#   - GCC / MinGW-w64 (WinLibs)
# ============================================================

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "       CompiladorCasero - BUILD" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================
# UTILIDADES
# ============================================================

function Test-Command {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Refresh-Path {

    $machinePath = [Environment]::GetEnvironmentVariable(
        "Path",
        "Machine"
    )

    $userPath = [Environment]::GetEnvironmentVariable(
        "Path",
        "User"
    )

    $env:Path = "$machinePath;$userPath"
}

function Add-ToUserPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathToAdd
    )

    $userPath = [Environment]::GetEnvironmentVariable(
        "Path",
        "User"
    )

    $parts = @()

    if ($userPath) {
        $parts = $userPath -split ';' |
            Where-Object { $_ -and $_.Trim() }
    }

    $exists = $parts |
        Where-Object {
            $_.TrimEnd('\') -ieq $PathToAdd.TrimEnd('\')
        }

    if (-not $exists) {

        $newPath = "$PathToAdd;$userPath"

        [Environment]::SetEnvironmentVariable(
            "Path",
            $newPath,
            "User"
        )
    }

    if ($env:Path -notlike "*$PathToAdd*") {
        $env:Path = "$PathToAdd;$env:Path"
    }
}

# ============================================================
# WINGET
# ============================================================

function Ensure-Winget {

    if (Test-Command "winget") {
        Write-Host "[OK] winget encontrado." -ForegroundColor Green
        return
    }

    throw @"
winget no está disponible.

Instala Microsoft App Installer desde Microsoft Store
y vuelve a ejecutar este script.
"@
}

# ============================================================
# CMAKE
# ============================================================

function Ensure-CMake {

    if (Test-Command "cmake") {

        $version = & cmake --version 2>$null |
            Select-Object -First 1

        Write-Host "[OK] CMake encontrado: $version" -ForegroundColor Green

        return
    }

    Write-Host "[INFO] CMake no está instalado." -ForegroundColor Yellow
    Write-Host "[INFO] Instalando CMake..." -ForegroundColor Cyan

    winget install `
        --id Kitware.CMake `
        -e `
        --accept-source-agreements `
        --accept-package-agreements

    Refresh-Path

    if (-not (Test-Command "cmake")) {
        throw @"
CMake fue instalado pero no está disponible en PATH.

Cierra PowerShell, abre una nueva ventana y ejecuta:

.\build.ps1
"@
    }

    Write-Host "[OK] CMake instalado." -ForegroundColor Green
}

# ============================================================
# NASM
# ============================================================

function Ensure-NASM {

    if (Test-Command "nasm") {

        $version = & nasm -v 2>$null

        Write-Host "[OK] NASM encontrado: $version" -ForegroundColor Green

        return
    }

    Write-Host "[INFO] NASM no está instalado." -ForegroundColor Yellow
    Write-Host "[INFO] Instalando NASM..." -ForegroundColor Cyan

    winget install `
        --id NASM.NASM `
        -e `
        --accept-source-agreements `
        --accept-package-agreements

    Refresh-Path

    if (-not (Test-Command "nasm")) {
        throw @"
NASM fue instalado pero no está disponible en PATH.

Cierra PowerShell, abre una nueva ventana y ejecuta:

.\build.ps1
"@
    }

    Write-Host "[OK] NASM instalado." -ForegroundColor Green
}

# ============================================================
# WINLIBS GCC
# ============================================================

function Find-WinLibsGCC {

    $possiblePaths = @(
        "C:\WinLibs\mingw64\bin\gcc.exe",
        "C:\WinLibs\bin\gcc.exe",
        "$env:USERPROFILE\WinLibs\mingw64\bin\gcc.exe",
        "$env:LOCALAPPDATA\WinLibs\mingw64\bin\gcc.exe"
    )

    foreach ($path in $possiblePaths) {

        if (Test-Path $path) {
            return $path
        }
    }

    # Buscar en PATH
    $gcc = Get-Command gcc.exe -ErrorAction SilentlyContinue

    if ($gcc) {
        return $gcc.Source
    }

    return $null
}

function Ensure-GCC {

    $gcc = Find-WinLibsGCC

    if ($gcc) {

        $gccDir = Split-Path $gcc -Parent

        Add-ToUserPath $gccDir

        Write-Host "[OK] GCC encontrado." -ForegroundColor Green
        Write-Host "     $gcc" -ForegroundColor Gray

        $version = & $gcc --version 2>$null |
            Select-Object -First 1

        Write-Host "     $version" -ForegroundColor Gray

        return
    }

    Write-Host ""
    Write-Host "[INFO] GCC no está instalado." -ForegroundColor Yellow
    Write-Host "[INFO] Instalando GCC nativo Windows (WinLibs)..." -ForegroundColor Cyan
    Write-Host ""

    $installDir = "C:\WinLibs"

    if (Test-Path $installDir) {
        Remove-Item `
            $installDir `
            -Recurse `
            -Force
    }

    New-Item `
        -ItemType Directory `
        -Path $installDir `
        -Force |
        Out-Null

    # --------------------------------------------------------
    # Buscar WinLibs mediante winget
    # --------------------------------------------------------

    Write-Host "[INFO] Buscando WinLibs..." -ForegroundColor Cyan

    $wingetSearch = winget search WinLibs 2>$null

    if (-not $wingetSearch) {

        Write-Host "[WARN] WinLibs no está disponible directamente mediante winget." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Instala WinLibs manualmente desde su distribución oficial." -ForegroundColor Yellow
        Write-Host "Después coloca GCC en:" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "    C:\WinLibs\mingw64\bin" -ForegroundColor White
        Write-Host ""

        throw "No se pudo instalar WinLibs automáticamente."
    }

    Write-Host ""
    Write-Host $wingetSearch
    Write-Host ""

    # Intentar detectar un paquete WinLibs
    $packageId = $null

    $lines = $wingetSearch -split "`n"

    foreach ($line in $lines) {

        if ($line -match 'WinLibs') {

            $columns = $line -split '\s{2,}'

            if ($columns.Count -ge 2) {
                $packageId = $columns[1].Trim()
                break
            }
        }
    }

    if (-not $packageId) {

        Write-Host "[WARN] No se encontró un paquete WinLibs instalable mediante winget." -ForegroundColor Yellow

        throw @"
No se pudo instalar WinLibs automáticamente.

Instala una distribución WinLibs GCC/MinGW-w64
y asegúrate de que gcc.exe esté en PATH.
"@
    }

    Write-Host "[INFO] Instalando paquete: $packageId" -ForegroundColor Cyan

    winget install `
        --id $packageId `
        -e `
        --accept-source-agreements `
        --accept-package-agreements

    Refresh-Path

    $gcc = Find-WinLibsGCC

    if (-not $gcc) {

        throw @"
WinLibs fue instalado, pero no se encontró gcc.exe.

Ejecuta:

where.exe gcc

y comprueba que GCC esté disponible en PATH.
"@
    }

    $gccDir = Split-Path $gcc -Parent

    Add-ToUserPath $gccDir

    Write-Host "[OK] GCC nativo Windows instalado." -ForegroundColor Green
    Write-Host "     $gcc" -ForegroundColor Gray
}

# ============================================================
# VSWHERE
# ============================================================

function Find-VSWhere {

    $paths = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    foreach ($path in $paths) {

        if (Test-Path $path) {
            return $path
        }
    }

    return $null
}

# ============================================================
# BUSCAR MSVC
# ============================================================

function Find-MSVC {

    $vswhere = Find-VSWhere

    if (-not $vswhere) {
        return $null
    }

    $result = & $vswhere `
        -latest `
        -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath `
        2>$null

    if ($result) {
        return $result.Trim()
    }

    return $null
}

# ============================================================
# INSTALAR MSVC
# ============================================================

function Ensure-MSVC {

    $vsPath = Find-MSVC

    if ($vsPath) {

        Write-Host "[OK] MSVC encontrado." -ForegroundColor Green
        Write-Host "     $vsPath" -ForegroundColor Gray

        return $vsPath
    }

    Write-Host ""
    Write-Host "[INFO] MSVC no está instalado." -ForegroundColor Yellow
    Write-Host "[INFO] Instalando Visual Studio Build Tools..." -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Esto puede tardar varios minutos." -ForegroundColor Yellow
    Write-Host ""

    winget install `
        --id Microsoft.VisualStudio.BuildTools `
        -e `
        --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" `
        --accept-source-agreements `
        --accept-package-agreements

    Write-Host ""
    Write-Host "[INFO] Verificando instalación de MSVC..." -ForegroundColor Cyan

    $vsPath = Find-MSVC

    if (-not $vsPath) {

        throw @"
Visual Studio Build Tools se instaló, pero no se encontró MSVC.

Reinicia Windows y vuelve a ejecutar:

.\build.ps1
"@
    }

    Write-Host "[OK] MSVC instalado correctamente." -ForegroundColor Green

    return $vsPath
}

# ============================================================
# INICIALIZAR MSVC
# ============================================================

function Initialize-MSVC {

    param(
        [Parameter(Mandatory = $true)]
        [string]$VSPath
    )

    $vcvars = Join-Path `
        $VSPath `
        "VC\Auxiliary\Build\vcvars64.bat"

    if (-not (Test-Path $vcvars)) {

        $vcvars = Join-Path `
            $VSPath `
            "VC\Auxiliary\Build\vcvarsall.bat"

        if (-not (Test-Path $vcvars)) {
            throw "No se encontró vcvars64.bat ni vcvarsall.bat."
        }
    }

    Write-Host "[INFO] Inicializando entorno MSVC x64..." -ForegroundColor Cyan

    if ($vcvars -like "*vcvarsall.bat") {

        $envDump = cmd.exe /c "`"$vcvars`" amd64 > nul && set"
    }
    else {

        $envDump = cmd.exe /c "`"$vcvars`" > nul && set"
    }

    foreach ($line in $envDump) {

        if ($line -match '^([^=]+)=(.*)$') {

            $name = $matches[1]
            $value = $matches[2]

            Set-Item `
                -Path "Env:$name" `
                -Value $value
        }
    }

    if (-not (Test-Command "cl.exe")) {
        throw "MSVC está instalado pero cl.exe no está disponible."
    }

    if (-not (Test-Command "link.exe")) {
        throw "MSVC está instalado pero link.exe no está disponible."
    }

    Write-Host "[OK] MSVC x64 listo." -ForegroundColor Green
}

# ============================================================
# SIGNTOOL
# ============================================================

function Find-SignTool {

    $patterns = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe",
        "${env:ProgramFiles}\Windows Kits\10\bin\*\x64\signtool.exe"
    )

    foreach ($pattern in $patterns) {

        $tool = Get-ChildItem `
            $pattern `
            -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending |
            Select-Object -First 1

        if ($tool) {
            return $tool.FullName
        }
    }

    return $null
}

# ============================================================
# CERTIFICADO DE DESARROLLO
# ============================================================

function Ensure-DevelopmentCertificate {

    $subject = "CN=CompiladorCasero Development"

    $cert = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object {
            $_.Subject -eq $subject -and
            $_.HasPrivateKey -and
            $_.NotAfter -gt (Get-Date)
        } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1

    if (-not $cert) {

        Write-Host "[INFO] Creando certificado de desarrollo..." -ForegroundColor Cyan

        $cert = New-SelfSignedCertificate `
            -Type CodeSigningCert `
            -Subject $subject `
            -CertStoreLocation "Cert:\CurrentUser\My" `
            -HashAlgorithm SHA256 `
            -NotAfter (Get-Date).AddYears(3)

        Write-Host "[OK] Certificado creado." -ForegroundColor Green
    }
    else {

        Write-Host "[OK] Certificado encontrado." -ForegroundColor Green
    }

    $tempCert = Join-Path `
        $env:TEMP `
        "CompiladorCasero-Development.cer"

    Export-Certificate `
        -Cert $cert `
        -FilePath $tempCert `
        -Force |
        Out-Null

    $trustedRoot = "Cert:\CurrentUser\Root"

    $alreadyTrusted = Get-ChildItem $trustedRoot |
        Where-Object {
            $_.Thumbprint -eq $cert.Thumbprint
        }

    if (-not $alreadyTrusted) {

        Write-Host "[INFO] Agregando certificado a Trusted Root..." -ForegroundColor Cyan

        Import-Certificate `
            -FilePath $tempCert `
            -CertStoreLocation $trustedRoot |
            Out-Null

        Write-Host "[OK] Certificado agregado a confianza local." -ForegroundColor Green
    }

    Remove-Item `
        $tempCert `
        -Force `
        -ErrorAction SilentlyContinue

    Write-Host "     Thumbprint: $($cert.Thumbprint)" -ForegroundColor Gray

    return $cert
}

# ============================================================
# FIRMAR EJECUTABLE
# ============================================================

function Sign-Executable {

    param(
        [Parameter(Mandatory = $true)]
        [string]$Executable
    )

    if (-not (Test-Path $Executable)) {
        throw "No se encontró el ejecutable: $Executable"
    }

    $signtool = Find-SignTool

    if (-not $signtool) {

        Write-Host ""
        Write-Host "[WARN] No se encontró signtool.exe." -ForegroundColor Yellow
        Write-Host "[WARN] Se omitirá la firma." -ForegroundColor Yellow

        return
    }

    $cert = Ensure-DevelopmentCertificate

    Write-Host ""
    Write-Host "[INFO] Firmando ejecutable..." -ForegroundColor Cyan
    Write-Host "       $Executable" -ForegroundColor Gray

    & $signtool sign `
        /sha1 $cert.Thumbprint `
        /fd SHA256 `
        $Executable

    if ($LASTEXITCODE -ne 0) {
        throw "signtool no pudo firmar el ejecutable."
    }

    Write-Host "[OK] Ejecutable firmado." -ForegroundColor Green
}

# ============================================================
# PREPARAR TOOLCHAIN
# ============================================================

Ensure-Winget
Ensure-CMake
Ensure-NASM
Ensure-GCC

$vsPath = Ensure-MSVC

Initialize-MSVC $vsPath

# Volver a asegurar GCC después de inicializar MSVC.
# MSVC puede modificar PATH.
Ensure-GCC

# ============================================================
# PROYECTO
# ============================================================

$root = Split-Path -Parent $MyInvocation.MyCommand.Path

if ($args.Count -gt 0) {
    $buildDir = $args[0]
}
else {
    $buildDir = Join-Path $root "build"
}

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Configurando proyecto con CMake" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "[INFO] gcc:" -ForegroundColor Gray
where.exe gcc

Write-Host ""
Write-Host "[INFO] cl:" -ForegroundColor Gray
where.exe cl

Write-Host ""
Write-Host "[INFO] link:" -ForegroundColor Gray
where.exe link

Write-Host ""

cmake `
    -S $root `
    -B $buildDir `
    -DCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    throw "CMake falló durante la configuración."
}

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Compilando proyecto" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

cmake `
    --build $buildDir `
    --config Release `
    --parallel

if ($LASTEXITCODE -ne 0) {
    throw "La compilación falló."
}

# ============================================================
# BUSCAR EJECUTABLE
# ============================================================

Write-Host ""
Write-Host "[INFO] Buscando compilador.exe..." -ForegroundColor Cyan

$exe = Get-ChildItem `
    $buildDir `
    -Recurse `
    -Filter "compilador.exe" `
    -File `
    -ErrorAction SilentlyContinue |
    Select-Object -First 1

if (-not $exe) {
    throw "No se encontró compilador.exe dentro de: $buildDir"
}

$builtExe = $exe.FullName
$rootExe = Join-Path $root "compilador.exe"

Write-Host "[OK] Ejecutable encontrado:" -ForegroundColor Green
Write-Host "     $builtExe" -ForegroundColor Gray

# ============================================================
# FIRMAR
# ============================================================

Sign-Executable $builtExe

# ============================================================
# COPIAR A LA RAÍZ
# ============================================================

Copy-Item `
    $builtExe `
    $rootExe `
    -Force

Write-Host ""
Write-Host "[OK] Ejecutable copiado a:" -ForegroundColor Green
Write-Host "     $rootExe" -ForegroundColor Gray

# ============================================================
# VERIFICAR FIRMA
# ============================================================

$signature = Get-AuthenticodeSignature $rootExe

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "          BUILD COMPLETADO" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

if ($signature.Status -eq "Valid") {
    Write-Host "Estado de firma: VALID" -ForegroundColor Green
}
else {
    Write-Host "Estado de firma: $($signature.Status)" -ForegroundColor Yellow
}

if ($signature.SignerCertificate) {
    Write-Host "Firmante: $($signature.SignerCertificate.Subject)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Herramientas:"
Write-Host "  CMake : $(if (Test-Command cmake) { 'OK' } else { 'NO' })"
Write-Host "  NASM  : $(if (Test-Command nasm) { 'OK' } else { 'NO' })"
Write-Host "  GCC   : $(if (Test-Command gcc) { 'OK' } else { 'NO' })"
Write-Host "  MSVC  : $(if (Test-Command cl) { 'OK' } else { 'NO' })"
Write-Host ""
Write-Host "Ejecutar:"
Write-Host "  .\compilador.exe <archivo.mx>" -ForegroundColor White
Write-Host ""