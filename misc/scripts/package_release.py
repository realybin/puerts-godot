#!/usr/bin/env python3
"""Package puerts-godot release archives from extracted CI build artifacts."""

from __future__ import annotations

import argparse
import fnmatch
import shutil
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

try:
    from .build_info import load_build_info
    from .gdextension import LibraryMapping, render_gdextension
    from .manifest import BACKENDS, TARGETS, Backend
except ImportError:
    from build_info import load_build_info
    from gdextension import LibraryMapping, render_gdextension
    from manifest import BACKENDS, TARGETS, Backend

REPO_ROOT = Path(__file__).resolve().parents[2]
DIST_ROOT = "addons/puerts-godot"
DESKTOP_PLATFORMS = ("linux", "macos", "windows")
DOC_FILES = ("README.md", "LICENSE.txt", "COPYRIGHT.txt")
PACKAGE_TARGETS = ("template_debug", "template_release")
SKIP_SUFFIXES = {".exp", ".ilk", ".lib"}
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)


@dataclass(frozen=True)
class Artifact:
    bin_dir: Path
    platform: str
    arch: str
    target: str
    variant: str


def scan_artifacts(root: Path, godot: str) -> list[Artifact]:
    artifacts = []
    for path in sorted(root.iterdir()):
        tokens = path.name.split("-")
        if len(tokens) < 4 or tokens[-1] != f"godot{godot}" or not (path / "bin").is_dir():
            continue
        platform, arch, target, *variant = tokens[:-1]
        if target in PACKAGE_TARGETS:
            artifacts.append(Artifact(path / "bin", platform, arch, target, variant[0] if variant else ""))
    return artifacts


def is_extension_lib(backend: Backend, platform: str, name: str) -> bool:
    stem = name.removeprefix("lib")
    return stem.startswith(f"{backend.source_dir}.{platform}.") and not name.endswith(".pdb")


def matches(backend: Backend, platform: str, arch: str, name: str) -> bool:
    if Path(name).suffix in SKIP_SUFFIXES:
        return False
    if is_extension_lib(backend, platform, name):
        return True
    if name.split(".")[0].removeprefix("lib") == backend.library:
        return True
    patterns = (PurePosixPath(pattern).name for pattern in backend.dependency_patterns(platform, arch))
    return any(fnmatch.fnmatchcase(name, pattern) for pattern in patterns)


def collect(backend: Backend, artifacts: list[Artifact]) -> tuple[dict[str, Path], dict[tuple[str, str, str], str]]:
    files: dict[str, Path] = {}
    libraries: dict[tuple[str, str, str], str] = {}
    for artifact in artifacts:
        if not backend.supports(artifact.platform):
            continue
        for item in artifact.bin_dir.rglob("*"):
            if not item.is_file():
                continue
            relative = item.relative_to(artifact.bin_dir).as_posix()
            if relative != Path(relative).name:
                if Path(relative).parts[0] == f"{artifact.platform}-{backend.name}":
                    files[f"{DIST_ROOT}/bin/{relative}"] = item
                continue
            if not matches(backend, artifact.platform, artifact.arch, relative):
                continue
            if artifact.platform == "web":
                arcname = f"{DIST_ROOT}/bin/web-wasm32/{relative}"
            else:
                feature = TARGETS[artifact.target].feature
                arcname = f"{DIST_ROOT}/bin/{artifact.platform}-{artifact.arch}/{feature}/{relative}"
            files[arcname] = item
            if is_extension_lib(backend, artifact.platform, relative):
                libraries[(artifact.platform, artifact.arch, artifact.target)] = arcname
    return files, libraries


def find_wasm(files: dict[str, Path], source: str, target: str, variant: str) -> str | None:
    suffix = ".nothreads" if variant == "nothreads" else ""
    name = f"{source}.web.{target}.wasm32{suffix}.wasm"
    for arcname in files:
        if arcname.rsplit("/", 1)[-1] == name:
            return arcname
    return None


def native_dependencies(
    backend: Backend, files: dict[str, Path], platform: str, arch: str, target: str
) -> tuple[str, ...]:
    prefix = f"{DIST_ROOT}/bin/{platform}-{arch}/{TARGETS[target].feature}/"
    dependencies = []
    for arcname in files:
        if not arcname.startswith(prefix):
            continue
        name = arcname.rsplit("/", 1)[-1]
        if name.endswith(".pdb") or is_extension_lib(backend, platform, name):
            continue
        dependencies.append(f"res://{arcname}")
    return tuple(sorted(dependencies))


