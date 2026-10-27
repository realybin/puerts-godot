# Testing

Run from the repository root:

```powershell
python scripts/run_tests.py --godot C:\path\to\Godot_v4.x.x-stable_win64_console.exe --timeout=80
```

## Browser JavaScript backend

JavaScript runtime tests (Node.js):

```bash
node --expose-gc --test tests/web/runtime.test.cjs
```

For Godot, [build Core and Web](build.md#web-builds) in Debug mode and copy the `.wasm` files to `tests/web/bin/`. The supplied export preset uses no threads; for a threaded build, enable `variant/thread_support`. Install matching export templates, then run:

```bash
mkdir -p build/web-smoke
godot --headless --path tests/web --editor --import
godot --headless --path tests/web --export-debug Web ../../build/web-smoke/index.html
python tests/web/serve.py --directory build/web-smoke
```

OK: `globalThis.puertsWebSmoke.complete === true` and `globalThis.puertsWebSmoke.failed === 0`
