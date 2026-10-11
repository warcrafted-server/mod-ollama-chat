# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/); versionado con [SemVer](https://semver.org/lang/es/).

## [Sin publicar]

## [0.1.1] - 2026-10-12

### Corregido
- Proveedor OpenAI: las peticiones fallaban con «HTTP 400: you must provide a model parameter» porque la cabecera `Content-Type` se enviaba duplicada (la añadían a la vez `BuildHeaders()` y `Post()`). Se elimina del mapa de cabeceras; Ollama no se ve afectado.
