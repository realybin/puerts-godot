import glob
import json
import os

from ..manifest import Backend, Platform, Target


def resolve_puerts_paths(
    puerts_root: str, platform: Platform, arch: str, target: Target, backend: Backend
) -> tuple[str, str | None]:
    puerts_arch = platform.map_arch(arch)
    is_debug = target.cmake != "Release"
    puerts_config = target.cmake
    if platform.name == "windows" and is_debug:
        puerts_config = "RelWithDebInfo"

    suffix = "_debug" if is_debug else ""
    build_dir_name = f"build_{platform.puerts}_{puerts_arch}_{backend.native_dir}{suffix}"
    backend_root = os.path.join(puerts_root, backend.native_dir)

    if platform.puerts in ("win", "osx"):
        lib_path = os.path.join(backend_root, build_dir_name, puerts_config)
    elif platform.puerts == "ios":
        lib_path = os.path.join(backend_root, build_dir_name, f"{puerts_config}-iphoneos")
    else:
        lib_path = os.path.join(backend_root, build_dir_name)

    runtime = None
    if platform.puerts == "win":
        runtime = os.path.join(lib_path, f"{backend.library}.dll")
    elif platform.puerts in ("linux", "android"):
        runtime = os.path.join(lib_path, f"lib{backend.library}.so")
    elif platform.puerts == "osx":
        filename = f"{backend.library}.bundle" if puerts_arch == "x64" else f"lib{backend.library}.dylib"
        runtime = os.path.join(lib_path, filename)

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
    seen_archives = set()
    missing_patterns = []

    for relative_pattern in copy_libraries:
        normalized = relative_pattern.lstrip("/\\").replace("/", os.sep)
        pattern = os.path.join(backend_assets_root, normalized)
        matched = sorted(glob.glob(pattern))
        if not matched:
            missing_patterns.append(pattern)
        for path in matched:
            normalized_path = os.path.normpath(path)
            if os.path.isfile(path) and path.endswith(".a") and normalized_path not in seen_archives:
                archives.append(normalized_path)
                seen_archives.add(normalized_path)

    return archives, missing_patterns
