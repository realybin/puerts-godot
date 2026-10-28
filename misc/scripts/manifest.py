from __future__ import annotations

import tomllib
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Platform:
    name: str
    puerts: str
    default_arch: str
    architectures: dict[str, str]

    def map_arch(self, arch: str) -> str:
        try:
            return self.architectures[arch]
        except KeyError:
            raise ValueError(f"Unsupported arch for platform {self.name}: {arch}") from None


@dataclass(frozen=True)
class Target:
    name: str
    feature: str
    cmake: str


@dataclass(frozen=True)
class Dependency:
    platform: str
    arch: str
    patterns: tuple[str, ...]


@dataclass(frozen=True)
class Backend:
    name: str
    native_dir: str
    source_dir: str
    library: str
    extension: str
    entry: str
    platforms: frozenset[str]
    requires: tuple[str, ...]
    reloadable: bool
    dependency_root: str
    dependencies: tuple[Dependency, ...]

    def supports(self, platform: str) -> bool:
        return platform in self.platforms

    def dependency_patterns(self, platform: str, arch: str) -> tuple[str, ...]:
        return tuple(
            pattern
            for dependency in self.dependencies
            if dependency.platform == platform and dependency.arch in ("", arch)
            for pattern in dependency.patterns
        )


@dataclass(frozen=True)
class Patch:
    file: str
    platform: str
    backends: frozenset[str]


@dataclass(frozen=True)
class RuntimeTests:
    platforms: frozenset[str]
    backends: tuple[str, ...]
    aliases: dict[str, str]


@dataclass(frozen=True)
class Manifest:
    platforms: dict[str, Platform]
    targets: dict[str, Target]
    backends: dict[str, Backend]
    patches: tuple[Patch, ...]
    runtime_tests: RuntimeTests

    def supported_backends(self, platform: str) -> tuple[str, ...]:
        return tuple(name for name, backend in self.backends.items() if backend.supports(platform))

    def resolve_backends(self, names: Iterable[str]) -> tuple[str, ...]:
        requested = set(names)
        unknown = requested - self.backends.keys()
        if unknown:
            raise ValueError(f"Unknown backends: {', '.join(sorted(unknown))}")

        resolved: set[str] = set()

        def add(name: str) -> None:
            if name in resolved:
                return
            resolved.add(name)
            for requirement in self.backends[name].requires:
                add(requirement)

        for name in requested:
            add(name)
        return tuple(name for name in self.backends if name in resolved)

    def patch_names(self, platform: str, backends: Iterable[str]) -> tuple[str, ...]:
        selected = set(backends)
        return tuple(
            patch.file for patch in self.patches if patch.platform in ("", platform) and patch.backends & selected
        )

    def validate(self) -> None:
        for platform in self.platforms.values():
            if platform.default_arch not in platform.architectures:
                raise ValueError(f"Invalid default arch for platform {platform.name}: {platform.default_arch}")

        for backend in self.backends.values():
            self._check_platforms(backend.platforms, f"backend {backend.name}")
            unknown = set(backend.requires) - self.backends.keys()
            if unknown:
                raise ValueError(f"Unknown requirements for backend {backend.name}: {', '.join(sorted(unknown))}")
            for dependency in backend.dependencies:
                self._check_platforms({dependency.platform}, f"backend {backend.name} dependency")
                if dependency.arch and dependency.arch not in self.platforms[dependency.platform].architectures:
                    raise ValueError(
                        f"Invalid arch for backend {backend.name} dependency: {dependency.platform}/{dependency.arch}"
                    )

        for patch in self.patches:
            if patch.platform:
                self._check_platforms({patch.platform}, f"patch {patch.file}")
            unknown = patch.backends - self.backends.keys()
            if unknown:
                raise ValueError(f"Unknown backends for patch {patch.file}: {', '.join(sorted(unknown))}")

        self._check_platforms(self.runtime_tests.platforms, "runtime tests")
        unknown = set(self.runtime_tests.backends) - self.backends.keys()
        unknown |= set(self.runtime_tests.aliases) - self.backends.keys()
        unknown |= set(self.runtime_tests.aliases.values()) - self.backends.keys()
        if unknown:
            raise ValueError(f"Unknown runtime test backends: {', '.join(sorted(unknown))}")

    def _check_platforms(self, names: Iterable[str], owner: str) -> None:
        unknown = set(names) - self.platforms.keys()
        if unknown:
            raise ValueError(f"Unknown platforms for {owner}: {', '.join(sorted(unknown))}")


def load_manifest() -> Manifest:
    repo_root = Path(__file__).resolve().parents[2]
    with (repo_root / "manifest.toml").open("rb") as source:
        data = tomllib.load(source)
    with (repo_root / "thirdparty" / "patches.toml").open("rb") as source:
        patch_data = tomllib.load(source)

    platforms = {
        name: Platform(name, entry["puerts"], entry["default_arch"], entry["arch"])
        for name, entry in data["platforms"].items()
    }
    targets = {name: Target(name, entry["feature"], entry["cmake"]) for name, entry in data["targets"].items()}
    backends = {
        name: Backend(
            name=name,
            native_dir=entry["native_dir"],
            source_dir=entry["source_dir"],
            library=entry["library"],
            extension=entry["extension"],
            entry=entry["entry"],
            platforms=frozenset(entry.get("platforms", platforms)),
            requires=tuple(entry.get("requires", ())),
            reloadable=entry.get("reloadable", True),
            dependency_root=entry.get("dependency_root", ""),
            dependencies=tuple(
                Dependency(item["platform"], item.get("arch", ""), tuple(item["patterns"]))
                for item in entry.get("dependencies", ())
            ),
        )
        for name, entry in data["backends"].items()
    }
    patches = tuple(
        Patch(entry["file"], entry.get("platform", ""), frozenset(entry["backends"]))
        for entry in patch_data.get("patches", ())
    )
    runtime = data["tests"]["runtime"]
    manifest = Manifest(
        platforms=platforms,
        targets=targets,
        backends=backends,
        patches=patches,
        runtime_tests=RuntimeTests(frozenset(runtime["platforms"]), tuple(runtime["backends"]), runtime["aliases"]),
    )
    manifest.validate()
    return manifest


MANIFEST = load_manifest()
PLATFORMS = MANIFEST.platforms
TARGETS = MANIFEST.targets
BACKENDS = MANIFEST.backends
