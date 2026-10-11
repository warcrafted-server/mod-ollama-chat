# Plan: bots que pasen por jugadores humanos

Objetivo: que alguien que entre en el servidor no sepa si habla con un bot o con una persona.
Repo: `~/Repos/mods/mod-ollama-chat` (versión actual 0.1.1). Se prueba en **acore-test** (reino 2,
modelo `gpt-6-luna` vía OpenAI). Normas obligatorias: `CLAUDE.md` del módulo (hilos, snapshot de
config, canales por id) y `~/Repos/mods/CLAUDE.md`. No parchear el core ni tocar `~/Servers/*`.

## Diagnóstico (2026-10-12, comprobado)

1. **Los bots no dicen nada vía LLM.** En `Server.log` de acore-test todas las generaciones dan
   `Generation ok ... 0 chars`. Reproducido con el prompt real: `gpt-6-luna` razona en oculto,
   agota `max_completion_tokens=100` (`finish_reason: length`, `reasoning_tokens: 100`, contenido
   vacío). OpenAI no devuelve el texto del razonamiento, así que el self-heal de
   `QueryOllama` (`mod-ollama-chat_api.cpp`, exige `!result.thinking.empty()`) nunca salta. Con
   `"reasoning_effort": "none"` responde bien y en español (valores admitidos: none, low, medium,
   high, xhigh; `minimal` da 400).
2. **El inglés que se ve no sale de este módulo** (que hoy está mudo). Fuente probable:
   playerbots, con `Playerbots.RandomBotTalk = 1` y `Playerbots.EnableBroadcasts = 1` (frases
   enlatadas de botín, misiones, nivel). Sin verificar si `ai_playerbot_texts` tiene textos esES.
3. **Datos del prompt en inglés, fijos en código** (`handler.cpp` ~1940-1960: `Male/Female`,
   `Alliance/Horde`, `In a group/Solo`, `No Guild`, `The player`; clase/raza de playerbots en
   inglés; prompt de gestos en `expression.cpp:369`; textos de `roleplay.cpp` y `memory.cpp`).
   Nombres de hechizos/objetos/misiones también en inglés (`A Troubling Breeze`).
4. **Sin hilo:** el historial son 5 pares por bot-jugador pegados como texto en un solo mensaje
   `user`; no hay historial compartido del canal (un bot no ve lo que dijeron otros).
5. **Delatores de bot:** respuesta en 1,5-2 s, siempre contesta alguien (60 % por bot × ~20 bots
   en General), ortografía perfecta con `¿`, y el `SystemPrompt` actual obliga a «devolver algo, una
   pregunta» → casi todo acaba en pregunta (en el fork `-es` era el delator nº 1:
   `mod-ollama-chat-es/QUALITY_EVALUATIONS.md`). Las muestras de `gpt-6-luna` confirman el tono de
   asistente: «¿De qué te apetece hablar?».

Referencia útil: `~/Repos/mods/mod-ollama-chat-es` (fork de producción) ya resolvió parte de 3 y 4
(«Instrucción de idioma», «Nombres localizados», «Historial de canal compartido»: ver
`CHANGES_VS_UPSTREAM.md`). Portar ideas, no copiar a ciegas: el código base difiere.

## Principios de diseño (para todos los pasos)

- Lo mecánico (tiempos, minúsculas, erratas, abreviaturas) se hace **en C++ de forma
  determinista**, no se le pide al LLM: el modelo lo exagera y gasta tokens.
- Al LLM se le pide solo contenido y registro, con poco dato y concreto (ver `CLAUDE.md`,
  «Prompt content»). Menos dato = menos frases sobre su oro o su nivel.
- Callar es humano: la mayoría de mensajes en General no tienen respuesta.
- Cada bot tiene un estilo estable (derivado de su guid), no todos escriben igual.
- Toda opción nueva en `conf/mod_ollama_chat.conf.dist` con su bloque de comentario; valores por
  defecto genéricos (el repo es el fork del upstream en inglés) y el idioma se configura.
- Medir antes y después de cada paso (paso 7).

## Pasos

### Paso 0 — Configuración del servidor de pruebas (usuario, sin código)
- En `~/Servers/acore-test/etc/modules/playerbots.conf`: `Playerbots.RandomBotTalk = 0` y
  `Playerbots.EnableBroadcasts = 0` (o comprobar antes que `ai_playerbot_texts` tiene esES).
- En `mod_ollama_chat.conf`: `OllamaChat.DebugShowFullPrompt = 1` temporalmente; activar el log de
  chat (los `chat_*.log` están vacíos) para tener muestras.
- Rotar la `ApiKey` de OpenAI (se mostró en claro en una sesión).
- Terminado: en juego ya no aparecen frases enlatadas en inglés.

