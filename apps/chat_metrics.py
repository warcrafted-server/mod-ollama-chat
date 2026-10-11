#!/usr/bin/env python3
"""Mide rasgos de las respuestas de bots en un Server.log de AzerothCore."""

import argparse
from collections import Counter, deque
from datetime import datetime
import math
import re
import statistics
import sys

DEFAULT_LOG = "/home/stark/Servers/acore-test/logs/Server.log"
BOT_LINE = re.compile(r"\[Ollama Chat\]\s+.+?\s+\([^\r\n]*?,\s*depth\s+\d+\):\s*(.*)$")
HUMAN_LINE = re.compile(r"\[Ollama Chat\]\s+Player\s+.+?\s+sent msg: '(.*?)'\s+\|\s+Source:")
NO_BOTS = re.compile(r"\[Ollama Chat\]\s+\*\*\*\s+NO BOTS RESPONDING\s+\*\*\*")
SOME_BOTS = re.compile(r"\[Ollama Chat\]\s+\*\*\*\s+\d+ BOTS RESPONDING\s+\*\*\*")
WORD = re.compile(r"[^\W_]+(?:['’][^\W_]+)*", re.UNICODE)
ENGLISH = re.compile(r"\b(?:the|you|your|and|with|for|what|how|lol|bro|hello|hi|guys|anyone|need|looking|group)\b", re.IGNORECASE)
TIMESTAMP = re.compile(r"^\s*(\d{4}-\d\d-\d\d[ T]\d\d:\d\d:\d\d)")


def percentile(values, fraction):
    """Percentil nearest-rank, definido también para muestras pequeñas."""
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]


def analyze(path, since):
    bot_lines = []
    human_count = no_answer = 0
    pending_humans = deque()
    with open(path, "r", encoding="utf-8", errors="replace") as log:
        for line in log:
            if since:
                match = TIMESTAMP.match(line)
                if match:
                    try:
                        if datetime.strptime(match.group(1), "%Y-%m-%d %H:%M:%S") < since:
                            continue
                    except ValueError:
                        pass
            if HUMAN_LINE.search(line):
                human_count += 1
                pending_humans.append(None)
                continue
            if NO_BOTS.search(line):
                if pending_humans:
                    pending_humans.popleft()
                    no_answer += 1
                continue
            if SOME_BOTS.search(line):
                if pending_humans:
                    pending_humans.popleft()
                continue
            match = BOT_LINE.search(line)
            if match:
                bot_lines.append(match.group(1).strip())
    # Los mensajes sin marcador de respuesta no se cuentan como «sin respuesta»:
    # el log debe incluir el marcador de decisión para poder clasificarlos.
    return bot_lines, human_count, no_answer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ruta", nargs="?", default=DEFAULT_LOG)
    parser.add_argument("--since", metavar="YYYY-MM-DD HH:MM:SS")
    args = parser.parse_args()
    try:
        since = datetime.strptime(args.since, "%Y-%m-%d %H:%M:%S") if args.since else None
        lines, humans, unanswered = analyze(args.ruta, since)
    except (OSError, ValueError) as exc:
        print(f"Error al leer el log: {exc}", file=sys.stderr)
        return 2

    count = len(lines)
    word_counts = [len(WORD.findall(line)) for line in lines]
    repeated = sum(n for n in Counter(lines).values() if n > 1)
    unanswered_pct = unanswered * 100 / humans if humans else 0.0
    if not count:
        print("No se encontraron líneas de bot; no hay métricas de texto que calcular.")
        print(f"Mensajes de humano vistos: {humans}")
        print(f"Mensajes sin respuesta: {unanswered} ({unanswered_pct:.1f}%) — {goal(30 <= unanswered_pct <= 60)}")
        return 0

    q_pct = sum(line.endswith("?") for line in lines) * 100 / count
    opening_pct = sum("¿" in line for line in lines) * 100 / count
    median_words = statistics.median(word_counts)
    p90_words = percentile(word_counts, .90)
    capital_pct = sum(bool(line and line.lstrip()[:1].isupper()) for line in lines) * 100 / count
    period_pct = sum(line.endswith(".") for line in lines) * 100 / count
    english_pct = sum(bool(ENGLISH.search(line)) for line in lines) * 100 / count
    repeated_pct = repeated * 100 / count
    rows = [
        ("Líneas de bot analizadas", str(count), "—"),
        ("Acaban en '?'", f"{q_pct:.1f}%", goal(q_pct < 20)),
        ("Contienen '¿'", f"{opening_pct:.1f}%", "—"),
        ("Palabras (mediana / p90)", f"{median_words:g} / {p90_words}", goal(median_words <= 7)),
        ("Empiezan en mayúscula", f"{capital_pct:.1f}%", goal(capital_pct < 70)),
        ("Acaban en punto", f"{period_pct:.1f}%", "—"),
        ("Contienen palabra inglesa", f"{english_pct:.1f}%", goal(english_pct == 0)),
        ("Mensajes de humano vistos", str(humans), "—"),
        ("Humanos sin respuesta", f"{unanswered} ({unanswered_pct:.1f}%)", goal(30 <= unanswered_pct <= 60)),
        ("Líneas de bot repetidas", f"{repeated} ({repeated_pct:.1f}%)", "—"),
    ]
    width = max(len(row[0]) for row in rows)
    print(f"{'Métrica':<{width}}  Valor                 Objetivo")
    print("-" * (width + 39))
    for label, value, result in rows:
        print(f"{label:<{width}}  {value:<21} {result}")
    return 0


def goal(condition):
    return "OK" if condition else "REVISAR"


if __name__ == "__main__":
    sys.exit(main())
