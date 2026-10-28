import importlib.util
import subprocess
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class BuildInfo:
    version: str
    label: str


def load_build_info() -> BuildInfo:
    spec = importlib.util.spec_from_file_location("puerts_godot_version", REPO_ROOT / "version.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    version = f"{module.major}.{module.minor}.{module.patch}"
    commit = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()
    return BuildInfo(version, f"v{version}.{module.status} [{commit}]")
