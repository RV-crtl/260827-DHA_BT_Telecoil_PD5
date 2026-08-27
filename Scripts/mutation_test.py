#!/usr/bin/env python3
"""Small deterministic mutation test proving the unit suite detects a broken BT-priority rule."""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Application" / "Src" / "connectivity_controller.c"
ORIGINAL = """    if (controller->bluetooth_enabled && inputs->bluetooth_connected) {\n        return AUDIO_SOURCE_BLUETOOTH;\n    }\n    if (controller->telecoil_enabled && inputs->telecoil_valid) {\n        return AUDIO_SOURCE_TELECOIL;\n    }\n"""
MUTATED = """    if (controller->telecoil_enabled && inputs->telecoil_valid) {\n        return AUDIO_SOURCE_TELECOIL;\n    }\n    if (controller->bluetooth_enabled && inputs->bluetooth_connected) {\n        return AUDIO_SOURCE_BLUETOOTH;\n    }\n"""
M1_ORIGINAL = """        if (controller->bluetooth_enabled && inputs->bluetooth_connected) {\n            enter_active(controller, AUDIO_SOURCE_BLUETOOTH, now_ms);\n            break;\n        }\n        if (controller->telecoil_enabled && inputs->telecoil_valid) {\n            enter_active(controller, AUDIO_SOURCE_TELECOIL, now_ms);\n            break;\n        }\n"""
M1_MUTATED = """        if (controller->telecoil_enabled && inputs->telecoil_valid) {\n            enter_active(controller, AUDIO_SOURCE_TELECOIL, now_ms);\n            break;\n        }\n        if (controller->bluetooth_enabled && inputs->bluetooth_connected) {\n            enter_active(controller, AUDIO_SOURCE_BLUETOOTH, now_ms);\n            break;\n        }\n"""


def main() -> int:
    source_text = SOURCE.read_text(encoding="utf-8")
    if (ORIGINAL not in source_text) or (M1_ORIGINAL not in source_text):
        print("[MUTATION] ERROR: expected priority code patterns not found")
        return 2

    with tempfile.TemporaryDirectory(prefix="dha_mutation_") as tmp:
        work = pathlib.Path(tmp) / "project"
        shutil.copytree(ROOT, work, ignore=shutil.ignore_patterns(
            "build", "build-*", "Debug", "Testing", "SelfTest", "*.gcov", ".git"
        ))
        mutated_source = work / "Application" / "Src" / "connectivity_controller.c"
        mutated_text = source_text.replace(ORIGINAL, MUTATED, 1).replace(M1_ORIGINAL, M1_MUTATED, 1)
        mutated_source.write_text(mutated_text, encoding="utf-8")

        result = subprocess.run(
            ["make", "-f", "Makefile.host", "test"],
            cwd=work,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )

        if result.returncode == 0:
            print(result.stdout)
            print("[MUTATION] FAIL: BT-priority mutation survived the unit suite")
            return 1

        if "Failures" not in result.stdout and "FAIL" not in result.stdout:
            print(result.stdout)
            print("[MUTATION] ERROR: mutated build failed for an unrelated reason")
            return 2

        print("[MUTATION] PASS: reversing Bluetooth > Telecoil priority is detected by tests")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
