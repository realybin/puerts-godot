# 静态绑定

静态绑定将 C++ 类型及其成员暴露给脚本。Godot 类的静态绑定与 ClassDB 反射共用类型身份，未静态绑定的成员由反射补齐。

## 示例

在场景初始化期间注册一次，早于脚本调用 `load_type`：

```cpp
#include "PuertsCore/puerts_static_binding.h"

class ExampleCounter {
public:
	double value = 0.0;
	ExampleCounter() = default;
	explicit ExampleCounter(double p_value) : value(p_value) {}
	double add(double p_amount) { value += p_amount; return value; }
};

PUERTS_SCRIPT_TYPE(ExampleCounter, "ExampleCounter")

void register_example_bindings() {
	puerts::define_type<ExampleCounter>()
			.constructor<>()
			.constructor<double>()
			.method<&ExampleCounter::add>("add")
			.property<&ExampleCounter::value>("value")
			.register_type();
}
```

```javascript
const Counter = load_type("ExampleCounter");
const counter = new Counter(10);
counter.add(2); // 12
counter.value = 5;
```

## 成员注册

| 成员 | API |
|------|-----|
| 构造函数 | `constructor<Args...>()` |
| 实例方法 | `method<&T::method>("name")` |
| 静态函数 | `static_method<&function>("name")` |
| 成员属性 | `property<&T::member>("name")` |
| 访问器属性 | `property<&getter, &setter>("name")`，省略 setter 表示只读 |
| 常量 | `constant("name", value)` 或 `constant<&getter>("name")` |
| 嵌套类型或枚举组 | `nested_type("Mode", "Example.Mode")` |
| 信号 | `signal("name")`，仅用于 Godot Object 类型 |

命名空间和枚举组通过 `define_namespace` 注册：

```cpp
puerts::define_namespace("Example.Mode")
		.constant("FAST", 1)
		.register_type();
```

## 重载与继承

重复调用 `constructor<Args...>()` 注册构造函数重载；方法重载写作 `method<&overload1, &overload2>("name")`，必要时显式转换成员指针。实例方法也接受首个参数为 `T &` 或 `const T &` 的自由函数。

重载优先匹配精确类型，其次允许转换，`Variant` 参数兜底；同等匹配报告歧义。重复类型和同一作用域内的成员重名会被拒绝。

普通 C++ 派生类型使用 `.extends<Base>()`，基类须先注册。每个类型向脚本暴露一个基类，并执行必要的指针调整。Godot 类使用 ClassDB 继承关系。

## 所有权

普通 C++ 类型的转换规则：

| 参数或返回值 | 行为 |
|--------------|------|
| `const T &` 参数 | 借用原对象 |
| 值返回 | 脚本拥有返回值 |
| 引用或 `const T *` 返回 | 生成脚本拥有的副本，类型须可复制 |
| `T *` 返回 | 借用原对象，原对象须在脚本使用期间保持有效 |
| 空指针 | 对应脚本 null |

不支持非 const 引用参数及指向 Godot 内置值的指针。普通 C++ 类型不会自动转换为 Variant。

普通 C++ 实例默认在包装器回收或环境释放时删除。可用 `.finalizer(&fn)` 自定义终结器；`.no_finalize()` 仅用于外部持有的实例，不能与构造函数或值复制返回配合使用。

Godot 类型禁止自定义终结器；Object 类型遵循[对象生命周期](lifetime.md)。内置值统一通过 Variant 包装，实例修改会写回包装。

API 定义见 [puerts_static_binding.h](../../src/PuertsCore/puerts_static_binding.h)；内置绑定的生成方式见[生成器说明](../../tools/puerts-godot-binding-gen/README.md)。
