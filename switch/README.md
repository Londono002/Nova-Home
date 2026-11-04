# Port nativo para Switch

Este directorio contiene la versión gráfica nativa de NOVA HOME. Lee el catálogo instalado y sus iconos mediante libnx, usa los datos de perfil, reloj, batería y tiempo de juego cuando los servicios los permiten, e incluye el logotipo NOVA en RomFS. La navegación funciona con izquierda/derecha, arriba/abajo, `A`, `B`, `+`, `-` y la pantalla táctil en modo portátil. Las carátulas son verticales y el espacio entre ellas se calcula para que no se superpongan; la selección se amplía.

## Compilación de NovaHome

Requiere devkitPro con `devkitA64`, `libnx`, `switch-sdl2`, `switch-sdl2_gfx` y `switch-sdl2_image`, además de GNU Make. Desde el entorno MSYS2 de devkitPro instala las bibliotecas con `pacman -S switch-sdl2 switch-sdl2_gfx switch-sdl2_image` y compila:

```sh
cd switch/NovaHome
make
```

El resultado será `switch/NovaHome/NovaHome.nro`. Copia el archivo a `sd:/switch/NovaHome/NovaHome.nro` y ábrelo desde el Homebrew Menu. Las imágenes se muestran desde el icono oficial incluido en los metadatos de cada aplicación. Esta app no cambia archivos de Atmosphère ni reemplaza el HOME.

## Activación como HOME

`NovaToggle.nro` todavía no se genera. Para que su activación tenga un efecto real, primero hace falta implementar y validar el ejecutable de HOME que Atmosphère cargará para el título del menú, además de su loader y el paquete de recuperación. Un `.nro` de aplicación por sí solo no puede reemplazar `qlaunch`.

La futura herramienta de activación se mantendrá separada: validará una instalación, conservará respaldo de los archivos que gestione y ofrecerá restauración antes de pedir reiniciar. Nunca debe escribir en NAND ni borrar directorios desconocidos de Atmosphère. El objetivo de esta etapa es producir primero un `NovaHome.nro` que abra de forma fiable como aplicación normal.
