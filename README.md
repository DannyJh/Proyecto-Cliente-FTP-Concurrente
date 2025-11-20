# Proyecto-Cliente-FTP-Concurrente

Este es un cliente FTP de línea de comandos desarrollado en C para sistemas Unix (Linux/macOS). Su característica principal es la **concurrencia basada en procesos**, permitiendo realizar transferencias de archivos (subidas y bajadas) en segundo plano mientras se sigue interactuando con el servidor.

## Características

* **Concurrencia:** Utiliza `fork()` para manejar descargas (`get`) y subidas (`put`) en procesos separados, permitiendo múltiples transferencias simultáneas.
* **Modo Pasivo:** Implementa el modo pasivo (PASV) para la transferencia de datos, lo que facilita la conexión a través de firewalls.
* **Manejo de Timeouts:** Incluye detección de tiempos de espera (timeouts) en la conexión de datos para evitar bloqueos.
* **Interfaz de Comandos:** Shell interactivo para enviar comandos al servidor FTP.

## Requisitos

* Compilador GCC.
* Sistema operativo tipo Unix (WSL).
* Utilidad `make`.

## Compilación

El proyecto incluye un `Makefile` para facilitar la compilación. Ejecuta el siguiente comando en la raíz del proyecto:

```bash
make
