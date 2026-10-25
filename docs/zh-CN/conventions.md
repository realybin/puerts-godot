# 约定

我们提供了一些辅助函数。

## 加载类型

`load_type(name)` 会先查询静态绑定，再查询 Godot ClassDB。对重复使用的类型请做缓存。

```javascript
const Vector2 = load_type("Vector2");
const value = new Vector2(3.0, 4.0);
value.length(); // 5
```

> [!NOTE]
> 从 ClassDB 获得的类型只支持无参构造。静态绑定可以暴露其他构造函数。

### 使用 import

为了便利使用，我们还提供了基于重写输出的 Babel 插件，让你能够使用 import 来加载类型。

也就是可以：

```javascript
import Godot, { Node as GodotNode } from "godot";
```

更多信息请参见 [README.md](../../src/PuertsRuntime/ECMAScript/babel-plugin-godot-import-rewrite/README.md)

## 类型映射

传递给脚本的 Godot `Variant` 值的默认映射：

| Godot                                              | 脚本             |
|----------------------------------------------------|------------------|
| `null`  `bool` `int` `float` String`、`StringName` | Cast             |
| `Object`                                           | 包装             |
| 内置类型（如 `Vector2`、`Array`、`Dictionary` 等） | 包装             |
| `PuertsScriptValue`                                | Raw Script Value |

`PackedByteArray` 的 Cast：

- `VARIANT_TO_SCRIPT_PACKED_BYTE_ARRAY_CAST`：将 `PackedByteArray` 复制到脚本的 ByteArray 类型。
- `SCRIPT_TO_VARIANT_PACKED_BYTE_ARRAY_CAST`：转换为 `Variant` 时（包括 `to_native()`）将脚本二进制复制到 `PackedByteArray`。

但是这并不推荐，会带来额外的开销；所以默认都是包装。

## Callable & Signal

`to_callable(function)` 会保留该函数及其闭包。要断开连接，请保留该可调用对象：

```javascript
const callback = to_callable(() => log_info("timeout"));
timer.timeout.connect(callback);
timer.timeout.disconnect(callback);
```

释放环境会使脚本可调用对象失效。信号连接也会保持其可调用对象，直到断开连接或其发射者被销毁。

## 枚举、常量与 GlobalScope

枚举是嵌套的静态成员。常量是只读静态属性。全局函数和常量是 `GlobalScope` 的成员。

```javascript
const Vector2 = load_type("Vector2");
Vector2.Axis.AXIS_X; // 0
Vector2.ZERO;        // 值常量

const GlobalScope = load_type("GlobalScope");
GlobalScope.sin(1.0);
```

## 运算符

运算符重载是以运算符映射命名的静态方法：

```javascript
const sum = Vector2.op_Addition(new Vector2(1, 2), new Vector2(3, 4));
```

> [!NOTE]
> 我们这样做的原因是 JavaScript 不支持运算符重载。

更多的运算符映射请参见[运算符映射](../../tools/puerts-godot-operator-model/index.js)。
