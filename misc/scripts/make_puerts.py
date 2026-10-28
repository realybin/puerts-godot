#!/usr/bin/env python3
"""Build puerts native backends for this repository."""

from __future__ import annotations

import argparse
import filecmp
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

try:
    from .manifest import BACKENDS, MANIFEST, PLATFORMS, Backend, Platform
except ImportError:
    from manifest import BACKENDS, MANIFEST, PLATFORMS, Backend, Platform

REPO_ROOT = Path(__file__).resolve().parents[2]
PUERTS_REPO_DIR = REPO_ROOT / "thirdparty" / "puerts"
UNITY_DIR = PUERTS_REPO_DIR / "unity"
NATIVE_DIR = UNITY_DIR / "native"
BIN_DIR = REPO_ROOT / "bin"
PATCHES_DIR = REPO_ROOT / "thirdparty" / "patches"
PATCH_APPLY_FLAGS = ["--ignore-whitespace", "--ignore-space-change"]


def _same_file(source: Path, target: Path) -> bool:
    filecmp.clear_cache()
    return filecmp.cmp(source, target, shallow=False)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build puerts native backends.")
    parser.add_argument("--platform", required=True, choices=sorted(PLATFORMS), help="Godot platform name.")
    parser.add_argument("--arch", default="", help="Godot arch (x86_64/x86_32/arm64/arm32/wasm32).")
    parser.add_argument("--config", choices=["Release", "Debug"], default="Debug", help="Puerts CMake config.")
    parser.add_argument(
        "--backends", default=",".join(BACKENDS), help="Comma-separated backends: " + ",".join(BACKENDS)
    )
    parser.add_argument(
        "--threads",
        choices=["true", "false"],
        default="true",
        help="Web threading support; match the SCons threads option (default: true).",
    )
    return parser.parse_args()


def resolve_executable(name: str) -> str:
    path = shutil.which(name)
    if path:
        return path
    if os.name == "nt" and "." not in name:
        for ext in (".cmd", ".exe", ".bat"):
            path = shutil.which(name + ext)
            if path:
                return path
    return name


def run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    resolved = list(command)
    resolved[0] = resolve_executable(command[0])
    printable = " ".join(shlex.quote(str(part)) for part in resolved)
    print(f"[make_puerts] ({cwd}) $ {printable}")
    subprocess.run(resolved, cwd=str(cwd), env=env, check=True)


def ensure_patch(name: str) -> None:
    patch_file = PATCHES_DIR / f"{name}.patch"
    if not patch_file.is_file():
        raise FileNotFoundError(f"patch file not found: {patch_file}")

    def applies_cleanly(*extra: str) -> bool:
        result = subprocess.run(
            [resolve_executable("git"), "apply", "--check", *extra, *PATCH_APPLY_FLAGS, str(patch_file.resolve())],
            cwd=PUERTS_REPO_DIR,
            capture_output=True,
            check=False,
        )
        return result.returncode == 0

    if applies_cleanly():
        run(["git", "apply", *PATCH_APPLY_FLAGS, str(patch_file.resolve())], PUERTS_REPO_DIR)
        print(f"[make_puerts] patch {name} applied.")
    elif applies_cleanly("--reverse"):
        print(f"[make_puerts] patch {name} already applied, skip.")
    else:
        raise RuntimeError(f"patch {name} does not apply cleanly; the puerts submodule may have drifted.")


def normalize_backends(raw: str) -> list[str]:
    names = list(dict.fromkeys(token.strip().lower() for token in raw.split(",") if token.strip()))
    unknown = set(names) - BACKENDS.keys()
    if unknown:
        raise ValueError(f"Unsupported backends: {', '.join(sorted(unknown))}")
    return names


