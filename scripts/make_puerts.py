#!/usr/bin/env python3
"""Build puerts native backends for this repository."""

from __future__ import annotations

import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

from scons.puerts_matrix import ARCH_TO_PUERTS, PLATFORM_TO_PUERTS, SUPPORTED_BACKENDS

REPO_ROOT = Path(__file__).resolve().parents[1]
PUERTS_REPO_DIR = REPO_ROOT / "thirdparty" / "puerts"
UNITY_DIR = REPO_ROOT / "thirdparty" / "puerts" / "unity"
NATIVE_DIR = UNITY_DIR / "native"
PAPI_LUA_OBJECT_NEW_FILE = PUERTS_REPO_DIR / "unity" / "native" / "papi-lua" / "source" / "CppObjectMapperLua.cpp"
PAPI_LUA_OBJECT_NEW_PATCH = REPO_ROOT / "patches" / "papi-lua-object-new-nullptr.patch"
PAPI_V8_NODEJS_LINUX_RPATH_PATCH = REPO_ROOT / "patches" / "papi-v8-nodejs-linux-origin-rpath.patch"

BACKEND_ALIASES = {
    "core": "puerts",
    "puerts": "puerts",
    "v8": "papi-v8",
    "papi-v8": "papi-v8",
    "nodejs": "papi-nodejs",
    "papi-nodejs": "papi-nodejs",
    "quickjs": "papi-quickjs",
    "papi-quickjs": "papi-quickjs",
    "lua": "papi-lua",
    "papi-lua": "papi-lua",
}

DEFAULT_BACKENDS = ["core", "v8", "nodejs", "quickjs", "lua"]

