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
- `switch/NovaHome`: primera app nativa para comprobar arranque y mando en Atmosphère.
- `.github/workflows/build-switch.yml`: genera un artefacto `.nro` con devkitPro al actualizar el port nativo.

La biblioteca, el tiempo de juego y las acciones del sistema son demostrativos. Las imágenes personalizadas y el último inicio registrado se guardan por identificador de juego en el almacenamiento local del navegador; se admiten imágenes de hasta 8 MB. SteamGridDB y las operaciones de sistema no están conectados.

El panel de Música reproduce las pistas incluidas en `Songs/`, permite elegir una canción, controlar la reproducción y ajustar el volumen. La música se pausa al iniciar un juego o al ocultar la ventana. Para añadir temas al proyecto, coloca los archivos en esa carpeta y registra sus rutas en la lista del reproductor.

## Alcance de Nintendo Switch

La interfaz actual es un prototipo para PC, no es todavía un `.nro`. La fase de Switch debe empaquetarse como aplicación homebrew independiente con libnx. En ese formato, el usuario inicia NOVA HOME desde el entorno homebrew y sale de ella con los controles de salida de la aplicación o el menú HOME. Un `.nro` independiente no puede activar/desactivar el HOME original como si fuera una capa persistente que sigue ejecutándose fuera de la aplicación. Un overlay conmutado globalmente sería otro tipo de integración y requeriría investigación separada; el diseño inicial se mantiene como app independiente para limitar el alcance y los riesgos.

En Switch, la interfaz debe consultar el tamaño real del framebuffer y escalar el diseño para 1280 × 720 y 1920 × 1080. El prototipo web usa reglas adaptativas como aproximación para ambas resoluciones.