def render_descriptor(
    backend: Backend, files: dict[str, Path], libraries: dict[tuple[str, str, str], str], godot: str, version_label: str
) -> str:
    mappings = []
    for (platform, arch, target), arcname in sorted(libraries.items()):
        if platform == "web":
            continue
        features = [TARGETS[target].feature]
        if platform in DESKTOP_PLATFORMS and target == "template_debug":
            features.insert(0, "editor")
        dependencies = native_dependencies(backend, files, platform, arch, target)
        for feature in features:
            mappings.append(LibraryMapping(f"{platform}.{feature}.{arch}", f"res://{arcname}", dependencies))

    if backend.supports("web"):
        for target in PACKAGE_TARGETS:
            for variant in ("threads", "nothreads"):
                arcname = find_wasm(files, backend.source_dir, target, variant)
                if arcname is None:
                    continue
                key = f"web.{TARGETS[target].feature}.{variant}.wasm32"
                dependencies = ()
                if backend.name != "core":
                    suffix = ".nothreads" if variant == "nothreads" else ""
                    core = f"{DIST_ROOT}/bin/web-wasm32/PuertsCore.web.{target}.wasm32{suffix}.wasm"
                    dependencies = (f"res://{core}",)
                mappings.append(LibraryMapping(key, f"res://{arcname}", dependencies))

    return render_gdextension(backend, godot, mappings, version_label)


def write_zip(path: Path, files: dict[str, Path], texts: dict[str, str]) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w") as archive:
        for arcname in sorted(texts):
            archive.writestr(_info(arcname), texts[arcname].encode("utf-8"))
        for arcname in sorted(files):
            with files[arcname].open("rb") as source, archive.open(_info(arcname), "w") as target:
                shutil.copyfileobj(source, target)
    return path.stat().st_size


def _info(arcname: str) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(arcname, ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o644 << 16
    return info


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Package puerts-godot release archives.")
    parser.add_argument("--artifacts", type=Path, required=True, help="directory containing extracted build artifacts")
    parser.add_argument("--godot", required=True, help="Godot API version the archives were built against, e.g. 4.7")
    parser.add_argument("--output", type=Path, default=Path("dist"), help="output directory (default: dist)")
    parser.add_argument("--backends", default=",".join(BACKENDS), help="comma separated backends to package")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if not args.artifacts.is_dir():
        print(f"[package_release] artifacts directory not found: {args.artifacts}", file=sys.stderr)
        return 2

    artifacts = scan_artifacts(args.artifacts, args.godot)
    if not artifacts:
        print(f"[package_release] no artifacts built against Godot {args.godot}", file=sys.stderr)
        return 2

    backends = [backend.strip() for backend in args.backends.split(",") if backend.strip()]
    unknown = set(backends) - BACKENDS.keys()
    if unknown:
        print(f"[package_release] unknown backends: {', '.join(sorted(unknown))}", file=sys.stderr)
        return 2

    build_info = load_build_info()
    docs = {f"{DIST_ROOT}/{name}": (REPO_ROOT / name).read_text(encoding="utf-8") for name in DOC_FILES}
    args.output.mkdir(parents=True, exist_ok=True)

    for name in backends:
        backend = BACKENDS[name]
        files, libraries = collect(backend, artifacts)
        if not files:
            print(f"[package_release] skip {name}: no files", file=sys.stderr)
            continue
        packaged = {platform for platform, _arch, _target in libraries}
        for platform in sorted(backend.platforms - packaged):
            print(f"[package_release] warn: {name} has no libraries for {platform}", file=sys.stderr)
        descriptor = render_descriptor(backend, files, libraries, args.godot, build_info.label)
        texts = {f"{DIST_ROOT}/{backend.extension}": descriptor}
        texts.update(docs)
        path = args.output / f"puerts-godot-v{build_info.version}-godot{args.godot}-{name}.zip"
        size = write_zip(path, files, texts)
        print(f"[package_release] {path} ({len(files)} files, {size / 1024 / 1024:.1f} MiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
