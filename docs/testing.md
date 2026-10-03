# Test Runner

Run from the repository root:

```powershell
python scripts/run_tests.py --godot C:\path\to\Godot_v4.x.x-stable_win64_console.exe --timeout=80
```

`--backends quickjs,lua` limits the backends. `--timeout` limits one run; a backend that keeps Godot alive is terminated after shutdown grace time. The runner syncs `bin/` to `tests/bin/` and starts `tests/main.tscn`.

Runtime tests live in `tests/tests/`; each `test_*.gd` extends [`support/test_case.gd`](../tests/tests/support/test_case.gd). Test methods take no arguments and return `bool`. Use `new_environment()` for setup; the runner disposes environments after each test. `equal()`, `check()`, and `skip()` record results.

## Browser JavaScript backend

JavaScript runtime tests (Node.js):

```bash
node --expose-gc --test tests/webgl/runtime.test.cjs
```

For the Godot smoke test, [build Core and WebGL](build.md#web-builds) in Debug mode and copy the `.wasm` files to `tests/webgl/bin/`. The supplied preset uses no threads; for a threaded build, enable `variant/thread_support`. Install matching export templates, then run:

```bash
mkdir -p build/webgl-smoke
godot --headless --path tests/webgl --editor --import
godot --headless --path tests/webgl --export-debug Web ../../build/webgl-smoke/index.html
python tests/webgl/serve.py --directory build/webgl-smoke
```

Open [localhost:8000](http://localhost:8000/) and read the page or console. Success is `globalThis.puertsWebglSmoke.complete === true` and `globalThis.puertsWebglSmoke.failed === 0`. Reload to rerun.

See the [production WASM FFI harness](../tests/webgl/ffi_smoke.md) to test the compiled Core and WebGL side modules without Godot.
