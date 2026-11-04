# Port nativo para Switch

Este directorio contiene la versión gráfica nativa de NOVA HOME, probada en una Switch con Atmosphère. Lee juegos e iconos instalados y consulta el perfil, la hora, la batería y las estadísticas de juego cuando los servicios del sistema lo permiten. La navegación usa izquierda/derecha, arriba/abajo, `A`, `B`, `+`, `-` y la pantalla táctil en modo portátil. Las cajas verticales mantienen separación y la selección se amplía. El logotipo de NOVA se usa como icono de la aplicación en el Homebrew Menu; no aparece en la pantalla principal.

La biblioteca puede ordenarse por título A–Z, tiempo de juego y uso reciente. El stick izquierdo y el D-pad navegan; el foco vertical recorre perfil y accesos superiores, Ordenar, biblioteca y acciones inferiores. El carrusel tiene principio y fin y respeta el orden actual. El panel de música reproduce y lista los MP3 empaquetados, permite pausar, cambiar pista y volumen; el archivo se detiene al iniciar un juego. Los menús de mandos, ajustes y Álbum/Homebrew siguen en desarrollo.

## Compilación de NovaHome

Requiere devkitPro con `devkitA64`, `libnx`, `switch-sdl2`, `switch-sdl2_gfx`, `switch-sdl2_image` y `switch-sdl2_mixer`. En PowerShell, desde la carpeta del proyecto, ejecuta:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-nro.ps1
```

Al compilar, los MP3 de la carpeta raíz `Songs/` se copian automáticamente a RomFS. Añade allí futuras canciones y vuelve a compilar. El resultado será `switch/NovaHome/NovaHome.nro`; el script compila desde una copia temporal para manejar rutas de Windows que contienen espacios. Cópialo a `sd:/switch/NovaHome/NovaHome.nro` y ábrelo desde el Homebrew Menu. Las portadas actuales usan el icono incluido en los metadatos de cada juego; SteamGridDB aún no está conectado al NRO. Esta app no cambia archivos de Atmosphère ni reemplaza el HOME.

## Activación como HOME

`NovaToggle.nro` todavía no se genera. Para que su activación tenga un efecto real, primero hace falta implementar y validar el ejecutable de HOME que Atmosphère cargará para el título del menú, además de su loader y el paquete de recuperación. Un `.nro` de aplicación por sí solo no puede reemplazar `qlaunch`.

La futura herramienta de activación se mantendrá separada: validará una instalación, conservará respaldo de los archivos que gestione y ofrecerá restauración antes de pedir reiniciar. Nunca debe escribir en NAND ni borrar directorios desconocidos de Atmosphère. El objetivo de esta etapa es producir primero un `NovaHome.nro` que abra de forma fiable como aplicación normal.
