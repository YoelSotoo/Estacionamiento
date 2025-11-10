Estacionamiento - Instrucciones de sincronización (Scanner ↔ UI ↔ ESP32)

Resumen rápido
- Este repositorio contiene:
  - `Estacionamiento.html` (interfaz principal para ver y controlar espacios)
  - `camera_scan.html` (scanner QR que usa la cámara)
 Estacionamiento demo — instrucciones mínimas (sin ESP32 ni relay)

 Archivo principales en este folder:
 - `Estacionamiento.html`  — interfaz principal (control y timers)
 - `camera_scan.html`     — escáner QR que guarda el comando en localStorage
 - `styles.css`           — estilos

 Modo sin servidor central (recomendado):
 - Sirve ambos archivos desde el mismo origen (misma IP y puerto) y ábrelos en el mismo navegador (distintas pestañas) o en dispositivos que apunten al mismo host+puerto.

 Prueba rápida (Windows / PowerShell):
 ```powershell
 cd "C:\Users\sotoy\Desktop\Estacionamiento"
 python -m http.server 8000
 # desde el mismo equipo abre:
 # http://localhost:8000/Estacionamiento.html
 # http://localhost:8000/camera_scan.html
 ```

 Flujo esperado:
 - Abre `Estacionamiento.html` en una pestaña (será la que controle los espacios).
 - Abre `camera_scan.html` en otra pestaña (puede ser la misma máquina o dispositivo que comparte origen).
 - En `camera_scan.html` la cámara intentará iniciarse automáticamente; si no, pulsa "Iniciar cámara".
 - Al escanear un QR, el escáner escribe `parkingCommand` en localStorage y muestra confirmación.
 - La pestaña con `Estacionamiento.html` escucha storage events y procesará el comando para iniciar el tiempo del espacio.

 Notas:
 - localStorage/storage events funcionan entre pestañas del mismo origen (protocolo+host+puerto). Si las páginas están en distintos orígenes, no funcionará.
 - Algunos navegadores pueden bloquear el inicio automático de la cámara por razones de seguridad; si eso ocurre prueba con Firefox móvil/desktop o pulsa el botón manualmente.

 Si quieres que deje solo el escáner y elimine `Estacionamiento.html` y `styles.css` del repo, o que haga algún ajuste (ej.: auto‑start distinto, notificaciones, QR en JSON), dime cuál y lo hago.

