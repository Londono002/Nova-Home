# NOVA HOME

Prototipo visual para PC de una interfaz HOME inspirada en bibliotecas modernas de videojuegos. Está construido con HTML, CSS y JavaScript nativos, sin dependencias de compilación.

## Ejecutar

Instala Node.js 18 o posterior y ejecuta `node server.js` desde la carpeta del proyecto. Abre `http://127.0.0.1:8787` en el navegador. El servidor es necesario para el puente local de SteamGridDB, que evita el bloqueo CORS del navegador. La interfaz web se adapta a 1280 × 720 y 1920 × 1080.

## Controles

- `←` / `→`: cambiar de juego.
- `+`: abrir las opciones del juego.
- `Enter`: iniciar la acción de demostración.
- `Esc`: cerrar un diálogo; en la pantalla principal, abrir opciones.
- Ratón: al pasar sobre una tarjeta, seleccionarla; el carrusel la resalta y amplía.
- **Ordenar**: ordenar por última vez abierto, tiempo de juego o título de la A a la Z.

También se puede navegar con el ratón y los botones visibles.

Sobre el fondo del juego, la pantalla HOME reúne Perfil, reloj y batería en un recuadro a la izquierda, y accesos de icono a Mandos, Ajustes, Música y Álbum / Homebrew a la derecha. No muestra un título de NOVA HOME en la parte superior. En PC, estos paneles son demostrativos: no consultan los Joy-Con ni las aplicaciones instaladas en una Switch.

## Estructura

- `index.html`: estructura de la interfaz.
- `styles.css`: tema, componentes, animaciones y adaptación de pantalla.
- `app.js`: biblioteca de demostración, renderizado, navegación, diálogos, reproducción de música y persistencia de arte.
- `switch/NovaHome`: app nativa SDL2/libnx para Atmosphère con biblioteca real, navegación con stick/D-pad, carrusel finito, música MP3 y el logo como icono de Homebrew.
- `build-nro.ps1`: compila el NRO desde Windows sin fallar por espacios en la ruta.
- `server.js`: servidor local estático y puente limitado para solicitudes a SteamGridDB.
- `.github/workflows/build-switch.yml`: genera un artefacto `.nro` con devkitPro al actualizar el port nativo.

## Estado del port de Switch

El NRO compila localmente. La versión probada anteriormente en Switch ya leía las aplicaciones instaladas y sus iconos, consultaba el perfil, la hora, la batería y las estadísticas disponibles, e iniciaba juegos. Este avance añade navegación por stick, carrusel finito y música MP3 nativa; la última compilación debe probarse en consola. El logo de NOVA queda como icono de la aplicación en Homebrew y no se dibuja en la pantalla principal.

En Switch, Música reproduce los MP3 de `Songs/`, permite elegir pista, pausar y cambiar el volumen. Al compilar, Makefile los copia a RomFS. En PC, Ajustes permite guardar una clave de SteamGridDB solo en el navegador, probarla, buscar el juego y elegir carátula, fondo o icono. La clave no se incluye en el repositorio. SteamGridDB está conectado al prototipo PC; el NRO sigue usando los iconos oficiales instalados.

Para SteamGridDB, crea una cuenta y genera una API key en [las preferencias de SteamGridDB](https://www.steamgriddb.com/profile/preferences/api). Pégala en Ajustes dentro de NOVA. La conexión requiere ejecutar `server.js`; abrir `index.html` directamente no permite usar el puente por las restricciones de origen del navegador.

### Próximas mejoras

- Completar la navegación de todos los menús y controles.
- Guardar la preferencia de orden de la biblioteca entre sesiones.
- Integrar SteamGridDB con el port nativo si la experiencia de consola lo requiere.
- Completar los menús de controles, ajustes, música y Álbum/Homebrew.

## Alcance de activación

`NovaHome.nro` se ejecuta como una aplicación homebrew independiente. `NovaToggle.nro` y el reemplazo del menú HOME aún no están implementados; son una etapa posterior separada que requiere diseñar y validar la integración con Atmosphère y su recuperación. Este NRO no modifica los archivos de Atmosphère ni reemplaza `qlaunch`.
