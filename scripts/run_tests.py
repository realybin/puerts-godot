#!/usr/bin/env python3
"""
Run runtime tests for the puerts-godot project.
"""

from __future__ import annotations

import argparse
import contextlib
import os
import platform
import queue
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

from scons.puerts_layout import map_godot_target_to_library_key
from scons.puerts_matrix import supports_runtime_tests

DEFAULT_TIMEOUT = 300
IMPORT_TIMEOUT_RATIO = 0.5
RUNTIME_SUFFIXES = {".dll", ".so", ".dylib"}
GDEXTENSION_BACKENDS = [
    {
        "name": "core",
        "file": "puerts_core.gdextension",
        "entry_symbol": "puerts_core_library_init",
        "binary_stem": "PuertsCore",
    },
    {
        "name": "quickjs",
        "file": "puerts_quickjs.gdextension",
        "entry_symbol": "puerts_quickjs_library_init",
        "binary_stem": "PuertsQuickjs",
    },
    {
        "name": "v8",
        "file": "puerts_v8.gdextension",
        "entry_symbol": "puerts_v8_library_init",
        "binary_stem": "PuertsV8",
    },
    {
        "name": "nodejs",
        "file": "puerts_nodejs.gdextension",
        "entry_symbol": "puerts_nodejs_library_init",
        "binary_stem": "PuertsNodejs",
    },
    {
        "name": "lua",
        "file": "puerts_lua.gdextension",
        "entry_symbol": "puerts_lua_library_init",
        "binary_stem": "PuertsLua",
    },
]
RUNTIME_BACKEND_NAMES = {backend["name"] for backend in GDEXTENSION_BACKENDS if backend["name"] != "core"}
# Keep only one heavy JS backend in runtime tests.
BACKEND_ALIAS = {
    "nodejs": "v8",
}
DEFAULT_BACKENDS = "lua,quickjs,v8"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run runtime tests for puerts-godot.")
    parser.add_argument("--godot", required=True, help="Path to the Godot executable.")
    parser.add_argument(
        "--platform",
        default="",
        choices=["", "windows", "macos", "linux", "android", "ios", "web"],
        help="Target Godot platform in CI. Empty means detect from host OS.",
    )
    parser.add_argument(
        "--project",
        default=str(Path(__file__).resolve().parents[1] / "tests"),
        help="Path to the Godot project directory.",
    )
    parser.add_argument(
        "--backends",
        default=DEFAULT_BACKENDS,
        help=f"Comma-separated backend names to run (e.g. quickjs,lua,v8,nodejs). Default: {DEFAULT_BACKENDS}.",
    )
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help="Maximum seconds to wait.")
    return parser.parse_args()


def detect_platform() -> str:
    system = platform.system().lower()
    if system == "windows":
        return "windows"
    if system == "darwin":
        return "macos"
    if system == "linux":
        return "linux"
    return ""


def sync_binaries(build_bin_dir: Path, project_bin_dir: Path) -> int:
    project_bin_dir.mkdir(parents=True, exist_ok=True)
    for stale_file in project_bin_dir.iterdir():
        if stale_file.is_file():
            stale_file.unlink()
    copied_count = 0
    for src_file in build_bin_dir.iterdir():
        if not src_file.is_file():
            continue
        dst_file = project_bin_dir / src_file.name
        shutil.copy2(src_file, dst_file)
        copied_count += 1
    return copied_count


def _render_gdextension(entry_symbol: str, library_lines: list[str]) -> str:
    rendered = [
        "[configuration]",
        "",
        f'entry_symbol = "{entry_symbol}"',
        'compatibility_minimum = "4.4"',
        "reloadable = true",
        "",
        "[libraries]",
        "",
    ]
    rendered.extend(library_lines)
    rendered.append("")
    return "\n".join(rendered)


