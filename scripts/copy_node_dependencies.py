#!/usr/bin/env python3
"""Copy Node.js backend runtime/static dependencies into bin output."""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
NODE_BACKEND_ROOT = (
    REPO_ROOT / "thirdparty" / "puerts" / "unity" / "native" / "papi-nodejs" / ".backends" / "papi-nodejs"
)
BIN_DIR = REPO_ROOT / "bin"
SKIPPED_PLATFORMS = {"web", "android"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Copy platform-specific nodejs dependencies to bin.")
    parser.add_argument("--platform", required=True, choices=["windows", "macos", "linux", "android", "ios", "web"])
    parser.add_argument("--arch", required=True, help="Godot arch (x86_64/x86_32/arm64/arm32/wasm32).")
    return parser.parse_args()


def copy_one(src: Path, dst: Path) -> None:
    if not src.is_file():
        raise FileNotFoundError(f"dependency file not found: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    print(f"[copy_node_deps] copied {src} -> {dst}")


def copy_glob(src_dir: Path, pattern: str, dst_dir: Path) -> int:
    if not src_dir.is_dir():
        raise FileNotFoundError(f"dependency directory not found: {src_dir}")
    copied = 0
    for src in sorted(src_dir.glob(pattern)):
        if not src.is_file():
            continue
        dst = dst_dir / src.name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        copied += 1
        print(f"[copy_node_deps] copied {src} -> {dst}")
    return copied


def build_copy_plan(platform: str, arch: str) -> tuple[str, Path, Path] | None:
    if platform == "windows":
        return ("file", NODE_BACKEND_ROOT / "lib" / "Win64" / "libnode.dll", BIN_DIR / "libnode.dll")
    if platform == "linux":
        return ("file", NODE_BACKEND_ROOT / "lib" / "Linux" / "libnode.so.93", BIN_DIR / "libnode.so.93")
    if platform == "macos":
        source_name = "macOS_arm64" if arch == "arm64" else "macOS"
        return (
            "file",
            NODE_BACKEND_ROOT / "lib" / source_name / "libnode.93.dylib",
            BIN_DIR / "libnode.93.dylib",
        )
    if platform == "ios":
        return ("glob", NODE_BACKEND_ROOT / "lib" / "iOS", BIN_DIR / "ios-nodejs")
    return None


def main() -> int:
    args = parse_args()
    platform = args.platform
    arch = args.arch

    if platform in SKIPPED_PLATFORMS:
        print(f"[copy_node_deps] skip: {platform} does not need copied nodejs dependencies")
        return 0

    plan = build_copy_plan(platform, arch)
    if plan is None:
        print(f"[copy_node_deps] unsupported platform: {platform}", file=sys.stderr)
        return 2

    kind, src, dst = plan
    if kind == "file":
        copy_one(src, dst)
        return 0

    copied = copy_glob(src, "*.a", dst)
    if platform == "ios":
        if copied == 0:
            print("[copy_node_deps] no iOS nodejs static archives found", file=sys.stderr)
            return 2
        return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
