$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$tempRoot = $env:TEMP
if (-not $tempRoot) { throw 'Windows no definió la carpeta temporal.' }
$buildDir = Join-Path $tempRoot ("nova-home-build-" + [guid]::NewGuid().ToString('N'))
$novaDir = Join-Path $buildDir 'NovaHome'
$devkitRoot = 'C:\devkitPro'
$bash = Join-Path $devkitRoot 'msys2\usr\bin\bash.exe'
$cygpath = Join-Path $devkitRoot 'msys2\usr\bin\cygpath.exe'

if (-not (Test-Path -LiteralPath $bash) -or -not (Test-Path -LiteralPath $cygpath)) {
    throw 'No se encontró devkitPro en C:\devkitPro. Instala Switch Development en devkitPro Updater.'
}

New-Item -ItemType Directory -Path $buildDir | Out-Null
New-Item -ItemType Directory -Path (Join-Path $buildDir 'tmp') | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'switch\NovaHome') -Destination $novaDir -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'Songs') -Destination $buildDir -Recurse
$msysBuildDir = (& $cygpath -u $novaDir).Trim()
$msysTempDir = (& $cygpath -u (Join-Path $buildDir 'tmp')).Trim()
if ($LASTEXITCODE -ne 0 -or -not $msysBuildDir -or -not $msysTempDir) { throw 'No se pudo convertir la ruta de compilación para MSYS2.' }

$escapedBuildDir = $msysBuildDir.Replace("'", "'\''")
$escapedTempDir = $msysTempDir.Replace("'", "'\''")
$command = "export DEVKITPRO=/c/devkitPro DEVKITA64=/c/devkitPro/devkitA64 PATH=/c/devkitPro/portlibs/switch/bin:/c/devkitPro/devkitA64/bin:/c/devkitPro/tools/bin:/usr/bin:/bin TMPDIR='$escapedTempDir' TMP='$escapedTempDir' TEMP='$escapedTempDir'; make -C '$escapedBuildDir'"
& $bash -lc $command
if ($LASTEXITCODE -ne 0) { throw "Falló la compilación del NRO (código $LASTEXITCODE). Revisa que switch-sdl2, switch-sdl2_gfx, switch-sdl2_image y switch-sdl2_mixer estén instalados." }

$builtNro = Join-Path $novaDir 'NovaHome.nro'
if (-not (Test-Path -LiteralPath $builtNro)) { throw 'La compilación terminó sin producir NovaHome.nro.' }
$destination = Join-Path $projectRoot 'switch\NovaHome\NovaHome.nro'
Copy-Item -LiteralPath $builtNro -Destination $destination -Force
Write-Host "NRO compilado: $destination"
