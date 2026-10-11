# Pendiente

Plan completo: `.agents/plans/bots-humanos/bots-humanos.PLAN.md`.

## Alta
- Compilar el módulo entero contra el core y desplegarlo en acore-test (reino Desarrollo). Hasta ahora solo hay comprobaciones de sintaxis parciales.
- Aplicar `docs/CONFIG_ES.md` en el `.conf` del servidor de pruebas y probar en juego: respuestas no vacías (`ReasoningEffort = none`), hilo de conversación, ritmo y estilo.
- Medir con `python3 apps/chat_metrics.py` tras unas horas de uso y ajustar los valores a los objetivos del plan.
- Rotar la `ApiKey` de OpenAI del `.conf` de acore-test (se mostró en claro en una sesión).

## Media
- Paso 2c del plan: textos ingleses fijos de los prompts de gestos, roleplay y memoria (`expression.cpp`, `roleplay.cpp`, `memory.cpp`).
- Revisar en `playerbots.conf` (`RandomBotTalk`, `EnableBroadcasts`) las frases enlatadas en inglés de playerbots; comprobar si `ai_playerbot_texts` tiene traducción esES.

## Baja
- Corrección de erratas con un segundo mensaje (`*palabra`), descartada de momento por la complejidad de enviar dos líneas.