### Paso 1 — Respuestas vacías por razonamiento oculto · v0.2.0 (menor: añade opción)
- Archivos: `src/mod-ollama-chat_api.cpp`, `src/mod-ollama-chat_api.h`,
  `src/mod-ollama-chat_config.{h,cpp}`, `conf/mod_ollama_chat.conf.dist`.
- `ParseOpenAIBody`: leer `choices[0].finish_reason` y
  `usage.completion_tokens_details.reasoning_tokens` y llevarlos en `OllamaApiResult`.
- Self-heal: tratar «ok + texto vacío + (`thinking` no vacío **o** `reasoning_tokens > 0`)»
  como razonamiento incondicional; reintentar con la reserva. Mensaje de error final igual.
- Opción `OllamaChat.ReasoningEffort` (vacío por defecto = comportamiento actual). Si tiene valor
  y el proveedor es OpenAI, se envía siempre como `reasoning_effort` en las peticiones de chat
  (no en las de memoria/sentimiento si piden razonar). Para acore-test: `none`.
- Va por el snapshot de config (`OllamaConfig_Publish`), nunca leído como global desde el worker.
- Terminado: con `NumPredict = 100` y `ReasoningEffort = none`, el log muestra `N chars > 0`; sin
  la opción, el self-heal reintenta en vez de dar 0.
- Ejecutor: AgentRelay, esfuerzo medio. CHANGELOG: «Corregido: respuestas vacías con modelos de
  razonamiento de OpenAI… Añadido: `OllamaChat.ReasoningEffort`».

### Paso 2 — Prompt sin inglés · v0.3.0
- Archivos: `src/mod-ollama-chat_handler.cpp`, `src/mod-ollama-chat_expression.cpp`,
  `src/mod-ollama-chat_roleplay.cpp`, `src/mod-ollama-chat_memory.cpp`, conf.
- Sustituir los literales fijos (género, facción, grupo, hermandad, «The player», prompt de gestos,
  frases de roleplay/memoria/heridas) por opciones de conf con su valor inglés por defecto. Para
  agruparlos, una opción por bloque mejor que 30 sueltas (p. ej. `OllamaChat.Words.*`).
- Clase y raza: nombre localizado desde DBC (`sChrClassesStore`/`sChrRacesStore`, campo de nombre
  con el índice de `OllamaChat.PromptLocale`, p. ej. `esES`) en vez de `ChatHelper::FormatClass`.
- Hechizo, objeto, misión, criatura y zona: usar la tabla `*_locale` / `ObjectMgr::Get*Locale` del
  locale configurado cuando exista; si no, el nombre original. Mirar cómo lo hace el fork `-es`.
- Terminado: con `DebugShowFullPrompt = 1` y conf en español, no queda ninguna palabra inglesa
  generada por el código en el prompt de una respuesta de canal ni de un evento.
- Ejecutor: AgentRelay, esfuerzo medio, en dos tareas (handler / resto).

### Paso 3 — Hilo de conversación · v0.4.0 (diseño delicado: revisa Opus)
- Archivos: `src/mod-ollama-chat_dispatch.{h,cpp}`, `src/mod-ollama-chat_api.{h,cpp}`,
  `src/mod-ollama-chat_handler.cpp`, conf.
- **Historial de ámbito:** buffer por clave de ámbito del governor (canal por id+nombre real,
  grupo, hermandad, decir por zona) con las últimas `OllamaChat.ScopeHistory.Lines` (def. 12)
  líneas `{hablante, texto, hora}` de **todos** (humanos y bots). Se escribe en el world thread
  (al recibir un mensaje y al entregar la respuesta en `OllamaDispatch_Update`); se lee y copia al
  `OllamaChatRequest` al construirlo. Caducidad `OllamaChat.ScopeHistory.MaxAgeSeconds` (def. 600).
- **Mensajes reales:** `OllamaChatRequest` lleva `std::vector<{role, content}>` con el historial;
  `BuildOpenAIRequest`/`BuildAnthropicRequest` lo envían como turnos (`user` = lo que dijeron otros
  con «Nombre: texto», `assistant` = lo que dijo este bot). Ollama `/api/generate` sigue con texto
  plano. El prompt de instrucciones va en `system`, no en el último `user`.
- Mantener el historial privado bot-jugador para susurros y como memoria.
- Terminado: en una prueba de 3 mensajes seguidos en General, el bot cita o sigue lo que dijo otro
  bot o jugador; ningún acceso a `Player*`/`Channel*` fuera del world thread (revisión manual).
- Ejecutor: AgentRelay, esfuerzo alto, en dos tareas (buffer + request / builders de API).
  Revisión del diff por Opus.

### Paso 4 — Tiempos y volumen humanos · v0.5.0
- Archivos: `src/mod-ollama-chat_handler.cpp`, `src/mod-ollama-chat_dispatch.cpp`,
  `src/mod-ollama-chat_governor.cpp`, conf.
