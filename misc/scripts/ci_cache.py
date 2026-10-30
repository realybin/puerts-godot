#!/usr/bin/env python3
"""Describe the reusable native outputs and SCons baseline for a CI build."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

from .make_puerts import normalize_backends
from .manifest import BACKENDS, MANIFEST, PLATFORMS, TARGETS
from .scons.puerts_layout import resolve_puerts_paths

ROOT = Path(__file__).resolve().parents[2]
NATIVE = Path("thirdparty/puerts/unity/native")


def command_output(*command: str) -> str:
    executable = shutil.which(command[0]) or command[0]
    return subprocess.check_output([executable, *command[1:]], cwd=ROOT, text=True, stderr=subprocess.STDOUT).strip()


def fingerprint(values: list[str], paths: list[Path]) -> str:
    digest = hashlib.sha256()
    for value in values:
        digest.update(value.encode())
        digest.update(b"\0")
    for path in sorted(set(paths)):
        digest.update(path.relative_to(ROOT).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()[:24]


def toolchain_versions(platform: str) -> list[str]:
    versions = [os.environ.get("ImageOS", os.environ.get("RUNNER_OS", "local"))]
    for tool in ("cmake", "gcc", "clang", "emcc", "scons"):
        if shutil.which(tool):
            versions.append(command_output(tool, "--version"))
    if os.name == "nt":
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        installations = json.loads(command_output(str(vswhere), "-format", "json"))
        versions.extend(sorted(item["installationVersion"] for item in installations))
    if platform == "android":
        ndk = Path(os.environ["ANDROID_NDK_HOME"])
        versions.append((ndk / "source.properties").read_text())
    if platform in ("macos", "ios"):
        versions.append(command_output("xcodebuild", "-version"))
    return versions


def native_cache_paths(platform_name: str, arch: str, target: str, backends: tuple[str, ...]) -> list[str]:
    platform = PLATFORMS[platform_name]
    config = json.loads((ROOT / "thirdparty/puerts/unity/cli/backends.json").read_text())
    paths = []
    for name in backends:
        if name == "web":
            continue
        backend = BACKENDS[name]
        library_dir, _ = resolve_puerts_paths(NATIVE.as_posix(), platform, arch, TARGETS[target], backend)
        library_dir = Path(library_dir).as_posix()
        # Preserve only final libraries, never CMake state, object files or UPM copies.
        paths.extend(
            f"{library_dir}/{pattern}" for pattern in ("*.a", "*.lib", "*.dll", "*.so*", "*.dylib", "*.bundle")
        )
        paths.extend(
            (NATIVE / backend.native_dir / backend.dependency_root / pattern).as_posix()
            for pattern in backend.dependency_patterns(platform_name, arch)
        )
        if platform_name == "ios":
            patterns = (
                config[backend.native_dir]["config"]
                .get("copy-libraries", {})
                .get("ios", {})
                .get(platform.map_arch(arch), [])
            )
            paths.extend(
                (NATIVE / backend.native_dir / ".backends" / backend.native_dir / pattern.lstrip("/")).as_posix()
                for pattern in patterns
            )
    return sorted(set(paths))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=PLATFORMS)
    parser.add_argument("--arch", required=True)
    parser.add_argument("--target", required=True, choices=TARGETS)
    parser.add_argument("--precision", required=True, choices=("single", "double"))
    parser.add_argument("--godot", required=True)
    parser.add_argument("--scope", required=True, choices=("ci", "build"))
    parser.add_argument("--backends", default="")
    parser.add_argument("--threads", default="true", choices=("true", "false"))
    args = parser.parse_args()
    requested = normalize_backends(args.backends) if args.backends else BACKENDS
    backends = MANIFEST.resolve_backends(name for name in requested if BACKENDS[name].supports(args.platform))
    sources = [
        ROOT / "manifest.toml",
        ROOT / "thirdparty/patches.toml",
        ROOT / "misc/scripts/make_puerts.py",
        ROOT / "misc/scripts/scons/puerts_layout.py",
    ]
    sources.extend((ROOT / "thirdparty/patches").glob("*.patch"))
    versions = toolchain_versions(args.platform)
    native_config = [args.platform, args.arch, TARGETS[args.target].cmake, *backends]
    if args.platform == "web":
        native_config.append(args.threads)
    native_hash = fingerprint(
        [command_output("git", "-C", "thirdparty/puerts", "rev-parse", "HEAD"), *versions, *native_config], sources
    )
    native_key = f"puerts-native-{args.platform}-{args.arch}-{TARGETS[args.target].cmake.lower()}-{native_hash}"
    scons_sources = [ROOT / "SConstruct", *(ROOT / "misc/scripts/scons").glob("*.py")]
    scons_hash = fingerprint(
        [
            native_hash,
            args.target,
            args.precision,
            args.godot,
            *(
                command_output("git", "rev-parse", f"HEAD:{name}")
                for name in ("godot-cpp", "thirdparty/EASTL", "thirdparty/EABase")
            ),
        ],
        scons_sources,
    )
    scons_key = (
        f"gdextension-{args.scope}-{args.platform}-{args.arch}-{args.target}-"
        f"godot{args.godot}-{args.precision}-{scons_hash}"
    )
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        output.write(f"native-key={native_key}\nscons-key={scons_key}\n")
        output.write(
            "native-paths<<CACHE_PATHS\n"
            + "\n".join(native_cache_paths(args.platform, args.arch, args.target, backends))
            + "\nCACHE_PATHS\n"
        )


if __name__ == "__main__":
    main()
