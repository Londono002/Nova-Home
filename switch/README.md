# Port nativo para Nintendo Switch

NOVA HOME es una aplicación homebrew `.nro` construida con SDL2 y libnx para Atmosphère. Enumera los juegos instalados, utiliza los iconos oficiales del sistema y consulta datos de perfil, reloj, batería y tiempo jugado cuando los servicios de la consola los proporcionan. Al elegir un juego lo inicia mediante el servicio del sistema.

## Pantalla principal y Biblioteca

El carrusel contiene hasta nueve juegos y una décima tarjeta de Biblioteca. La tarjeta abre una cuadrícula que muestra cinco cajas verticales por fila. La cuadrícula es continua: al navegar hacia abajo, desplaza las filas visibles y sigue recorriendo la lista sin dividirla en páginas. El título de cada juego aparece centrado sobre la carátula; en el carrusel también se centra el nombre.

El panel inferior del carrusel coloca el logotipo del juego a la izquierda. A la derecha muestra, en orden, `A Iniciar`, `+ Opciones` y el tiempo jugado. Al seleccionar Biblioteca en el carrusel, `A` la abre. Dentro de la biblioteca, `A` inicia el juego y `+` abre las mismas opciones disponibles en el carrusel.

El menú Ordenar permite elegir entre título A–Z, tiempo jugado y último jugado. El orden solo cambia al confirmar la opción y queda guardado en `sd:/switch/NovaHome/preferences.cfg`; el valor inicial es último jugado. El carrusel tiene límites, permite arrastrar con la pantalla táctil y anima el desplazamiento y el aumento de la tarjeta seleccionada. El stick izquierdo y el D-pad navegan por carrusel, Biblioteca y paneles.

Las tarjetas de juego usan carátulas verticales. Los fondos panorámicos se ajustan a la pantalla 16:9 sin deformarse. NOVA conserva en memoria las texturas de los nueve títulos principales y limita la carga de carátulas de Biblioteca a las filas visibles para reducir el uso de memoria. Las imágenes descargadas que superan las dimensiones de trabajo se reducen antes de crear sus texturas.

## Opciones visuales y SteamGridDB

En `+ Opciones`, las acciones por juego incluyen iniciar, consultar información, cambiar carátula, cambiar logotipo, cambiar fondo, restaurar apariencia y eliminar el juego. El flujo SteamGridDB también está disponible desde el menú `+` de cada título en Biblioteca. Una personalización iniciada desde Biblioteca se guarda para el juego seleccionado y su fondo o logotipo se carga cuando el título vuelve a aparecer en el carrusel.

La ventana de SteamGridDB permite buscar el nombre asociado al juego o escribir otra búsqueda, elegir el resultado correcto, consultar categorías, explorar miniaturas y previsualizar antes de descargar. Desde su pantalla inicial se puede borrar el arte de esa categoría, borrar la clave o introducir/cambiarla. La opción de icono personalizado se retiró: el formato de iconos de SteamGridDB no correspondía al icono de aplicación esperado y cambiaba la carátula.

La API key se guarda localmente en `sd:/switch/NovaHome/steamgriddb.key` y no se almacena en el código. El arte final se guarda por ID de aplicación en `sd:/switch/NovaHome/art/`; las miniaturas se almacenan en `sd:/switch/NovaHome/cache/`. Se utiliza HTTPS, límites de tamaño, validación básica de formato, reintentos para errores temporales y escritura por archivo temporal para reemplazar arte existente. Borrar una personalización o restaurar la apariencia elimina el archivo asociado. Las solicitudes de red ocurren en una tarea aparte.

## Música y fuentes

NOVA busca archivos `.mp3` en `romfs:/songs`, que se llena desde la carpeta `Songs/` del repositorio al compilar. Al iniciar el menú escoge una pista al azar; el panel Música permite pausar, avanzar o retroceder y ajustar el volumen. La reproducción se detiene cuando se inicia un juego.

La interfaz usa los pesos Regular 400, Medium 500, SemiBold 600 y Bold 700 de Manrope, empaquetados en RomFS. El logotipo de NOVA se usa como icono de la aplicación en Homebrew; no se dibuja en la pantalla principal.

## Compilación

Requiere devkitPro, `devkitA64`, `libnx`, `switch-sdl2`, `switch-sdl2_gfx`, `switch-sdl2_image`, `switch-sdl2_ttf` y `switch-sdl2_mixer`. Desde PowerShell, en la raíz del repositorio:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-nro.ps1
```

El resultado es `switch/NovaHome/NovaHome.nro`. Cópialo a `sd:/switch/NovaHome/NovaHome.nro` y ejecútalo desde el Homebrew Menu. La compilación copia los MP3 y las fuentes de Manrope a RomFS.

## NOVA Manager

`../NovaManager/NOVAManager.nro` es una aplicación independiente. Su prototipo muestra el estado, archivos esperados y recuperación, pero sus acciones solo se simulan en memoria. No lee/escribe la SD, no cambia Atmosphère, no reinicia la consola y no reemplaza el HOME. Compila desde la raíz del repositorio con `build-manager-nro.ps1`.

La integración real continúa sin implementarse. El diseño técnico y sus condiciones de seguridad están documentados en `../docs/NOVA-MANAGER-ARCHITECTURE.md`.