def _collect_library_lines(project_bin_dir: Path, backend_stem: str, platform_name: str) -> list[str]:
    target_pattern = re.compile(
        rf"^(?:lib)?{re.escape(backend_stem)}\.{re.escape(platform_name)}\.(template_debug|template_release)\.([^.]+)\.([^.]+)$"
    )
    lines: list[str] = []
    for artifact in sorted(project_bin_dir.iterdir()):
        if not artifact.is_file() or artifact.suffix.lower() not in RUNTIME_SUFFIXES:
            continue
        match = target_pattern.match(artifact.name)
        if not match:
            continue
        target, arch, _suffix = match.groups()
        library_key = map_godot_target_to_library_key(target)
        if not library_key:
            continue
        resource_path = f"res://bin/{artifact.name}"
        lines.append(f'{platform_name}.{library_key}.{arch} = "{resource_path}"')
        # Headless Godot in CI uses editor flavor; map it to current built artifact.
        lines.append(f'{platform_name}.editor.{arch} = "{resource_path}"')
    return lines


def _parse_requested_backends(raw_backends: str) -> tuple[list[str], list[str]]:
    requested: list[str] = []
    invalid: list[str] = []
    for token in raw_backends.split(","):
        normalized = token.strip().lower()
        if not normalized:
            continue
        normalized = BACKEND_ALIAS.get(normalized, normalized)
        if normalized in RUNTIME_BACKEND_NAMES:
            if normalized not in requested:
                requested.append(normalized)
        else:
            invalid.append(normalized)
    return requested, invalid


def _prepend_env_path(env: dict[str, str], key: str, value: str) -> None:
    existing = env.get(key, "")
    if existing:
        env[key] = value + os.pathsep + existing
    else:
        env[key] = value


def configure_runtime_loader_env(platform_name: str, project_bin_dir: Path, env: dict[str, str]) -> None:
    bin_path = str(project_bin_dir)
    if platform_name == "linux":
        _prepend_env_path(env, "LD_LIBRARY_PATH", bin_path)
        print(f"[test-runner] loader path: LD_LIBRARY_PATH prefixed with {bin_path}")
        return
    if platform_name == "windows":
        _prepend_env_path(env, "PATH", bin_path)
        print(f"[test-runner] loader path: PATH prefixed with {bin_path}")
        return
    if platform_name == "macos":
        _prepend_env_path(env, "DYLD_LIBRARY_PATH", bin_path)
        print(f"[test-runner] loader path: DYLD_LIBRARY_PATH prefixed with {bin_path}")


