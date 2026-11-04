# Port nativo para Switch

Este directorio inicia el port con libnx. La primera aplicación comprueba que un `.nro` de NOVA arranca desde Atmosphère, lee el mando y sale limpiamente con `+`. Esta pantalla de texto es un hito técnico temporal; todavía no es el HOME visual del prototipo de PC.

## Compilación de NovaHome

Requiere devkitPro con `devkitA64` y `libnx`, además de GNU Make. Desde el entorno MSYS2 de devkitPro:

```sh
cd switch/NovaHome
make
```

El resultado será `switch/NovaHome/NovaHome.nro`. Copia el archivo a `sd:/switch/NovaHome/NovaHome.nro` y ábrelo desde el Homebrew Menu. La prueba no cambia archivos de Atmosphère ni reemplaza el HOME.

## Activación como HOME

`NovaToggle.nro` todavía no se genera. Para que su activación tenga un efecto real, primero hace falta implementar y validar el ejecutable de HOME que Atmosphère cargará para el título del menú, además de su loader y el paquete de recuperación. Un `.nro` de aplicación por sí solo no puede reemplazar `qlaunch`.

La futura herramienta de activación se mantendrá separada: validará una instalación, conservará respaldo de los archivos que gestione y ofrecerá restauración antes de pedir reiniciar. Nunca debe escribir en NAND ni borrar directorios desconocidos de Atmosphère. El objetivo de esta etapa es producir primero un `NovaHome.nro` que abra de forma fiable como aplicación normal.
