#!/usr/bin/env python3
"""Fail if portable Application line coverage falls below the project threshold."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
THRESHOLD = 98.0
MODULES = [
    "audio_dynamics",
    "audio_processing",
    "bt_profile",
    "connectivity_actions",
    "connectivity_controller",
    "connectivity_service",
    "control_protocol",
    "pcm_transport",
    "signal_quality",
    "telecoil_detector",
    "telecoil_filter",
]

covered_total = 0
executable_total = 0
failed = False

for module in MODULES:
    path = ROOT / f"{module}.c.gcov"
    if not path.exists():
        print(f"[COVERAGE] FAIL: missing {path.name}; run `make -f Makefile.host coverage`")
        failed = True
        continue

    covered = 0
    executable = 0
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        match = re.match(r"^\s*([^:]+):\s*\d+:", line)
        if not match:
            continue
        token = match.group(1).strip()
        if token == "-":
            continue
        if token == "#####" or token.startswith("====="):
            executable += 1
        elif token.isdigit():
            executable += 1
            covered += 1

    percent = (100.0 * covered / executable) if executable else 100.0
    covered_total += covered
    executable_total += executable
    print(f"[COVERAGE] {module:28s} {percent:6.2f}% ({covered}/{executable})")

if executable_total:
    total_percent = 100.0 * covered_total / executable_total
else:
    total_percent = 0.0

print(f"[COVERAGE] Application aggregate      {total_percent:6.2f}% "
      f"({covered_total}/{executable_total}); threshold {THRESHOLD:.1f}%")

if failed or total_percent < THRESHOLD:
    print("[COVERAGE] FAIL")
    sys.exit(1)
print("[COVERAGE] OK")
