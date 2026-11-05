# NOVA HOME

NOVA HOME es una interfaz HOME nativa para Nintendo Switch con Atmosphère, empaquetada como una aplicación homebrew `.nro`. Este repositorio está centrado actualmente en el port de Switch. El proyecto aún no reemplaza el HOME original de la consola.

## Interfaz y navegación

- El carrusel principal muestra hasta nueve juegos y termina con una tarjeta **Biblioteca**. Ya no se abre con `X`.
- La Biblioteca presenta los juegos en una cuadrícula continua. Al bajar hasta la última fila visible, la cuadrícula se desplaza y muestra las siguientes filas; no hay páginas.
- Las carátulas usan formato vertical de caja física y el título va centrado sobre la imagen. Los nombres del carrusel también se centran.
- El panel inferior pone el logotipo del juego a la izquierda y, a la derecha, los botones `A Iniciar` y `+ Opciones`, seguidos por el tiempo de juego.
- El orden se puede elegir entre título A–Z, tiempo de juego y último jugado. La selección se confirma desde el menú Ordenar y se conserva entre sesiones; por defecto usa último jugado.
- D-pad y stick izquierdo navegan por la interfaz y los menús. En Biblioteca, el stick y el D-pad mueven la selección por filas. `A` abre juegos o Biblioteca; `+` muestra las opciones del juego seleccionado; `B` vuelve.
- La pantalla táctil permite seleccionar y arrastrar el carrusel. La Biblioteca permite tocar directamente una carátula.
- Perfil, hora, batería, controles, ajustes, música y Álbum están dentro de la pantalla principal. Mantener `R` al abrir Álbum solicita el visor original de capturas de la Switch.

## Personalización con SteamGridDB

Desde `+ Opciones` se pueden cambiar la carátula, el logotipo y el fondo del juego. Las mismas opciones están disponibles sobre un juego dentro de Biblioteca. La aplicación mantiene la selección ligada al ID del juego durante toda la búsqueda y descarga, guarda el archivo en la SD y lo vuelve a cargar para ese mismo juego; si se personaliza desde Biblioteca, el fondo y el logotipo se verán al seleccionar el título en el carrusel.

El flujo muestra resultados de búsqueda, categorías, miniaturas y una vista previa antes de aplicar la imagen. También permite buscar un nombre manualmente, borrar el arte aplicado, restaurar la apariencia original y borrar o cambiar la clave API. La opción de icono personalizado se quitó porque SteamGridDB estaba reemplazando la carátula en lugar de cambiar el icono esperado; NOVA conserva los iconos oficiales que proporciona el sistema.

La clave se guarda solo en `sd:/switch/NovaHome/steamgriddb.key`; no está incluida en el código ni en GitHub. Las imágenes personalizadas se guardan asociadas al ID del juego en `sd:/switch/NovaHome/art/`, y las miniaturas en `sd:/switch/NovaHome/cache/`. Las consultas HTTPS y descargas se ejecutan en una tarea aparte para mantener la interfaz activa. Hay validación de tipo y tamaño de imagen, reintentos ante algunos errores de red y reemplazo seguro del archivo existente.

## Música y fuente

El menú Música reproduce aleatoriamente al iniciar una de las canciones MP3 empaquetadas desde `Songs/`. Desde su panel se puede pausar, cambiar de pista y ajustar el volumen. La música se limita al menú y se detiene al iniciar un juego. Para añadir más pistas, coloca los archivos `.mp3` en `Songs/` y vuelve a compilar.

La interfaz usa Manrope: Semibold 600 para nombres de juegos, Medium 500 para títulos y menús, Regular 400 para texto descriptivo y Bold 700 para opciones seleccionadas. El logotipo de NOVA se usa como icono de la aplicación en Homebrew y no se dibuja sobre la pantalla principal.

## Compilar e instalar

Requiere devkitPro con `devkitA64`, `libnx`, `switch-sdl2`, `switch-sdl2_gfx`, `switch-sdl2_image`, `switch-sdl2_ttf` y `switch-sdl2_mixer`. En PowerShell, desde la carpeta del repositorio:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-nro.ps1
```

El resultado es `switch/NovaHome/NovaHome.nro`. Cópialo a `sd:/switch/NovaHome/NovaHome.nro` y ejecútalo desde el Homebrew Menu. El script compila desde una copia temporal para admitir rutas de Windows con espacios; los MP3 de `Songs/` y los pesos de Manrope se copian a RomFS durante la compilación.

## Estado y alcance

El NRO funciona como aplicación homebrew independiente y usa los servicios de Switch disponibles para listar títulos instalados, abrir juegos y consultar información del sistema. Los cambios recientes amplían la navegación, reorganizan la pantalla principal y la Biblioteca, y permiten guardar arte SteamGridDB por juego. La conexión de red, la carga de imágenes descargadas y los nuevos cambios visuales deben validarse en la consola con cada versión compilada.

## NOVA Manager (prototipo nativo)

Se agregó `switch/NovaManager/NOVAManager.nro`, una aplicación independiente de NOVA HOME con pantallas de Resumen, Archivos y Recuperación. Esta compilación simula la activación, desactivación y reinicio en memoria: no inspecciona ni escribe la SD, no modifica Atmosphère y no reemplaza el HOME. La integración real no está implementada.

Compílalo con `powershell -ExecutionPolicy Bypass -File .\build-manager-nro.ps1`. El análisis técnico, la arquitectura propuesta y los requisitos de recuperación están en [docs/NOVA-MANAGER-ARCHITECTURE.md](docs/NOVA-MANAGER-ARCHITECTURE.md). La referencia HomeNX y la documentación de Atmosphère se usan para orientar el diseño; no se copió su implementación.
