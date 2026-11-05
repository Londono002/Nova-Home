$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$tempRoot = $env:TEMP
if (-not $tempRoot) { throw 'Windows no definió la carpeta temporal.' }
$buildDir = Join-Path $tempRoot ("nova-manager-build-" + [guid]::NewGuid().ToString('N'))
$managerDir = Join-Path $buildDir 'NovaManager'
$devkitRoot = 'C:\devkitPro'
$bash = Join-Path $devkitRoot 'msys2\usr\bin\bash.exe'
$cygpath = Join-Path $devkitRoot 'msys2\usr\bin\cygpath.exe'

if (-not (Test-Path -LiteralPath $bash) -or -not (Test-Path -LiteralPath $cygpath)) {
    throw 'No se encontró devkitPro en C:\devkitPro.'
}
New-Item -ItemType Directory -Path $buildDir | Out-Null
New-Item -ItemType Directory -Path (Join-Path $buildDir 'tmp') | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'switch\NovaManager') -Destination $managerDir -Recurse
New-Item -ItemType Directory -Path (Join-Path $managerDir 'romfs\fonts') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $buildDir 'fonts\Manrope\static') -Force | Out-Null
Copy-Item -Path (Join-Path $projectRoot 'fonts\Manrope\static\*.ttf') -Destination (Join-Path $buildDir 'fonts\Manrope\static') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'switch\NovaHome\NovaHome.jpg') -Destination (Join-Path $managerDir 'NOVAManager.jpg') -Force
$msysManagerDir = (& $cygpath -u $managerDir).Trim()
$msysTempDir = (& $cygpath -u (Join-Path $buildDir 'tmp')).Trim()
if ($LASTEXITCODE -ne 0 -or -not $msysManagerDir -or -not $msysTempDir) { throw 'No se pudo convertir la ruta de compilación para MSYS2.' }
$escapedManagerDir = $msysManagerDir.Replace("'", "'\''")
$escapedTempDir = $msysTempDir.Replace("'", "'\''")
$command = "export DEVKITPRO=/c/devkitPro DEVKITA64=/c/devkitPro/devkitA64 PATH=/c/devkitPro/portlibs/switch/bin:/c/devkitPro/devkitA64/bin:/c/devkitPro/tools/bin:/usr/bin:/bin TMPDIR='$escapedTempDir' TMP='$escapedTempDir' TEMP='$escapedTempDir'; make -C '$escapedManagerDir' FONT_SOURCE_DIR=../fonts/Manrope/static"
& $bash -lc $command
if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación de NOVA Manager.' }
$builtNro = Join-Path $managerDir 'NOVAManager.nro'
if (-not (Test-Path -LiteralPath $builtNro)) { throw 'La compilación terminó sin producir NOVAManager.nro.' }
$destination = Join-Path $projectRoot 'switch\NovaManager\NOVAManager.nro'
Copy-Item -LiteralPath $builtNro -Destination $destination -Force
Write-Host "NRO compilado: $destination"
