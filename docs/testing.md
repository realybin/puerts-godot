# Testing

Run from the repository root:

```powershell
python scripts/run_tests.py --godot C:\path\to\Godot_v4.x.x-stable_win64_console.exe --timeout=80
```

## Browser JavaScript backend

JavaScript runtime tests (Node.js):

```bash
node --expose-gc --test tests/webgl/runtime.test.cjs
```

For Godot, [build Core and WebGL](build.md#web-builds) in Debug mode and copy the `.wasm` files to `tests/webgl/bin/`. The supplied export preset uses no threads; for a threaded build, enable `variant/thread_support`. Install matching export templates, then run:

```bash
mkdir -p build/webgl-smoke
godot --headless --path tests/webgl --editor --import
godot --headless --path tests/webgl --export-debug Web ../../build/webgl-smoke/index.html
python tests/webgl/serve.py --directory build/webgl-smoke
```

OK: `globalThis.puertsWebglSmoke.complete === true` and `globalThis.puertsWebglSmoke.failed === 0`
