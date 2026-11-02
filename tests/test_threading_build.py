"""Check threading configuration without invoking a compiler."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from misc.scripts import make_puerts
from misc.scripts.scons.puerts_layout import NATIVE_BUILD_METADATA, validate_native_build


class ThreadingBuildTests(unittest.TestCase):
    def test_native_mode_must_match(self):
        with tempfile.TemporaryDirectory() as directory:
            metadata = Path(directory) / NATIVE_BUILD_METADATA
            for enabled in (True, False):
                metadata.write_text(json.dumps({"thread_safe": enabled}), encoding="utf-8")
                validate_native_build(directory, enabled)
                with self.assertRaisesRegex(ValueError, "does not match"):
                    validate_native_build(directory, not enabled)

    def test_missing_or_invalid_metadata_requires_rebuild(self):
        with tempfile.TemporaryDirectory() as directory:
            metadata = Path(directory) / NATIVE_BUILD_METADATA
            with self.assertRaises(ValueError):
                validate_native_build(directory, True)
            for content in ("invalid", "[]", "{}", '{"thread_safe": "yes"}'):
                metadata.write_text(content, encoding="utf-8")
                with self.assertRaises(ValueError):
                    validate_native_build(directory, True)

    def test_native_build_enables_upstream_thread_safety_by_default(self):
        platform = make_puerts.PLATFORMS["windows"]
        backend = make_puerts.BACKENDS["v8"]
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / backend.native_dir).mkdir()
            with patch.object(make_puerts, "NATIVE_DIR", Path(directory)), patch.object(make_puerts, "run") as run:
                make_puerts.build_backend(platform, "x64", "Debug", backend)
                self.assertIn("--thread_safe", run.call_args.args[0])
                make_puerts.build_backend(platform, "x64", "Debug", backend, thread_safe=False)
                self.assertNotIn("--thread_safe", run.call_args.args[0])

    def test_nonthreaded_web_does_not_require_native_mutexes(self):
        platform = make_puerts.PLATFORMS["web"]
        backend = make_puerts.BACKENDS["core"]
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / backend.native_dir).mkdir()
            with patch.object(make_puerts, "NATIVE_DIR", Path(directory)), patch.object(make_puerts, "run") as run:
                make_puerts.build_backend(platform, "wasm32", "Debug", backend, threads=False)
                self.assertNotIn("--thread_safe", run.call_args.args[0])
                make_puerts.build_backend(platform, "wasm32", "Debug", backend, threads=True)
                self.assertIn("--thread_safe", run.call_args.args[0])


if __name__ == "__main__":
    unittest.main()
