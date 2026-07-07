"""Puerts backend layout helpers for SConstruct."""

import glob
import json
import os

from .puerts_matrix import map_puerts_arch, map_puerts_platform

GODOT_LIBRARY_KEY_BY_TARGET = {
    "template_debug": "debug",
    "template_release": "release",
}


def resolve_puerts_paths(puerts_root, godot_platform, godot_arch, godot_target, backend_dir, lib_name):
    puerts_platform = map_puerts_platform(godot_platform)
    if not puerts_platform:
        raise ValueError(f"Unsupported Godot platform for puerts: {godot_platform}")

    puerts_arch = map_puerts_arch(godot_platform, godot_arch)
    if not puerts_arch:
        raise ValueError(f"Unsupported arch mapping for puerts: platform={godot_platform} arch={godot_arch}")

    is_debug = godot_target != "template_release"
    puerts_config = "Debug" if is_debug else "Release"
    if puerts_platform == "win" and is_debug:
        puerts_config = "RelWithDebInfo"

    suffix = "_debug" if is_debug else ""
    build_dir_name = f"build_{puerts_platform}_{puerts_arch}_{backend_dir}{suffix}"
    backend_root = os.path.join(puerts_root, backend_dir)

    if puerts_platform in ["win", "osx"]:
        lib_path = os.path.join(backend_root, build_dir_name, puerts_config)
    elif puerts_platform == "ios":
        lib_path = os.path.join(backend_root, build_dir_name, f"{puerts_config}-iphoneos")
    else:
        lib_path = os.path.join(backend_root, build_dir_name)

    runtime = None
    if puerts_platform == "win":
        runtime = os.path.join(lib_path, f"{lib_name}.dll")
    elif puerts_platform in ["linux", "android"]:
        runtime = os.path.join(lib_path, f"lib{lib_name}.so")
    elif puerts_platform == "osx":
        runtime = os.path.join(lib_path, f"{lib_name}.bundle" if puerts_arch == "x64" else f"lib{lib_name}.dylib")

    return lib_path, runtime


def load_backend_config(config_path):
    if not os.path.isfile(config_path):
        return {}
    with open(config_path, "r", encoding="utf-8") as config_file:
        return json.load(config_file)


def collect_ios_dependency_archives(backend_config, puerts_root, backend_dir, puerts_arch):
    backend_entry = backend_config.get(backend_dir, {}).get("config", {})
    copy_libraries = backend_entry.get("copy-libraries", {}).get("ios", {}).get(puerts_arch, [])
    if not copy_libraries:
        return [], []

    backend_assets_root = os.path.join(puerts_root, backend_dir, ".backends", backend_dir)
    archives = []
    missing_patterns = []

    for relative_pattern in copy_libraries:
        normalized = relative_pattern.lstrip("/\\").replace("/", os.sep)
        pattern = os.path.join(backend_assets_root, normalized)
        matched = sorted(glob.glob(pattern))
        if not matched:
            missing_patterns.append(pattern)
        for path in matched:
            if os.path.isfile(path) and path.endswith(".a"):
                archives.append(os.path.normpath(path))

    return archives, missing_patterns


def map_godot_target_to_library_key(godot_target):
    return GODOT_LIBRARY_KEY_BY_TARGET.get(godot_target, "")
