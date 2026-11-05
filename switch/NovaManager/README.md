# NOVA Manager

Aplicación independiente `.nro` para administrar NOVA. Esta primera compilación es un prototipo nativo de Switch: las opciones de activar, desactivar, comprobar archivos y reiniciar son simulaciones en memoria. No lee ni escribe la microSD, no cambia Atmosphère y no reemplaza el HOME.

## Controles

- Izquierda / derecha: cambiar entre Resumen, Archivos y Recuperación.
- Arriba / abajo: recorrer las acciones del Resumen.
- `A`: seleccionar o confirmar una simulación.
- `B`: volver o cancelar.
- `+`: salir.

## Compilar

Requiere devkitPro, devkitA64, libnx, `switch-sdl2`, `switch-sdl2_gfx` y `switch-sdl2_ttf`.

Desde la raíz del repositorio:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-manager-nro.ps1
```

El resultado es `switch/NovaManager/NOVAManager.nro`. El icono y Manrope se empaquetan en el NRO.

## Antes de una integración real

La interfaz no habilita la instalación real. El formato del paquete de integración, la estrategia de respaldo y restauración, la comprobación de compatibilidad, el reinicio y la recuperación deben definirse y validarse primero. Consulta `docs/NOVA-MANAGER-ARCHITECTURE.md`.
