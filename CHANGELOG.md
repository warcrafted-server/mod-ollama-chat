# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/); versionado con [SemVer](https://semver.org/lang/es/).

## [Sin publicar]

## [0.7.0] - 2026-10-12

### Añadido
- Volumen humano en canales públicos: `OllamaChat.Human.ChannelReplyChance`, `.ChannelQuestionChance` y `.SecondReplyChance` hacen que muchos mensajes se queden sin respuesta y que normalmente conteste un solo bot. Los bots mencionados por nombre no pasan por esa puerta. Por defecto 100 (sin cambio).
- Retraso de entrega humano (`OllamaChat.Human.Enable` y `.ReadMinMs`, `.ReadMaxMs`, `.CharsPerSecond`, `.DistractedChance`, `.DistractedMinMs`, `.DistractedMaxMs`, `.MaxDelayMs`): lectura + escritura + despiste ocasional, medido desde que se envía la petición, de modo que el tiempo del modelo cuenta dentro del retraso. Desactivado por defecto.

## [0.6.0] - 2026-10-12

### Añadido
- Los bots ven lo que se ha dicho en el canal, grupo o hermandad: el historial de ámbito se envía a OpenAI y Anthropic como turnos `user`/`assistant`, y a Ollama como bloque de texto al inicio del prompt. Se aplica a respuestas, charla aleatoria y charla de eventos.
- Opciones `OllamaChat.ScopeHistory.TextHeader` y `OllamaChat.ScopeHistory.SelfLabel` para el formato en texto plano de Ollama.

### Cambiado
- Si hay historial de ámbito, se omite el historial plano por pareja bot–jugador para no duplicar contexto. Los susurros siguen usándolo.

## [0.5.0] - 2026-10-12

### Añadido
- Historial de conversación compartido por ámbito (canal, grupo, hermandad, decir): guarda lo que dicen humanos y bots en cada sitio. Opciones `OllamaChat.ScopeHistory.Enable`, `.Lines` y `.MaxAgeSeconds`. De momento solo se registra; se enviará al modelo en la siguiente versión.

## [0.4.0] - 2026-10-12

### Añadido
- Marcador `{max_words}` en las plantillas de prompt (chat, gesto, charla aleatoria y eventos) y opción `OllamaChat.ReplyLength.Buckets` (`MAX:PESO,...`) para muestrear un tope de palabras distinto en cada respuesta. Por defecto `15:100`, igual que antes.
- Opción `OllamaChat.AssistantPhrases`: lista de frases típicas de asistente (`en qué puedo ayudarte`, `no dudes en`…); la respuesta que las contiene se descarta.

## [0.3.0] - 2026-10-12

### Añadido
- Opciones `OllamaChat.Words.*` (género, facción, grupo, hermandad y «el jugador») para que el prompt use palabras de tu idioma; los valores por defecto siguen en inglés.
- Opción `OllamaChat.PromptLocale` (p. ej. `esES`): clase y raza se muestran con el nombre del DBC en ese idioma.
- Archivo `mod-ollama-chat_words.{h,cpp}` que centraliza esas palabras.

### Cambiado
- `handler`, `events`, `random` y `topics` ya no llevan literales ingleses de género, facción, grupo ni hermandad.

## [0.2.0] - 2026-10-12

### Añadido
- Opción `OllamaChat.ReasoningEffort`: nivel de razonamiento que se envía siempre en las peticiones de chat al proveedor OpenAI (p. ej. `none` para `gpt-6-luna`).

### Corregido
- Respuestas vacías con modelos de razonamiento de OpenAI: el razonamiento oculto agotaba `NumPredict` y el texto llegaba vacío sin que el reintento automático se enterase. Ahora se detecta también por `finish_reason` y por los tokens de razonamiento que informa la API.

## [0.1.1] - 2026-10-12

### Corregido
- Proveedor OpenAI: las peticiones fallaban con «HTTP 400: you must provide a model parameter» porque la cabecera `Content-Type` se enviaba duplicada (la añadían a la vez `BuildHeaders()` y `Post()`). Se elimina del mapa de cabeceras; Ollama no se ve afectado.
