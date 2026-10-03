# WebGL FFI smoke test

Tests the compiled Core/WebGL modules without Godot. [Build both modules](../../docs/build.md#web-builds) with Debug, single precision, and `threads=false`, then run from the repository root (PowerShell):

```powershell
New-Item -ItemType Directory -Force build/webgl-ffi
em++ -std=c++17 tests/webgl/ffi_smoke.cpp `
  -I src/PuertsWebgl -I thirdparty/puerts/unity/native/puerts/include `
  -I godot-cpp/include `
  -L bin `
  bin/PuertsWebgl.web.template_debug.wasm32.nothreads.wasm `
  bin/PuertsCore.web.template_debug.wasm32.nothreads.wasm `
  -sMAIN_MODULE=1 -sWASM_BIGINT=1 -sSUPPORT_LONGJMP=wasm `
  -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=node `
  -sMODULARIZE=1 -sEXPORT_NAME=createPuertsFfiSmoke `
  -o build/webgl-ffi/smoke.js

node -e 'const path = require("path"); global.document = {}; require("./build/webgl-ffi/smoke.js")({ locateFile(file) { return path.resolve(file.includes("Puerts") ? "bin" : "build/webgl-ffi", path.basename(file)); } });'
```

`global.document = {}` satisfies the bridge's main-thread check. Test DOM access with the [Godot Web smoke project](../../docs/testing.md#browser-javascript-backend).

Success prints `[webgl-ffi] PASS`; assertion failures exit nonzero.

## Worker memory growth

Rebuild Core/WebGL with `threads=true`. In the harness command, use the threaded `.wasm` filenames (without `.nothreads`), add `-pthread -sINITIAL_MEMORY=16777216`, replace `-sENVIRONMENT=node` with `-sENVIRONMENT=web`, and output to `build/webgl-ffi-threads/smoke.js`.

Copy [ffi_threads_smoke.html](ffi_threads_smoke.html) there as `index.html`. Serve with `python tests/webgl/serve.py --directory .` and open [the harness](http://localhost:8000/build/webgl-ffi-threads/). Success prints `[webgl-ffi] MEMORY_GROWTH PASS`.

Emscripten 4.0.0 requires a generated-loader workaround: initialize `sharedModules` before `createWasm()`. This limitation affects the threaded harness, not Godot exports.
