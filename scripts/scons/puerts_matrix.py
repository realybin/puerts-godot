"""Shared build and test configuration."""

import json
from pathlib import Path

CONFIG = json.loads((Path(__file__).resolve().parents[1] / "config.json").read_text(encoding="utf-8"))
PLATFORMS = CONFIG["platforms"]
BACKENDS = CONFIG["backends"]


def map_puerts_platform(godot_platform):
    return PLATFORMS.get(godot_platform, {}).get("puerts")


def map_puerts_arch(godot_platform, godot_arch):
    return PLATFORMS.get(godot_platform, {}).get("arch", {}).get(godot_arch)


def supported_backends(godot_platform):
    if godot_platform not in PLATFORMS:
        return set()
    return {backend["native"] for backend in BACKENDS.values() if godot_platform in backend.get("platforms", PLATFORMS)}


def supports_runtime_tests(godot_platform):
    return godot_platform in CONFIG["runtime_test_platforms"]
