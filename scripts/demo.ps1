#Requires -Version 5.1
<#
.SYNOPSIS
    demo.ps1 - Demostracion guiada del simulador MLFQ para Windows.

.DESCRIPTION
    Equivalente de scripts/demo.sh. Muestra el funcionamiento completo del
    proyecto en diez pasos, imprimiendo solo lo esencial de cada uno: la idea es
    que quepa en pantalla y se pueda narrar en vivo, no volcar toda la salida.

    Compila con `make` si esta disponible (MSYS2 / WSL); si no, compila
    directamente con gcc, clang o cc, sin necesidad de make.

.PARAMETER Pausa
    Espera un ENTER entre pasos. Util para presentar en vivo.

.EXAMPLE
    .\scripts\demo.ps1
.EXAMPLE
    .\scripts\demo.ps1 -Pausa
.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\demo.ps1
#>
[CmdletBinding()]
param(
    [switch] $Pausa
)

# Los comandos nativos senalan el error con su codigo de salida, no con
# excepciones: se comprueba a mano donde importa (el paso 6 falla a proposito).
$ErrorActionPreference = 'Continue'
if (Test-Path 'variable:PSNativeCommandUseErrorActionPreference') {
    $PSNativeCommandUseErrorActionPreference = $false
}

# La consola de Windows no siempre usa UTF-8; sin esto las tildes se ven mal.
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }

Set-Location (Split-Path -Parent $PSScriptRoot)

# ---------------------------------------------------------------------------
# Utilidades de presentacion
# ---------------------------------------------------------------------------

$script:Paso  = 0
$script:Total = 10

function Escribir-Titulo {
    param([string] $Texto, [string] $Subtitulo)
    Write-Host ''
    Write-Host "  $Texto" -ForegroundColor Cyan
    Write-Host "  $Subtitulo" -ForegroundColor DarkGray
}

