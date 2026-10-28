from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass

try:
    from .manifest import Backend
except ImportError:
    from manifest import Backend


@dataclass(frozen=True, order=True)
class LibraryMapping:
    key: str
    path: str
    dependencies: tuple[str, ...] = ()


def render_gdextension(
    backend: Backend, compatibility: str, libraries: Iterable[LibraryMapping], version_label: str
) -> str:
    mappings = sorted(libraries)
    lines = [
        f"; {version_label}",
        "",
        "[configuration]",
        "",
        f'entry_symbol = "{backend.entry}"',
        f'compatibility_minimum = "{compatibility}"',
        f"reloadable = {str(backend.reloadable).lower()}",
        "",
        "[libraries]",
        "",
        *(f'{mapping.key} = "{mapping.path}"' for mapping in mappings),
    ]
    dependencies = []
    for mapping in mappings:
        if mapping.dependencies:
            entries = ", ".join(f'"{path}": ""' for path in mapping.dependencies)
            dependencies.append(f"{mapping.key} = {{ {entries} }}")
    if dependencies:
        lines += ["", "[dependencies]", "", *dependencies]
    return "\n".join(lines) + "\n"
