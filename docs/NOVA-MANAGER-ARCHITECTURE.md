# Diseño de NOVA Integration y NOVA Manager

## Alcance actual

NOVA HOME sigue siendo una aplicación NRO independiente. Este trabajo agrega NOVA Manager como otro NRO, separado de NOVA HOME. Su estado de integración es simulado y vive solo mientras Manager está abierto. No accede ni modifica la SD, Atmosphère, los archivos de sistema o el menú HOME original.

La sustitución real del HOME queda expresamente fuera de esta implementación. No se debe activar hasta completar la investigación y revisión del mecanismo, la compatibilidad y la recuperación.

## Análisis del proyecto actual

- `switch/NovaHome/` contiene la interfaz principal SDL2/libnx, sus fuentes y el `.nro` de NOVA HOME.
- `switch/NovaHome/source/main.c` inicia la interfaz como aplicación homebrew; no hay código que la integre con qlaunch ni un instalador de contenido de Atmosphère.
- NOVA puede iniciar juegos y encadenar a hbmenu mediante las APIs del entorno homebrew. Eso no convierte un NRO normal en el proceso del HOME: el NRO no permanece ejecutándose como HOME del sistema.
- `switch/NovaManager/` es una aplicación independiente y no comparte el ejecutable ni el estado del HOME. Su prototipo actual solo representa estados visuales.

## Referencia HomeNX / NXToggle

La documentación pública de HomeNX presenta dos artefactos distintos: un paquete completo que reemplaza HOME y un `NXToggle.nro` que selecciona el frontend, permite alternarlo y solicita reiniciar. Publica el contenido bajo `atmosphere/contents/0100000000001000` y explica que el menú vuelve al HOME original al desactivar HomeNX. También publica una copia de respaldo del `hbmenu.nro` anterior cuando instala su loader. Estos son comportamientos descritos por el proyecto; no se debe asumir que NOVA puede reproducirlos con solo copiar un NRO.

Atmosphère documenta que el módulo `loader` puede redirigir ejecutables de procesos mediante archivos en `atmosphere/contents/<program id>/exefs/` o sustituir una partición ExeFS con `exefs.nsp`. La documentación identifica esto como una vía para reemplazar procesos/aplicaciones y advierte que su semántica tiene detalles específicos. Por ello, una futura NOVA Integration necesitaría el payload/formato que Atmosphère realmente consume; NOVA Manager por sí solo no puede ser el reemplazo de HOME.

Fuentes consultadas:

- [HomeNX: instalación, artefactos, alternancia y recuperación](https://github.com/ad3333m/homenx)
- [NXToggle y flujo de selección/reinicio descritos en releases](https://github.com/ad3333m/homenx/releases)
- [Documentación oficial de Atmosphère: loader y ExeFS replacement](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/loader.md)

## Arquitectura propuesta

```text
NOVA Interface (NRO)
  UI y experiencia de usuario; independiente del instalador

NOVA Integration (paquete versionado)
  Frontend compatible con el mecanismo de carga de HOME
  Metadatos: versión, hash, compatibilidad y archivos esperados

NOVA Manager (NRO independiente)
  Inspecciona, valida, prepara, confirma, activa/desactiva y reporta
  No integra lógica de renderizado del HOME
```

La capa de archivos futura debería ser un módulo pequeño separado de la UI, con operaciones explícitas y pruebas unitarias: inventario, validación por hash/tamaño, copia a temporal, verificación, renombrado atómico cuando el filesystem lo permita y registro de transacción. No borrar ni sobrescribir archivos desconocidos. Si existe una instalación de terceros, si el respaldo no coincide o si la versión no está soportada, cancelar sin mutaciones.

## Contrato de seguridad antes de escribir en la SD

Antes de activar, el diseño deberá responder y probar:

1. Qué archivo exacto hace que Atmosphère cargue el frontend en lugar de qlaunch, y qué formato espera.
2. Cómo detectar temas, parches u otra integración ya instalada y evitar colisiones.
3. Cómo conservar bytes originales verificables, incluso con espacio limitado o errores de escritura.
4. Cómo confirmar cada etapa y recuperar tras pérdida de energía o reinicio inesperado.
5. Qué versión de Atmosphère/firmware se soporta, con hashes y manifiesto del paquete.
6. Qué procedimiento externo permite recuperar la consola si HOME no inicia. El Manager puede no estar disponible en ese estado; por eso se necesita una recuperación manual probada desde PC/SD.
7. Cómo solicitar el reinicio con APIs de libnx y comprobar el resultado al siguiente arranque, sin afirmar éxito antes de verificar.

El modelo recomendado es transaccional: `sin instalar → validando → preparado → activación pendiente de reinicio → verificado`; en desactivación, preservar el payload y volver al estado original solo si la copia válida existe. Un fallo de precondición debe dejar el estado anterior intacto. El protocolo y los nombres de rutas quedan sin definir hasta verificar la integración concreta.

## Archivos y dependencias

Prototipo actual:

- `switch/NovaManager/Makefile`
- `switch/NovaManager/source/main.c`
- `switch/NovaManager/README.md`
- `build-manager-nro.ps1`
- fuentes Manrope copiadas al RomFS del NRO

Dependencias nativas presentes en el proyecto: devkitPro/devkitA64, libnx, SDL2, SDL2_gfx y SDL2_ttf. El prototipo evita servicios de escritura y APIs de reinicio.

Futuro mecanismo real (no implementado): módulo de gestión de archivos libnx (`fsdev`/`stdio` sobre `sdmc:`), hashes criptográficos, manifiesto de compatibilidad, formato de integración Atmosphère validado, y solicitud de reinicio mediante libnx. No se incluye ni instala automáticamente ningún payload.

## Validación por fases

1. Revisar este diseño y acordar el formato/alcance de integración.
2. Investigar un paquete compatible de HOME y su licencia/avisos, además de contrastar el loader de Atmosphère por versiones.
3. Probar validación, transacciones, fallos y restauración con una imagen de SD de prueba, nunca en la tarjeta de uso diario.
4. Revisar la recuperación manual con HOME inutilizable antes de habilitar cualquier escritura real.
5. Solo entonces diseñar una fase real de instalación con confirmación explícita.
