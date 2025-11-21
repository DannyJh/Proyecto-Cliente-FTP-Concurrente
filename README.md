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

El proyecto incluye un `Makefile` para facilitar la compilación. Ejecuta el siguiente comando en la raíz del proyecto: make

### Comandos del Cliente

Una vez establecida la conexión, puedes utilizar los siguientes comandos dentro del shell interactivo (`ftp>`).

**Nota:** A diferencia de otros clientes FTP, el comando `login` requiere ingresar el usuario y la contraseña en una sola línea.

| Comando | Sintaxis | Descripción |
| :--- | :--- | :--- |
| **login** | `login <usuario> <clave>` | Inicia sesión enviando usuario y contraseña. |
| **get** | `get <remoto> <local>` | Descarga un archivo del servidor. Requiere especificar el nombre en el servidor y el nombre con el que se guardará localmente. |
| **put** | `put <local> <remoto>` | Sube un archivo al servidor. Requiere el nombre del archivo local y el nombre con el que se guardará en el servidor. |
| **pwd** | `pwd` | Muestra el directorio de trabajo actual en el servidor. |
| **mkdir** | `mkdir <nombre_dir>` | Crea un nuevo directorio en el servidor. |
| **delete** | `delete <archivo>` | Elimina un archivo específico en el servidor. |
| **quit** | `quit` o `exit` | Cierra la sesión, termina la conexión y sale del programa. |
