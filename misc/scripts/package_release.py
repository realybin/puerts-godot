#!/usr/bin/env python3
"""Package extracted Build GDExtension artifacts into installable component directories."""

from __future__ import annotations

import argparse
import fnmatch
import re
import shutil
import sys
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
ADDON_ROOT = "addons/puerts-godot"
ARTIFACT_NAME = re.compile(
    r"gdextension-build-godot(?P<godot>\d+\.\d+)-(?P<precision>single|double)-"
    r"(?P<platform>\w+)-(?P<arch>\w+)-(?P<target>template_debug|template_release)"
    r"(?:-(?P<threads>threads|nothreads))?"
)
EXCLUDED_SUFFIXES = {".exp", ".ilk", ".lib", ".pdb"}


@dataclass(frozen=True)
class BuildArtifact:
    directory: Path
    platform: str
    arch: str
    precision: str
    target: str
    godot: str
    threads: str | None

    @property
    def binary_directory(self) -> Path:
        # download-artifact normally extracts bin/** directly into the artifact directory.
        directory = self.directory / "bin"
        return directory if directory.is_dir() else self.directory

    @property
    def install_directory(self) -> str:
        directory = f"{ADDON_ROOT}/bin/{self.platform}-{self.arch}"
        return directory if self.platform == "web" else f"{directory}/{TARGETS[self.target].feature}"


def discover_artifacts(root: Path) -> list[BuildArtifact]:
    artifacts = []
    for directory in sorted(root.iterdir()):
        match = ARTIFACT_NAME.fullmatch(directory.name)
        if directory.is_dir() and match:
            artifact = BuildArtifact(directory, **match.groupdict())
            if (artifact.platform == "web") != (artifact.threads is not None):
                raise ValueError(f"Invalid Web thread variant: {directory.name}")
            artifacts.append(artifact)
    if not artifacts:
        raise ValueError(f"No build artifacts found in {root}")
    return artifacts


def is_extension_library(backend: Backend, platform: str, name: str) -> bool:
    return name.removeprefix("lib").startswith(f"{backend.source_dir}.{platform}.")


def belongs_to_backend(backend: Backend, artifact: BuildArtifact, name: str) -> bool:
    return (
        is_extension_library(backend, artifact.platform, name)
        or name.split(".")[0].removeprefix("lib") == backend.library
        or any(
            fnmatch.fnmatchcase(name, PurePosixPath(pattern).name)
            for pattern in backend.dependency_patterns(artifact.platform, artifact.arch)
        )
    )


def collect_files(backend: Backend, artifacts: list[BuildArtifact]) -> tuple[dict[str, Path], dict[BuildArtifact, str]]:
    files: dict[str, Path] = {}
    libraries: dict[BuildArtifact, str] = {}
    for artifact in artifacts:
        if not backend.supports(artifact.platform):
            continue
        for source in sorted(artifact.binary_directory.rglob("*")):
            if not source.is_file() or source.suffix in EXCLUDED_SUFFIXES:
                continue
            relative = source.relative_to(artifact.binary_directory)
            if len(relative.parts) > 1:
                if relative.parts[0] != f"{artifact.platform}-{backend.name}":
                    continue
                destination = f"{ADDON_ROOT}/bin/{relative.as_posix()}"
            else:
                if not belongs_to_backend(backend, artifact, source.name):
                    continue
                destination = f"{artifact.install_directory}/{source.name}"
                if is_extension_library(backend, artifact.platform, source.name):
                    if artifact in libraries:
                        raise ValueError(f"Multiple {backend.name} extension libraries in {artifact.directory}")
                    libraries[artifact] = destination
            if destination in files and files[destination].read_bytes() != source.read_bytes():
                raise ValueError(f"Conflicting package file: {destination}")
            files[destination] = source
        if artifact not in libraries:
            raise ValueError(f"Missing {backend.name} extension library in {artifact.directory}")
    return files, libraries


def render_descriptor(
    backend: Backend,
    files: dict[str, Path],
    libraries: dict[BuildArtifact, str],
    required_libraries: dict[str, dict[BuildArtifact, str]],
    version_label: str,
) -> str:
    mappings = []
    for artifact, library in libraries.items():
        dependencies = tuple(
            sorted(
                f"res://{name}"
                for name in files
                if name.startswith(f"{artifact.install_directory}/")
                and not is_extension_library(backend, artifact.platform, PurePosixPath(name).name)
            )
        )
        features = [TARGETS[artifact.target].feature]
        if artifact.platform in ("windows", "linux", "macos") and artifact.target == "template_debug":
            features.insert(0, "editor")
        if artifact.platform == "web":
            dependencies += tuple(f"res://{required_libraries[name][artifact]}" for name in backend.requires)
        for feature in features:
            tags = [artifact.platform, feature]
            if artifact.precision == "double":
                tags.append("double")
            # Godot exposes the threads feature only for threaded Web exports.
            if artifact.threads == "threads":
                tags.append("threads")
            tags.append(artifact.arch)
            mappings.append(LibraryMapping(".".join(tags), f"res://{library}", dependencies))
    return render_gdextension(backend, next(iter(libraries)).godot, mappings, version_label)


def documentation_files() -> dict[str, Path]:
    paths = [REPO_ROOT / name for name in ("README.md", "README.zh-CN.md", "LICENSE.txt", "COPYRIGHT.txt")]
    paths.extend(path for path in (REPO_ROOT / "docs").rglob("*") if path.is_file())
    return {f"{ADDON_ROOT}/{path.relative_to(REPO_ROOT).as_posix()}": path for path in paths}


def write_package(directory: Path, files: dict[str, Path], descriptor_name: str, descriptor: str) -> None:
    directory.mkdir(parents=True, exist_ok=False)
    for name, source in sorted(files.items()):
        destination = directory / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
    (directory / descriptor_name).write_text(descriptor, encoding="utf-8")


def package_artifacts(artifacts: list[BuildArtifact], output: Path, component: str | None = None) -> None:
    build_info = load_build_info()
    output.mkdir(parents=True, exist_ok=True)
    groups = sorted({(artifact.godot, artifact.precision) for artifact in artifacts})
    for godot, precision in groups:
        builds = [artifact for artifact in artifacts if (artifact.godot, artifact.precision) == (godot, precision)]
        components = {name: collect_files(backend, builds) for name, backend in BACKENDS.items()}
        required_libraries = {name: libraries for name, (_, libraries) in components.items()}
        for name, (files, libraries) in components.items():
            if not libraries or component is not None and name != component:
                continue
            backend = BACKENDS[name]
            for requirement in backend.requires:
                for artifact in libraries:
                    if artifact not in required_libraries[requirement]:
                        raise ValueError(f"Missing {requirement} required by {name} in {artifact.directory}")
            descriptor = render_descriptor(backend, files, libraries, required_libraries, build_info.label)
            if name == "core":
                files.update(documentation_files())
            path = output / f"puerts-godot-{name}-v{build_info.version}-godot{godot}-{precision}"
            write_package(path, files, f"{ADDON_ROOT}/{backend.extension}", descriptor)
            print(f"[package_release] {path} ({len(files) + 1} files)")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", type=Path, required=True, help="directory containing extracted CI artifacts")
    parser.add_argument("--output", type=Path, default=Path("dist"), help="output directory (default: dist)")
    parser.add_argument("--component", choices=BACKENDS, help="stage only this component (default: all)")
    args = parser.parse_args(argv)
    try:
        package_artifacts(discover_artifacts(args.artifacts), args.output, args.component)
    except (OSError, ValueError) as error:
        print(f"[package_release] {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
