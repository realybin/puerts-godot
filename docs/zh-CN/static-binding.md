# 静态绑定

静态绑定通过模板回调把脚本调用连接到 C++ 构造函数、方法和属性。`load_type(name)` 会先查询静态注册表，再查询 Godot ClassDB。

## 例子

下面的示例绑定了一个普通 C++ 类型，且未重新声明 Godot 内置类型：

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

重复调用 `constructor<Args...>()` 即可注册构造函数重载。在场景初始化期间注册一次，需早于脚本调用 `load_type`：

```javascript
const Counter = load_type("ExampleCounter");
const counter = new Counter(10.0);
counter.add(2.0); // 12
counter.value = 5.0;
```

## 支持的构建器

| 成员 | 构建器 |
|------|--------|
| 构造函数 | `constructor<Args...>()` |
| 实例方法 | `method<&T::method>("name")` |
| 方法重载 | `method<&overload1, &overload2>("name")` |
| 静态函数 | `static_method<&function>("name")` |
| 成员属性 | `property<&T::member>("name")` |
| 访问器属性 | `property<&getter, &setter>("name")` |
| 常量 | `constant<VALUE>("name")` 或 `constant<&getter>("name")` |
| 枚举组 | `enum_type<EnumTag>("name")` |
| 信号 | `signal<SignalName>("name")` |
| 终结器 | `finalizer(&fn)` / `no_finalize()` |

重载方法请使用显式的成员指针转换。实例方法也可以是首个参数为 `T &` 或 `const T &` 的自由函数，适用于默认参数适配器和自定义属性访问器。

## 继承与生命周期

先注册基类，再在派生类型上使用 `.extends<Base>()`。每个类型都需要自己的 `PUERTS_SCRIPT_TYPE` 和构造函数。注册表只建模一层简单的基类关系；多重继承或虚继承不会被调整。

普通 C++ 值使用生成的终结器，在其脚本包装器被回收或环境被释放时删除。可用 `.finalizer(&fn)` 替换它，或在实例由其他方持有时使用 `.no_finalize()`。不要给 Godot `Object` 的子类设置终结器。Godot `Object` 的子类遵循[对象分配与生命周期](lifetime.md)中的桥接所有权规则。普通 C++ 类型不会自动获得 Godot `Variant` 表示。

构建器和宏声明于 [puerts_static_binding.h](../../src/PuertsCore/puerts_static_binding.h)。生成的绑定由 [puerts_builtin_binding.cpp](../../src/PuertsCore/puerts_builtin_binding.cpp) 注册。
