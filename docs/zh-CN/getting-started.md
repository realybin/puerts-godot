# 入门

* Godot 4.5 或更高版本，建议使用最新版本。

从成功的 [Build GDExtension](https://github.com/realybin/puerts-godot/actions/workflows/make_build.yml)
运行中下载 core 和所需后端（下例使用 v8）。两者的项目版本、Godot API 版本及精度须一致。
将两个 ZIP 解压到项目根目录，保留 `addons/puerts-godot/` 目录结构。
也可以[从源码构建](build.md)。

```gdscript
extends Node

var _env: PuertsEnvironment

func _ready() -> void:
	var pool := PuertsStringNameUtf8Cache.new()
	_env = PuertsEnvironment.new()
	_env.set_log_callback(func(_category: int, _level: int, message: String): push_error(message))
	if _env.initialize(PuertsV8Backend.new(), pool) != OK:
		return
	var value := _env.eval("40 + 2", &"bootstrap.js")
	if value != null:
		print(value.to_native()) # 42

func _process(_delta: float) -> void:
	if _env != null and _env.is_ready():
		_env.tick() # Logic tick, may not available for all backends

func _game_exiting() -> void:
	if _env != null:
		_env.dispose() # 在适当的时机释放环境，如游戏结束时
```

## 值

我们大致有以下 API

* `set_global()` `get_global()`
* `eval()`
* `get_property()` `call()` `call_method()` `PuertsScriptValue`
* `to_float()` `to_int()`... `unwrap_native()` `to_native()`

`PuertsScriptValue` 包装了脚本引擎中的值：
* 是一个对脚本值的引用，销毁包装器会使得该引用减一，当引用计数为 0 时（视情况），脚本值会被引擎回收。
* 不得跨脚本实例使用

> [!WARNING]
> 你可以在编辑器中阅读到这些 API 的文档，我们建议你详细阅读所有的API。

其余方法参见[约定](conventions.md)、[对象生命周期](lifetime.md)

## 日志

```gdscript
_env.set_log_callback(func(category: int, level: int, message: String):
    print("[%d][%d] %s" % [category, level, message])
)
_env.set_log_level(PuertsEnvironment.LOG_LEVEL_INFO)
_env.set_category_log_level(1, PuertsEnvironment.LOG_LEVEL_DEBUG)
_env.clear_category_log_level(1)
```

```javascript
PLogger.trace(1, "Sending packet");
PLogger.debug(1, "Connected");
PLogger.info(1, "Player joined");
PLogger.warning(1, "Connection is slow");
PLogger.error(1, "Connection failed");
```

- 分类 ID 使用正的 32 位整数，`0` 保留给 Puerts 内部错误和脚本异常。
- 默认输出 `INFO` 及以上级别的日志；可为指定分类单独设置最低级别。
- 回调在产生日志的线程同步执行，回调中产生的嵌套日志会被丢弃。
- 未设置有效回调时，日志输出到 Godot。传入 `Callable()` 可恢复默认输出。
- 同一个 `PuertsEnvironment` 调用 `dispose()` 后再 `initialize()`，日志配置仍然保留，无需重新设置。

## ESM

参见 [README.md](../../src/PuertsRuntime/ECMAScript/README.md)
