# Port nativo para Switch

Este directorio contiene la versión gráfica nativa de NOVA HOME, probada en una Switch con Atmosphère. Lee juegos e iconos instalados y consulta el perfil, la hora, la batería y las estadísticas de juego cuando los servicios del sistema lo permiten. La navegación usa izquierda/derecha, arriba/abajo, `A`, `B`, `+`, `-` y la pantalla táctil en modo portátil. Las cajas verticales mantienen separación y la selección se amplía. El logotipo de NOVA se empaqueta en RomFS.

La biblioteca puede ordenarse por título A–Z, tiempo de juego y uso reciente. Los controles actuales están en evolución: quedan pendientes el stick izquierdo, corregir el foco vertical del botón de ordenar, cerrar el carrusel en sus extremos, reproducir MP3 y conectar SteamGridDB. Los paneles de mandos, ajustes, música y Álbum/Homebrew aún requieren integración.

## Compilación de NovaHome

Requiere devkitPro con `devkitA64`, `libnx`, `switch-sdl2`, `switch-sdl2_gfx` y `switch-sdl2_image`, además de GNU Make. Desde el entorno MSYS2 de devkitPro instala las bibliotecas con `pacman -S switch-sdl2 switch-sdl2_gfx switch-sdl2_image` y compila:

```sh
cd switch/NovaHome
make
```

El resultado será `switch/NovaHome/NovaHome.nro`. Copia el archivo a `sd:/switch/NovaHome/NovaHome.nro` y ábrelo desde el Homebrew Menu. Las portadas actuales usan el icono incluido en los metadatos de cada juego; SteamGridDB aún no está conectado. Esta app no cambia archivos de Atmosphère ni reemplaza el HOME.

## Activación como HOME

`NovaToggle.nro` todavía no se genera. Para que su activación tenga un efecto real, primero hace falta implementar y validar el ejecutable de HOME que Atmosphère cargará para el título del menú, además de su loader y el paquete de recuperación. Un `.nro` de aplicación por sí solo no puede reemplazar `qlaunch`.

La futura herramienta de activación se mantendrá separada: validará una instalación, conservará respaldo de los archivos que gestione y ofrecerá restauración antes de pedir reiniciar. Nunca debe escribir en NAND ni borrar directorios desconocidos de Atmosphère. El objetivo de esta etapa es producir primero un `NovaHome.nro` que abra de forma fiable como aplicación normal.