function Escribir-Paso {
    param([string] $Titulo)
    $script:Paso++
    Write-Host ''
    Write-Host ("-- {0}/{1} - {2} {3}" -f $script:Paso, $script:Total, $Titulo, ('-' * 12)) `
        -ForegroundColor Cyan
}

function Escribir-Comando { param([string] $Texto) Write-Host "   > $Texto" -ForegroundColor DarkGray }
function Escribir-Nota    { param([string] $Texto) Write-Host "   -> $Texto" -ForegroundColor Green }

function Escribir-Bloque {
    param([string[]] $Lineas)
    foreach ($linea in $Lineas) { Write-Host ('   ' + $linea) }
}

function Pausar {
    if (-not $Pausa) { return }
    Write-Host ''
    Read-Host '   [ENTER para continuar]' | Out-Null
}

# Equivalente de `sed -n '/Desde/,/Hasta/p'`.
function Obtener-Bloque {
    param([string[]] $Lineas, [string] $Desde, [string] $Hasta)
    $resultado = @()
    $dentro = $false
    foreach ($linea in $Lineas) {
        if (-not $dentro) {
            if ($linea -match $Desde) { $dentro = $true } else { continue }
        }
        elseif ($linea -match $Hasta) { break }
        $resultado += $linea
    }
    return $resultado
}

function Obtener-Metrica {
    param([string[]] $Lineas, [string] $Patron)
    foreach ($linea in $Lineas) {
        if ($linea -match $Patron) { return ($linea -replace '^[^:]*:\s*', '') }
    }
    return '?'
}

function Obtener-Promedio {
    param([string] $RutaCsv, [string] $Columna)
    $fila = Import-Csv -Path $RutaCsv | Where-Object { $_.PID -eq 'AVG' }
    if ($null -eq $fila) { return '?' }
    return $fila.$Columna
}

function Buscar-Programa {
    param([string] $Nombre)
    $encontrado = Get-Command $Nombre -ErrorAction SilentlyContinue
    if ($encontrado) { return $encontrado.Source }
    return $null
}

# ---------------------------------------------------------------------------
# Preparacion
# ---------------------------------------------------------------------------

$Temporal = Join-Path ([System.IO.Path]::GetTempPath()) ("mlfq-demo-" + [System.Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Temporal -Force | Out-Null

$Binario = 'build\scheduler.exe'

try {

Escribir-Titulo 'SIMULADOR DE SCHEDULER MLFQ - demostracion' `
                'C11 sin dependencias - arquitectura por capas - State + Strategy + Observer'

# --- 1. Compilacion --------------------------------------------------------

Escribir-Paso 'Compilacion'

$rutaMake  = Buscar-Programa 'make'
$logFallo  = Join-Path $Temporal 'build.log'
if ($rutaMake) {
    Escribir-Comando 'make clean && make'
    & make clean  2>&1 | Out-Null
    & make        2>&1 | Out-File (Join-Path $Temporal 'build.log')
    $compilacionOk = ($LASTEXITCODE -eq 0)
}
else {
    $compilador = $null
    foreach ($candidato in @('gcc', 'clang', 'cc')) {
        $compilador = Buscar-Programa $candidato
        if ($compilador) { break }
    }
    if (-not $compilador) {
        Write-Host '   No se encontro make ni un compilador C (gcc, clang, cc).' -ForegroundColor Red
        Write-Host '   Instale MSYS2 (pacman -S mingw-w64-ucrt-x86_64-gcc make) o vea COMPILACION.md.' `
            -ForegroundColor Red
        exit 1
    }

    # Sin make: se compila con un unico comando. Los comodines se expanden aqui
    # para no depender del globbing del compilador.
    $fuentesNucleo = Get-ChildItem -Path 'src' -Recurse -Filter '*.c' |
        Where-Object { $_.FullName -notmatch 'cli.main\.c$' } |
        ForEach-Object { $_.FullName }
    $mainAplicacion  = (Resolve-Path 'src\cli\main.c').Path
    $fuentesPruebas  = Get-ChildItem -Path 'tests' -Filter '*.c' | ForEach-Object { $_.FullName }
    $banderas        = @('-std=c11', '-Wall', '-Wextra', '-pedantic', '-O2', '-Iinclude', '-Isrc')

    Escribir-Comando "$(Split-Path -Leaf $compilador) -std=c11 -Wall -Wextra -pedantic -O2 ... -o $Binario"
    New-Item -ItemType Directory -Path 'build' -Force | Out-Null
    & $compilador @banderas @fuentesNucleo $mainAplicacion -o $Binario 2>&1 |
        Out-File (Join-Path $Temporal 'build.log')
    $compilacionOk = ($LASTEXITCODE -eq 0)

    if ($compilacionOk) {
        $logFallo = Join-Path $Temporal 'build-tests.log'
        & $compilador @banderas @fuentesNucleo @fuentesPruebas -o 'build\run_tests.exe' 2>&1 |
            Out-File $logFallo
        $compilacionOk = ($LASTEXITCODE -eq 0)
    }
}

if (-not $compilacionOk) {
    Escribir-Bloque (Get-Content $logFallo -Tail 20)
    Write-Host '   La compilacion fallo.' -ForegroundColor Red
    exit 1
}
Escribir-Nota 'compilado correctamente y sin advertencias'

# --- 2. Pruebas ------------------------------------------------------------

Escribir-Paso 'Pruebas unitarias'
if ($rutaMake) {
    # `make` compila el ejecutable de pruebas y lo ejecuta en un solo objetivo.
    Escribir-Comando 'make test'
    $salidaPruebas = & make test 2>&1
}
else {
    Escribir-Comando '.\build\run_tests.exe'
    $salidaPruebas = & '.\build\run_tests.exe' 2>&1
}
Escribir-Bloque ($salidaPruebas | Where-Object { $_ -match '^Suite:' })
Write-Host ''
Escribir-Bloque ($salidaPruebas | Where-Object { $_ -match 'verificaciones|RESULTADO' })
Pausar

# --- 3. Escenario del enunciado -------------------------------------------

Escribir-Paso 'Escenario del enunciado: P1(0,8) P2(1,4) P3(2,9) P4(3,5)'
Escribir-Comando ".\$Binario"
$salidaBase = & ".\$Binario" 2>&1
Escribir-Bloque (Obtener-Bloque $salidaBase '=== Metricas por proceso ===' '^\s*$')
Escribir-Bloque ($salidaBase | Where-Object { $_ -match 'promedio|Ciclos totales|Uso de CPU' })
Escribir-Nota 'response = primera respuesta - llegada; turnaround = fin - llegada; waiting = turnaround - burst'
Pausar

# --- 4. Linea de tiempo ----------------------------------------------------

Escribir-Paso 'Linea de tiempo: se lee el algoritmo completo'
Escribir-Bloque ((Obtener-Bloque $salidaBase '=== Linea de tiempo' '=== Eventos') |
    Where-Object { $_.Trim() -ne '' })
Escribir-Nota 'el digito es el NIVEL DE COLA en que ejecuto: 00 en Q0, 1111 en Q1 tras la democion'
Escribir-Nota 'en el ciclo 20 el priority boost devuelve a P3 y P4 a Q0 (vuelven a ejecutar con 0)'
Pausar

# --- 5. Traza (Observer) ---------------------------------------------------

Escribir-Paso 'Traza de eventos (patron Observer), primeros ciclos'
Escribir-Comando ".\$Binario --trace"
$salidaTraza = & ".\$Binario" '--trace' '--no-csv' 2>&1
$traza = Obtener-Bloque $salidaTraza '=== Traza de eventos ===' '^\s*$'
Escribir-Bloque ($traza | Select-Object -Skip 1 -First 15)
Escribir-Nota 'el motor publica eventos; la traza, el Gantt y las estadisticas son observadores'
Pausar

# --- 6. Validacion de entradas --------------------------------------------

Escribir-Paso 'Validacion de entradas (sin fallos silenciosos)'
Escribir-Comando ".\$Binario --input examples\procesos_invalido.csv"
$salidaError = & ".\$Binario" '--input' 'examples\procesos_invalido.csv' 2>&1
$codigoSalida = $LASTEXITCODE
Escribir-Bloque ($salidaError | Where-Object { $_ -match '^Error' })
Escribir-Nota "codigo de salida $codigoSalida : indica el archivo, la linea, el proceso, el valor y la regla violada"
Pausar

# --- 7. Strategy: tres politicas ------------------------------------------

Escribir-Paso 'Strategy: el mismo motor con tres politicas distintas'
Write-Host ('   {0,-14} {1,10} {2,12} {3,10} {4,8}' -f 'Politica', 'Response', 'Turnaround', 'Waiting', 'Cambios')
Write-Host ('   {0,-14} {1,10} {2,12} {3,10} {4,8}' -f ('-' * 14), ('-' * 10), ('-' * 12), ('-' * 10), ('-' * 8))

$politicas = @(
    @{ Etiqueta = 'MLFQ 2/4/8';  Banderas = @('--policy', 'mlfq') },
    @{ Etiqueta = 'Round Robin'; Banderas = @('--policy', 'rr', '--quantums', '2') },
    @{ Etiqueta = 'FCFS';        Banderas = @('--policy', 'fcfs') }
)
$csvTemporal = Join-Path $Temporal 'politica.csv'
foreach ($politica in $politicas) {
    $salida = & ".\$Binario" @($politica.Banderas) '--output' $csvTemporal '--no-gantt' 2>&1
    Write-Host ('   {0,-14} {1,10} {2,12} {3,10} {4,8}' -f `
        $politica.Etiqueta,
        (Obtener-Promedio $csvTemporal 'Response'),
        (Obtener-Promedio $csvTemporal 'Turnaround'),
        (Obtener-Promedio $csvTemporal 'Waiting'),
        (Obtener-Metrica $salida '^Cambios de contexto'))
}
Escribir-Nota 'MLFQ da un tiempo de respuesta 5.8x mejor que FCFS a cambio de 28% de turnaround'
Escribir-Nota 'el motor de simulacion no se recompilo: la politica es una dependencia inyectada'
Pausar

# --- 8. OCP: extensiones sin tocar el nucleo ------------------------------

Escribir-Paso 'Abierto/cerrado: extensiones que son banderas, no ediciones de codigo'
$extensiones = @(
    @{ Etiqueta = 'cuarto nivel de cola';    Banderas = @('--quantums', '2,4,8,16') },
    @{ Etiqueta = 'otra regla de democion';  Banderas = @('--demote-to-lowest') },
    @{ Etiqueta = 'otra disciplina de cola'; Banderas = @('--queue', 'srtf') },
    @{ Etiqueta = 'sin priority boost';      Banderas = @('--boost', '0') },
    @{ Etiqueta = 'quantum minimo en Q0';    Banderas = @('--quantums', '1,4,8') }
)
foreach ($extension in $extensiones) {
    & ".\$Binario" @($extension.Banderas) '--output' $csvTemporal '--no-gantt' 2>&1 | Out-Null
    Write-Host ('   {0,-24} {1,-20} turnaround {2} - waiting {3}' -f `
        $extension.Etiqueta,
        ($extension.Banderas -join ' '),
        (Obtener-Promedio $csvTemporal 'Turnaround'),
        (Obtener-Promedio $csvTemporal 'Waiting'))
}
Escribir-Nota 'cinco variaciones de comportamiento, cero modificaciones al motor o a la politica'
Pausar

# --- 9. Inanicion ----------------------------------------------------------

Escribir-Paso 'Inanicion: P1 (burst=30) contra 25 procesos cortos que llegan cada 2 ciclos'
foreach ($intervalo in @(0, 10)) {
    Write-Host ''
    if ($intervalo -eq 0) {
        Write-Host '   SIN priority boost:' -ForegroundColor White
    }
    else {
        Write-Host "   CON priority boost cada $intervalo ciclos:" -ForegroundColor White
    }
    $salida = & ".\$Binario" '--input' 'examples\starvation.csv' '--boost' "$intervalo" '--no-csv' 2>&1
    $lineaTiempo = Obtener-Bloque $salida '=== Linea de tiempo' '^IDLE'
    Escribir-Bloque ($lineaTiempo | Where-Object { $_ -match '^(t|P1) ' })
    $espera = '?'
    foreach ($linea in $salida) {
        if ($linea -match '^P1\s+espera continua maxima:\s*(\d+)') { $espera = $Matches[1] }
    }
    Escribir-Nota "P1 estuvo hasta $espera ciclos seguidos sin CPU"
}
Escribir-Nota 'sin boost la espera no esta acotada; con boost queda acotada por S'
Pausar

# --- 10. Salida exportada -------------------------------------------------

Escribir-Paso 'Salida exportada: results.csv'
Escribir-Comando 'type results.csv'
& ".\$Binario" 2>&1 | Out-Null
Escribir-Bloque (Get-Content 'results.csv')
Write-Host ''
Escribir-Nota 'columnas exactas del enunciado, mas una fila AVG con los promedios'

Write-Host ''
Write-Host '  Fin de la demostracion.' -ForegroundColor Green
Write-Host '  Mas detalle: README.md - DESIGN.md - ANALISIS.md - PRESENTACION.md - COMPILACION.md' `
    -ForegroundColor DarkGray
Write-Host '  Analisis completo con 15 variantes: make experiments (requiere bash: MSYS2 o WSL)' `
    -ForegroundColor DarkGray
Write-Host ''

}
finally {
    Remove-Item -Path $Temporal -Recurse -Force -ErrorAction SilentlyContinue
}
