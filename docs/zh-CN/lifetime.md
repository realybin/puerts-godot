# 生命周期

学习 Rust 的所有权概念对理解本文有所帮助。

## 脚本中的 Godot 值

| 原生值                                  | 包装期间的所有权                                 |
|-----------------------------------------|--------------------------------------------------|
| `RefCounted`                            | 持有一个强引用；随包装器一起释放。               |
| 脚本创建的非 `RefCounted` 对象          | 归脚本所有；若仍存活，则在释放其桥接记录时删除。 |
| 由 Godot 传入的非 `RefCounted` 对象     | 借用；包装器既不保持其存活，也不删除它。         |
| 装箱的内置类型（`Vector2`、`Array` 等） | 一份 `Variant` 拷贝；随包装器一起释放。          |

同一对象在一个环境中复用同一条桥接记录。借用包装器按对象 ID 解析：Godot 释放该对象后，`unwrap_native()` 返回 `null`，原生访问会报告对象无效。脚本侧的包装器本身可能仍然有效。

## Godot 中的脚本值

`eval()`、`get_global()`、属性读取和调用都返回 `PuertsScriptValue`。持有该包装器即可保持其脚本值可达。`to_callable()` 会保留函数及其闭包。两者都不会保持环境存活，且值不能跨环境使用。

## 释放与重新初始化

> 以下内容适用于 V8、Node.js、QuickJS 和 Lua。[WebGL](backends.md#浏览器-javascript-webgl) 会保持其环境直到页面卸载也就是单例。

`dispose()` 会立即令 `is_ready()` 返回 false，并使所有值和可调用对象失效。清理过程会释放脚本引用、强 `RefCounted` 引用、装箱值以及脚本拥有的非 `RefCounted` 对象。借用对象仍归 Godot 所有。

如果在脚本操作或回调期间请求释放，新操作会被拒绝，清理会等待外层操作结束。在初始化、已有操作进行中或释放期间，`initialize()` 会返回 `ERR_BUSY`。环境、其值及其 UTF-8 名称缓存必须在同一个线程中使用。

在空闲环境中调用 `initialize()` 会先释放之前的运行时，即使新的初始化失败也是如此。成功调用会开启新的一代；旧值和可调用对象保持失效，必须重新获取。日志回调仍保持配置。

```gdscript
var old_value := env.eval("({ answer: 42 })")
env.dispose()
assert(not old_value.is_valid())
if env.initialize(backend, pool) == OK:
	var new_value := env.eval("({ answer: 43 })")
	assert(new_value.is_valid())
```

初始化参见[入门](getting-started.md)，所有权检查参见 [test_lifecycle.gd](../../tests/tests/test_lifecycle.gd)。