- **Latencia:** retraso medido desde que llega el mensaje (el tiempo del LLM cuenta dentro):
  lectura `1-4 s` + escritura a `OllamaChat.Human.CharsPerSecond` (def. 4) + cola larga ocasional
  (`OllamaChat.Human.DistractedChance` def. 15 %, +10-40 s). Sustituye a la simulación actual
  cuando `OllamaChat.Human.Enable = 1`. La entrega retrasada se hace en `OllamaDispatch_Update`.
- **Volumen:** en canales públicos, en lugar de tirar dado por bot, una tirada por mensaje:
  `OllamaChat.Human.ChannelReplyChance` (def. 45 % pregunta abierta, 20 % afirmación), luego 1
  bot, y 2 solo con `SecondReplyChance` (def. 15 %) separados ≥ 5 s. Mención directa por nombre:
  responde ese bot (90 %). Grupo/decir cerca: más alto que canal.
- Terminado: en 20 mensajes de prueba en General, ~40 % sin respuesta, nunca 2 respuestas en el
  mismo segundo, mediana de latencia ≥ 5 s.
- Ejecutor: AgentRelay, esfuerzo medio-alto.

### Paso 5 — Estilo de escritura por bot · v0.6.0
- Archivos nuevos `src/mod-ollama-chat_style.{h,cpp}`; enganche en `dispatch.cpp` tras
  `ProcessLlmResponse`; conf.
- Perfil estable por guid (hash → semilla): mayúscula inicial sí/no, punto final, `¿` de apertura
  (la mayoría no), tildes (algunos las quitan), abreviaturas (`q`, `xq`, `tb`, `pq`, `x`, `k`),
  risa (`jaja`, `xd`), tasa de erratas (0-3 %, intercambio de letras vecinas en teclado) con
  corrección `*palabra` en un segundo mensaje a veces, y probabilidad de partir en dos líneas.
- Todo con `OllamaChat.Style.*` y `OllamaChat.Style.Enable`. Listas de abreviaturas en conf.
- No tocar enlaces de objeto `|H...|h`, nombres propios ni códigos de color.
- Terminado: test unitario simple o función de prueba con 3 perfiles distintos sobre la misma
  frase; nada roto en enlaces.
- Ejecutor: AgentRelay, esfuerzo medio.

### Paso 6 — Prompt y personalidad · v0.7.0 (redacción: Opus o Sonnet alto, no delegar a ciegas)
- Archivos: `conf/mod_ollama_chat.conf.dist` (y bloque de ejemplo en español para pegar en el
  servidor, en `docs/` o comentado en el conf).
- Quitar del `SystemPrompt` la orden de devolver siempre una pregunta; pedir lo contrario:
  «muchas veces no preguntas nada; a veces respondes con dos palabras».
- Longitud objetivo **muestreada por respuesta** en C++ (1-3 / 4-8 / 9-15 palabras, pesos
  50/35/15) y pasada al prompt con `{max_words}`, en vez de un «menos de 15 palabras» fijo.
- Unos pocos ejemplos de registro de chat real español de WoW (frases cortas, sin `¿`, con
  `jaja`, `ni idea`), marcados como estilo y no como contenido.
- Quitar del prompt datos que el bot tiende a recitar (oro, distancia exacta, «Especialidad»).
- No saberlo todo: permitido «ni idea», equivocarse un poco, no dar guías completas.
- Filtro de frases de asistente (`¿en qué puedo ayudarte`, `¡claro!`, `por supuesto`, `no dudes
  en`…) configurable: si aparece, se descarta la línea.
- Terminado: métricas del paso 7 dentro de objetivo.

### Paso 7 — Medición · se hace antes del paso 1 (línea base) y tras cada paso
- Script `apps/chat_metrics.py` (sin dependencias) que lea el log de chat o `Server.log` y dé:
  % de líneas de bot acabadas en `?`, % con `¿`, mediana y p90 de palabras, % con mayúscula
  inicial, latencia mediana, % de mensajes humanos sin respuesta, % de líneas con palabras
  inglesas (lista corta).
- Objetivos: `?` final < 20 %, mediana ≤ 7 palabras, latencia mediana 5-15 s, 30-60 % sin
  respuesta en General, 0 inglés.
- Prueba ciega periódica (manual): mezclar 20 líneas humanas reales y 20 de bots y que alguien las
  clasifique. Anotar resultados en `docs/EVALUACIONES.md`.
- Ejecutor: AgentRelay, esfuerzo bajo.

## Orden recomendado
0 → 7 (línea base) → 1 → 2 → 6 → 3 → 4 → 5, midiendo tras cada uno. El 1 desbloquea todo; el 6 es
barato y de mucho impacto; 3 y 4 son los más delicados.

## Estado
- [ ] 0 · [ ] 7 · [ ] 1 · [ ] 2 · [ ] 6 · [ ] 3 · [ ] 4 · [ ] 5
