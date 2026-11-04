# NOVA HOME

Prototipo visual para PC de una interfaz HOME inspirada en bibliotecas modernas de videojuegos. Está construido con HTML, CSS y JavaScript nativos, sin dependencias de compilación.

## Ejecutar

Abre `index.html` en un navegador moderno. Para cargar imágenes locales, el navegador necesita permitir almacenamiento local. La interfaz se adapta a la resolución disponible; 1280 × 720 y 1920 × 1080 son tamaños objetivo para las pantallas de Switch en modo portátil y TV.

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
- `switch/NovaHome`: app nativa SDL2/libnx, probada en una Switch con Atmosphère.
- `.github/workflows/build-switch.yml`: genera un artefacto `.nro` con devkitPro al actualizar el port nativo.

## Estado del port de Switch

La versión actual ya compila como `NovaHome.nro` y se probó en una Switch con Atmosphère. Lee las aplicaciones instaladas y sus iconos, muestra el perfil, la hora, la batería y las estadísticas de juego cuando libnx permite acceder a esos servicios. Permite iniciar juegos y ordenar la biblioteca por A–Z, tiempo de juego o uso reciente. Las carátulas se muestran en cajas verticales con la seleccionada ampliada, y el logotipo de NOVA está incluido.

El prototipo para PC reproduce archivos de `Songs/`, permite elegir canciones, pausar y ajustar el volumen. Esa reproducción aún no está integrada en el NRO. SteamGridDB tampoco está conectado al port nativo; por ahora, este usa los iconos oficiales incluidos en los datos de cada juego.

### Próximas mejoras

- Añadir navegación con la palanca izquierda.
- Revisar el recorrido con arriba/abajo para que el botón de ordenar y todos los controles puedan seleccionarse.
- Hacer que el carrusel tenga principio y fin según el orden actual, en vez de volver al otro extremo.
- Reproducir los MP3 de `Songs/` en el menú, con controles de canción y volumen.
- Integrar SteamGridDB para buscar y cargar imágenes.
- Completar los menús de controles, ajustes, música y Álbum/Homebrew.

## Alcance de activación

`NovaHome.nro` se ejecuta como una aplicación homebrew independiente. `NovaToggle.nro` y el reemplazo del menú HOME aún no están implementados; son una etapa posterior separada que requiere diseñar y validar la integración con Atmosphère y su recuperación. Este NRO no modifica los archivos de Atmosphère ni reemplaza `qlaunch`.