def build_backend(platform: Platform, puerts_arch: str, config: str, backend: Backend, threads: bool = True) -> None:
    backend_dir = NATIVE_DIR / backend.native_dir
    if not backend_dir.is_dir():
        raise FileNotFoundError(f"Backend directory not found: {backend_dir}")

    cmd = [
        "node",
        "../../cli",
        "make",
        "--platform",
        platform.puerts,
        "--arch",
        puerts_arch,
        "--config",
        config,
    ]

    env = os.environ.copy()
    if platform.name == "android":
        env.setdefault("ANDROID_NDK", str(Path.home() / "android-ndk-r27d"))
    elif platform.name == "web":
        compiler_flags = "-pthread -fPIC" if threads else "-fPIC"
        for key, flags in (
            ("CFLAGS", compiler_flags),
            ("CXXFLAGS", compiler_flags),
            ("LDFLAGS", "-pthread" if threads else ""),
        ):
            env[key] = f"{env.get(key, '')} {flags}".strip()
        build_dir = f"build_wasm_{puerts_arch}_{backend.native_dir}" + ("_debug" if config != "Release" else "")
        if (backend_dir / build_dir / "CMakeCache.txt").is_file():
            run(
                [
                    "cmake",
                    "-S",
                    ".",
                    "-B",
                    build_dir,
                    f"-DCMAKE_C_FLAGS={env['CFLAGS']}",
                    f"-DCMAKE_CXX_FLAGS={env['CXXFLAGS']}",
                    f"-DCMAKE_EXE_LINKER_FLAGS={env['LDFLAGS']}",
                    f"-DCMAKE_SHARED_LINKER_FLAGS={env['LDFLAGS']}",
                    f"-DCMAKE_MODULE_LINKER_FLAGS={env['LDFLAGS']}",
                ],
                backend_dir,
                env,
            )

    run(cmd, backend_dir, env)


def stage_dependencies(platform: str, arch: str, backend: Backend) -> None:
    patterns = backend.dependency_patterns(platform, arch)
    if not patterns:
        return

    root = NATIVE_DIR / backend.native_dir / backend.dependency_root
    dst_dir = BIN_DIR / f"{platform}-{backend.name}" if platform == "ios" else BIN_DIR
    dst_dir.mkdir(parents=True, exist_ok=True)
    for pattern in patterns:
        sources = sorted(root.glob(pattern))
        if not sources:
            raise FileNotFoundError(f"no {backend.name} dependencies for {platform} matching {root / pattern}")
        for src in sources:
            if not src.is_file():
                raise FileNotFoundError(f"{backend.name} dependency is not a file: {src}")
            destination = dst_dir / src.name
            source_stat = src.stat()
            if destination.is_file():
                destination_stat = destination.stat()
                if (
                    source_stat.st_size == destination_stat.st_size
                    and source_stat.st_mtime_ns == destination_stat.st_mtime_ns
                    and _same_file(src, destination)
                ):
                    continue
            shutil.copy2(src, destination)
            print(f"[make_puerts] copied {src} -> {destination}")


def main() -> int:
    args = parse_args()

    if not UNITY_DIR.is_dir():
        print(f"[make_puerts] unity directory not found: {UNITY_DIR}", file=sys.stderr)
        return 2

    try:
        backends = normalize_backends(args.backends)
        platform = PLATFORMS[args.platform]
        arch = args.arch or platform.default_arch
        puerts_arch = platform.map_arch(arch)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    if not backends:
        print("[make_puerts] no backend selected.", file=sys.stderr)
        return 2

    supported = set(MANIFEST.supported_backends(platform.name))
    skipped = [backend for backend in backends if backend not in supported]
    for backend in skipped:
        print(f"[make_puerts] skip unsupported backend {backend} on {platform.name}")
    selected = MANIFEST.resolve_backends(backend for backend in backends if backend in supported)
    build_list = [backend for backend in selected if backend != "web"]
    if not build_list:
        print("[make_puerts] no backend was selected for this platform.", file=sys.stderr)
        return 2

    for name in MANIFEST.patch_names(platform.name, build_list):
        ensure_patch(name)

    run(["npm", "ci"], UNITY_DIR)

    for name in build_list:
        build_backend(platform, puerts_arch, args.config, BACKENDS[name], args.threads == "true")

    for name in build_list:
        stage_dependencies(platform.name, arch, BACKENDS[name])

    print("[make_puerts] summary")
    print(f"  platform: {platform.name}")
    print(f"  arch: {arch} -> {puerts_arch}")
    print(f"  config: {args.config}")
    if platform.name == "web":
        print(f"  threads: {args.threads}")
    print(f"  built: {', '.join(build_list)}")
    if "web" in selected:
        print("  web: built by SCons from src/PuertsWeb; no native engine dependency")
    if skipped:
        print(f"  skipped: {', '.join(skipped)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