DEFAULT_ARCH = {
    "windows": "x86_64",
    "macos": "arm64",
    "linux": "x86_64",
    "android": "arm64",
    "ios": "arm64",
    "web": "wasm32",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build puerts native backends.")
    parser.add_argument(
        "--platform",
        required=True,
        choices=sorted(PLATFORM_TO_PUERTS.keys()),
        help="Godot platform name.",
    )
    parser.add_argument("--arch", default="", help="Godot arch (x86_64/x86_32/arm64/arm32/wasm32).")
    parser.add_argument(
        "--config",
        choices=["Release", "Debug"],
        default="Debug",
        help="Puerts CMake config.",
    )
    parser.add_argument(
        "--backends",
        default=",".join(DEFAULT_BACKENDS),
        help="Comma-separated backends: core,v8,nodejs,quickjs,lua",
    )
    parser.add_argument("--websocket", default="0", help="Forwarded websocket option for puerts CLI.")
    parser.add_argument("--rebuild", action="store_true", help="Clean and rebuild in puerts CLI.")
    parser.add_argument("--skip-npm-install", action="store_true", help="Skip npm ci in unity directory.")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Fail when a requested backend is unsupported on the target platform.",
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


def append_env_flags(env: dict[str, str], key: str, flags: str) -> None:
    current = env.get(key, "").strip()
    env[key] = f"{current} {flags}".strip() if current else flags


def ensure_papi_lua_constructor_patch() -> None:
    marker = "Null constructor results can happen under low-memory conditions."
    if not PAPI_LUA_OBJECT_NEW_FILE.is_file():
        raise FileNotFoundError(f"Papi Lua source not found: {PAPI_LUA_OBJECT_NEW_FILE}")

    def has_patch(content: str) -> bool:
        return marker in content or (
            "void* ptr = class_definition->Initialize(" in content
            and "if (ptr == nullptr)" in content
            and "BindCppObject(L, class_definition, ptr, false);" in content
        )

    source_text = PAPI_LUA_OBJECT_NEW_FILE.read_text(encoding="utf-8", errors="replace")
    if has_patch(source_text):
        print("[make_puerts] papi-lua constructor patch already present, skip.")
        return
    if not PAPI_LUA_OBJECT_NEW_PATCH.is_file():
        raise FileNotFoundError(f"papi-lua constructor patch not found: {PAPI_LUA_OBJECT_NEW_PATCH}")
    run(
        ["git", "apply", "--ignore-whitespace", "--ignore-space-change", str(PAPI_LUA_OBJECT_NEW_PATCH.resolve())],
        PUERTS_REPO_DIR,
    )
    print("[make_puerts] papi-lua constructor patch applied.")


def ensure_papi_v8_nodejs_linux_rpath_patch() -> None:
    markers = (
        'BUILD_RPATH "\\$ORIGIN"',
        'INSTALL_RPATH "\\$ORIGIN"',
    )
    targets = [
        PUERTS_REPO_DIR / "unity" / "native" / "papi-v8" / "CMakeLists.txt",
        PUERTS_REPO_DIR / "unity" / "native" / "papi-nodejs" / "CMakeLists.txt",
    ]
    for target in targets:
        if not target.is_file():
            raise FileNotFoundError(f"Papi CMake file not found: {target}")

    def has_patch(content: str) -> bool:
        return all(marker in content for marker in markers)

    if all(has_patch(target.read_text(encoding="utf-8", errors="replace")) for target in targets):
        print("[make_puerts] papi-v8/nodejs linux rpath patch already present, skip.")
        return
    if not PAPI_V8_NODEJS_LINUX_RPATH_PATCH.is_file():
        raise FileNotFoundError(f"papi-v8/nodejs linux rpath patch not found: {PAPI_V8_NODEJS_LINUX_RPATH_PATCH}")
    run(
        [
            "git",
            "apply",
            "--ignore-whitespace",
            "--ignore-space-change",
            str(PAPI_V8_NODEJS_LINUX_RPATH_PATCH.resolve()),
        ],
        PUERTS_REPO_DIR,
    )
    print("[make_puerts] papi-v8/nodejs linux rpath patch applied.")


def normalize_backends(raw: str) -> list[str]:
    result: list[str] = []
    for token in raw.split(","):
        name = token.strip().lower()
        if not name:
            continue
        if name not in BACKEND_ALIASES:
            raise ValueError(f"Unsupported backend token: {token}")
        canonical = BACKEND_ALIASES[name]
        if canonical not in result:
            result.append(canonical)
    return result


def build_backend(platform: str, puerts_arch: str, config: str, backend: str, websocket: str, rebuild: bool) -> None:
    backend_dir = NATIVE_DIR / backend
    if not backend_dir.is_dir():
        raise FileNotFoundError(f"Backend directory not found: {backend_dir}")

    cmd = [
        "node",
        "../../cli",
        "make",
        "--platform",
        PLATFORM_TO_PUERTS[platform],
        "--arch",
        puerts_arch,
        "--config",
        config,
        "--websocket",
        websocket,
    ]
    if rebuild:
        cmd.append("--rebuild")

    env = os.environ.copy()
    if platform == "android":
        env.setdefault("ANDROID_NDK", str(Path.home() / "android-ndk-r27d"))
    elif platform == "web":
        append_env_flags(env, "CFLAGS", "-pthread -fPIC")
        append_env_flags(env, "CXXFLAGS", "-pthread -fPIC")
        append_env_flags(env, "LDFLAGS", "-pthread")

    run(cmd, backend_dir, env)


def split_requested_backends(platform: str, requested: list[str], strict: bool) -> tuple[list[str], list[str]]:
    supported = SUPPORTED_BACKENDS[platform]
    build_list: list[str] = []
    skipped: list[str] = []
    for backend in requested:
        if backend in supported:
            build_list.append(backend)
            continue
        message = f"[make_puerts] skip unsupported backend {backend} on {platform}"
        if strict:
            raise ValueError(message)
        print(message)
        skipped.append(backend)
    return build_list, skipped


def main() -> int:
    args = parse_args()

    if not UNITY_DIR.is_dir():
        print(f"[make_puerts] unity directory not found: {UNITY_DIR}", file=sys.stderr)
        return 2

    backends = normalize_backends(args.backends)
    if not backends:
        print("[make_puerts] no backend selected.", file=sys.stderr)
        return 2

    arch = args.arch or DEFAULT_ARCH[args.platform]
    puerts_arch = ARCH_TO_PUERTS.get(args.platform, {}).get(arch)
    if not puerts_arch:
        print(f"[make_puerts] unsupported arch mapping for platform={args.platform}, arch={arch}", file=sys.stderr)
        return 2

    if args.platform == "linux" and any(backend in {"papi-v8", "papi-nodejs"} for backend in backends):
        ensure_papi_v8_nodejs_linux_rpath_patch()
    if "papi-lua" in backends:
        ensure_papi_lua_constructor_patch()

    if not args.skip_npm_install:
        run(["npm", "ci"], UNITY_DIR)

    try:
        build_list, skipped = split_requested_backends(args.platform, backends, args.strict)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    built: list[str] = []
    for backend in build_list:
        build_backend(
            platform=args.platform,
            puerts_arch=puerts_arch,
            config=args.config,
            backend=backend,
            websocket=args.websocket,
            rebuild=args.rebuild,
        )
        built.append(backend)

    if not built:
        print("[make_puerts] no backend was built.", file=sys.stderr)
        return 2

    print("[make_puerts] summary")
    print(f"  platform: {args.platform}")
    print(f"  arch: {arch} -> {puerts_arch}")
    print(f"  config: {args.config}")
    print(f"  built: {', '.join(built)}")
    if skipped:
        print(f"  skipped: {', '.join(skipped)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
