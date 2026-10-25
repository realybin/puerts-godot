# 测试

请在仓库根目录运行：

```powershell
python scripts/run_tests.py --godot C:\path\to\Godot_v4.x.x-stable_win64_console.exe --timeout=80
```

## 浏览器 JavaScript 后端

JavaScript 运行时测试（Node.js）：

```bash
node --expose-gc --test tests/webgl/runtime.test.cjs
```

对于 Godot，请以 Debug 模式[构建 Core 和 WebGL](build.md#web-构建)，并将 `.wasm` 文件复制到 `tests/webgl/bin/`。随附的导出预设不使用线程；如需线程构建，请启用 `variant/thread_support`。安装匹配的导出模板，然后运行：

```bash
mkdir -p build/webgl-smoke
godot --headless --path tests/webgl --editor --import
godot --headless --path tests/webgl --export-debug Web ../../build/webgl-smoke/index.html
python tests/webgl/serve.py --directory build/webgl-smoke
```

OK: `globalThis.puertsWebglSmoke.complete === true` and `globalThis.puertsWebglSmoke.failed === 0`
