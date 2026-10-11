# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/); versionado con [SemVer](https://semver.org/lang/es/).

## [Sin publicar]

## [0.2.0] - 2026-10-12

### Añadido
- Opción `OllamaChat.ReasoningEffort`: nivel de razonamiento que se envía siempre en las peticiones de chat al proveedor OpenAI (p. ej. `none` para `gpt-6-luna`).

### Corregido
- Respuestas vacías con modelos de razonamiento de OpenAI: el razonamiento oculto agotaba `NumPredict` y el texto llegaba vacío sin que el reintento automático se enterase. Ahora se detecta también por `finish_reason` y por los tokens de razonamiento que informa la API.

## [0.1.1] - 2026-10-12

### Corregido
- Proveedor OpenAI: las peticiones fallaban con «HTTP 400: you must provide a model parameter» porque la cabecera `Content-Type` se enviaba duplicada (la añadían a la vez `BuildHeaders()` y `Post()`). Se elimina del mapa de cabeceras; Ollama no se ve afectado.
