# 后端

我们目前支持了

| 后端                   | 语言       |
|------------------------|------------|
| `PuertsV8Backend`      | ECMAScript |
| `PuertsNodejsBackend`  | ECMAScript |
| `PuertsQuickjsBackend` | ECMAScript |
| `PuertsLuaBackend`     | Lua        |
| `PuertsWebglBackend`   | ECMAScript |

> 一个进程中请选择 V8 或 Node.js；同时加载两者可能产生冲突，尤其在 Linux 上。

## 浏览器 JavaScript (WebGL)

`PuertsWebglBackend` 运行在浏览器的 JavaScript 引擎中，持有相同的 `globalThis`，包括 DOM 访问。

我们只会支持单例，所以你只能使用一个 `PuertsEnvironment`。

安装 [Web 扩展](build.md#web-构建)后：

```gdscript
var pool = ClassDB.instantiate("PuertsStringNameUtf8Cache")
var env = ClassDB.instantiate("PuertsEnvironment")
if env.initialize(ClassDB.instantiate("PuertsWebglBackend"), pool) != OK:
	return
var title = env.eval("document.title")
if title != null:
	print(title.to_native())
```

> [!NOTE]
> 请使用 `ClassDB.instantiate()`，因为仅 Web 可用的扩展类在桌面编辑器中不可用，会导致不可构建。
