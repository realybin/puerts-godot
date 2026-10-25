# 进阶用法

## 性能

> [!NOTE]
> 如果碰到因为我们设计上的限制而导致的性能问题，可以在 Issue 反馈。

* 复用环境、已加载的类型和脚本函数。
* 把相关工作批量放进一次脚本调用中，以减少转换和 FFI 开销。
* 自定义 C++ 类型请使用静态绑定。
* 结果类型已知时使用类型化读取。
* `Array` 等容器时，尽可能使用参数类型匹配的类型化构造函数。例如：
  ```gdscript
  Array(base: Array, type: int, class_name: StringName, script: Variant)
  Dictionary(base: Dictionary, key_type: int, key_class_name: StringName, key_script: Variant, value_type: int, value_class_name: StringName, value_script: Variant)
  ```
    请确保每个参数都使用完全符合期望的类型，以避免不必要的转换和动态类型解析。

## 重入与线程

在单一线程中使用环境及其值； WebGL 要求使用浏览器主线程。 关于在回调期间释放环境，参见[对象生命周期](lifetime.md#释放与重新初始化)。