def verify_linux_v8_dependency_linkage(project_bin_dir: Path, requested_backends: set[str], env: dict[str, str]) -> int:
    if "v8" not in requested_backends:
        return 0
    if shutil.which("ldd") is None:
        print("[test-runner] skip linkage check: ldd not found")
        return 0

    papi_v8_path = project_bin_dir / "libPapiV8.so"
    if not papi_v8_path.is_file():
        print(f"[test-runner] skip linkage check: missing {papi_v8_path.name}")
        return 0

    expected_core = (project_bin_dir / "libPuertsCore.so").resolve()
    if not expected_core.is_file():
        print(f"[test-runner] skip linkage check: missing {expected_core.name}")
        return 0

    completed = subprocess.run(
        ["ldd", str(papi_v8_path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if completed.returncode != 0:
        print("[test-runner] failed to run ldd for libPapiV8.so", file=sys.stderr)
        print((completed.stdout or "").strip(), file=sys.stderr)
        return 2

    resolved_core: Path | None = None
    for line in (completed.stdout or "").splitlines():
        match = re.search(r"^\s*libPuertsCore\.so\s*=>\s*(\S+)", line)
        if not match:
            continue
        resolved_text = match.group(1)
        if resolved_text == "not":
            print("[test-runner] libPapiV8.so cannot resolve libPuertsCore.so", file=sys.stderr)
            print(line.strip(), file=sys.stderr)
            return 2
        resolved_core = Path(resolved_text).resolve(strict=False)
        break

    if resolved_core is None:
        print("[test-runner] ldd output missing libPuertsCore.so dependency line", file=sys.stderr)
        return 2
    if resolved_core != expected_core:
        print("[test-runner] libPapiV8.so resolved PuertsCore from unexpected path.", file=sys.stderr)
        print(f"[test-runner] expected: {expected_core}", file=sys.stderr)
        print(f"[test-runner] actual:   {resolved_core}", file=sys.stderr)
        return 2

    print(f"[test-runner] linkage check ok: libPuertsCore.so => {resolved_core}")
    return 0


def rewrite_gdextension_files(project_dir: Path, platform_name: str, requested_backends: set[str]) -> list[str]:
    updated: list[str] = []
    project_bin_dir = project_dir / "bin"
    for backend in GDEXTENSION_BACKENDS:
        backend_name = backend["name"]
        if backend_name != "core" and backend_name not in requested_backends:
            continue
        lines = _collect_library_lines(project_bin_dir, backend["binary_stem"], platform_name)
        if not lines:
            continue
        config_path = project_dir / backend["file"]
        config_path.write_text(_render_gdextension(backend["entry_symbol"], lines), encoding="utf-8", newline="\n")
        updated.append(config_path.name)
    return updated


def import_safe_backends(platform_name: str, requested_backends: set[str]) -> set[str]:
    # Godot headless import can crash while probing V8 GDExtension on Linux.
    # Keep import phase on stable backends, then restore full backend mapping for runtime tests.
    if platform_name == "linux" and "v8" in requested_backends:
        return {backend for backend in requested_backends if backend != "v8"}
    return set(requested_backends)


@contextlib.contextmanager
def temporarily_hide_gdextensions(project_dir: Path, keep_backends: set[str]):
    moved: list[tuple[Path, Path]] = []
    keep_names = {"core"} | set(keep_backends)
    try:
        for backend in GDEXTENSION_BACKENDS:
            backend_name = backend["name"]
            if backend_name in keep_names:
                continue
            path = project_dir / backend["file"]
            if not path.is_file():
                continue
            hidden = path.with_suffix(path.suffix + ".disabled-for-import")
            if hidden.exists():
                hidden.unlink()
            path.rename(hidden)
            moved.append((hidden, path))
        yield
    finally:
        for hidden, original in reversed(moved):
            if hidden.exists():
                hidden.rename(original)


def run_godot_import(root: Path, godot_exe: Path, project_dir: Path, timeout: float, env: dict[str, str]) -> int:
    import_timeout = max(15.0, timeout * IMPORT_TIMEOUT_RATIO)
    command = [
        str(godot_exe),
        "--headless",
        "--path",
        str(project_dir),
        "--import",
    ]
    print(f"[test-runner] importing project assets: {' '.join(command)}")
    try:
        completed = subprocess.run(
            command,
            cwd=root,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=import_timeout,
            check=False,
        )
    except subprocess.TimeoutExpired:
        print(f"[test-runner] import timeout after {import_timeout:.1f}s", file=sys.stderr)
        return 1

    output = (completed.stdout or "").strip()
    if output:
        print("[test-runner] import output:")
        print(output)
    print(f"[test-runner] import exit code: {completed.returncode}")
    return completed.returncode


def should_skip_godot_import(platform_name: str) -> bool:
    if os.environ.get("GITHUB_ACTIONS") != "true":
        return False
    # CI runners can crash during headless import while loading editor layout.
    return platform_name in {"linux", "windows", "macos"}


SUMMARY_PATTERN = re.compile(r"^\[mini-test\] summary .*failed=(\d+)\b")


def _terminate_process(proc: subprocess.Popen[str]) -> None:
    try:
        proc.terminate()
        proc.wait(timeout=3)
        return
    except Exception:
        pass
    try:
        proc.kill()
        proc.wait(timeout=3)
    except Exception:
        pass


def run_with_timeout(command: list[str], cwd: Path, timeout: float, env: dict[str, str]) -> int:
    proc = subprocess.Popen(
        command,
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )
    assert proc.stdout is not None

    output_queue: queue.Queue[str | None] = queue.Queue()

    def _reader() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            output_queue.put(line.rstrip("\n"))
        output_queue.put(None)

    reader_thread = threading.Thread(target=_reader, daemon=True)
    reader_thread.start()

    summary_failed: int | None = None

    start = time.monotonic()
    while True:
        if time.monotonic() - start > timeout:
            print(f"[test-runner] timeout after {timeout:.1f}s, terminating process.", file=sys.stderr)
            _terminate_process(proc)
            return 1

        try:
            item = output_queue.get(timeout=0.2)
        except queue.Empty:
            if proc.poll() is not None and not reader_thread.is_alive():
                break
            continue

        if item is None:
            break

        print(item)
        summary_match = SUMMARY_PATTERN.match(item)
        if summary_match:
            summary_failed = int(summary_match.group(1))
            # Some backends (for example nodejs) may keep the process alive.
            # Once summary is printed, tests are finished and we can terminate.
            if proc.poll() is None:
                _terminate_process(proc)
            return 1 if summary_failed > 0 else 0

    try:
        return proc.wait(timeout=1)
    except subprocess.TimeoutExpired:
        proc.kill()
        return 1


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parents[1]
    build_bin_dir = root / "bin"
    godot_exe = Path(args.godot).expanduser().resolve()
    project_dir = Path(args.project).expanduser().resolve()
    project_bin_dir = project_dir / "bin"
    platform_name = args.platform or detect_platform()
    process_env = dict(os.environ)
    requested_backend_list, invalid_backends = _parse_requested_backends(args.backends)
    if invalid_backends:
        print(
            f"[test-runner] unknown backend(s): {', '.join(invalid_backends)}; "
            f"supported values: {', '.join(sorted(RUNTIME_BACKEND_NAMES))}",
            file=sys.stderr,
        )
        return 2
    if not requested_backend_list:
        print("[test-runner] backend filter resolved to empty list.", file=sys.stderr)
        return 2
    backend_filter = ",".join(requested_backend_list)
    process_env["PUERTS_TEST_BACKENDS"] = backend_filter
    print(f"[test-runner] backend filter: {backend_filter}")

    if not platform_name:
        print("[test-runner] could not determine platform. Pass --platform explicitly.", file=sys.stderr)
        return 2
    if not supports_runtime_tests(platform_name):
        print(f"[test-runner] skip: runtime tests are not supported on platform={platform_name}")
        return 0

    if not godot_exe.is_file():
        print(f"[test-runner] Godot executable not found: {godot_exe}", file=sys.stderr)
        return 2
    if not project_dir.is_dir():
        print(f"[test-runner] Project directory not found: {project_dir}", file=sys.stderr)
        return 2
    if not build_bin_dir.is_dir():
        print(f"[test-runner] Build output directory not found: {build_bin_dir}", file=sys.stderr)
        print("[test-runner] Build first with scons so binaries are generated in ./bin.", file=sys.stderr)
        return 2

    copied_count = sync_binaries(build_bin_dir, project_bin_dir)
    print(f"[test-runner] synced {copied_count} build artifact(s) to {project_bin_dir}")
    requested_backend_set = set(requested_backend_list)
    import_backend_set = import_safe_backends(platform_name, requested_backend_set)

    if import_backend_set != requested_backend_set:
        print("[test-runner] import phase backend mapping reduced to: " + ",".join(sorted(import_backend_set)))

    updated_gdextensions = rewrite_gdextension_files(project_dir, platform_name, import_backend_set)
    if updated_gdextensions:
        print(f"[test-runner] updated gdextension mappings: {', '.join(updated_gdextensions)}")
    else:
        print(
            f"[test-runner] no runtime libraries found for platform={platform_name}; "
            "tests may skip unsupported backends."
        )

    if should_skip_godot_import(platform_name):
        print(
            "[test-runner] skipping project import on GitHub Actions "
            f"for platform={platform_name} due to headless import crash risk."
        )
    else:
        import_env = dict(process_env)
        with temporarily_hide_gdextensions(project_dir, import_backend_set):
            import_code = run_godot_import(root, godot_exe, project_dir, args.timeout, import_env)
        if import_code != 0:
            return import_code

    # Restore full backend mapping for runtime execution after a possibly reduced import mapping.
    if import_backend_set != requested_backend_set:
        updated_gdextensions = rewrite_gdextension_files(project_dir, platform_name, requested_backend_set)
        if updated_gdextensions:
            print("[test-runner] restored runtime gdextension mappings: " + ", ".join(updated_gdextensions))

    configure_runtime_loader_env(platform_name, project_bin_dir, process_env)

    if platform_name == "linux":
        linkage_code = verify_linux_v8_dependency_linkage(project_bin_dir, requested_backend_set, process_env)
        if linkage_code != 0:
            return linkage_code

    command = [
        str(godot_exe),
        "--headless",
        "--path",
        str(project_dir),
    ]

    print(f"[test-runner] launching: {' '.join(command)}")
    print("[test-runner] streaming Godot output:")
    return_code = run_with_timeout(command, root, args.timeout, process_env)
    print(f"[test-runner] godot exit code: {return_code}")
    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
