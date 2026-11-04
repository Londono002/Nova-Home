# Port nativo para Switch

Este directorio contiene la primera versión gráfica nativa de NOVA HOME. Dibuja el fondo dinámico, las carátulas verticales de demostración, la tarjeta seleccionada con animación, la barra superior y el panel de juego. Izquierda/derecha cambia la selección, `+` abre opciones, `A` indica que iniciar el juego aún no está conectado y `B` cierra el panel.

## Compilación de NovaHome

Requiere devkitPro con `devkitA64`, `libnx`, `switch-sdl2` y `switch-sdl2_gfx`, además de GNU Make. Desde el entorno MSYS2 de devkitPro instala las bibliotecas con `pacman -S switch-sdl2 switch-sdl2_gfx` y compila:

```sh
cd switch/NovaHome
make
```

El resultado será `switch/NovaHome/NovaHome.nro`. Copia el archivo a `sd:/switch/NovaHome/NovaHome.nro` y ábrelo desde el Homebrew Menu. Los juegos, carátulas y tiempos aún son de demostración; esta app no cambia archivos de Atmosphère ni reemplaza el HOME.

## Activación como HOME

`NovaToggle.nro` todavía no se genera. Para que su activación tenga un efecto real, primero hace falta implementar y validar el ejecutable de HOME que Atmosphère cargará para el título del menú, además de su loader y el paquete de recuperación. Un `.nro` de aplicación por sí solo no puede reemplazar `qlaunch`.

La futura herramienta de activación se mantendrá separada: validará una instalación, conservará respaldo de los archivos que gestione y ofrecerá restauración antes de pedir reiniciar. Nunca debe escribir en NAND ni borrar directorios desconocidos de Atmosphère. El objetivo de esta etapa es producir primero un `NovaHome.nro` que abra de forma fiable como aplicación normal.
