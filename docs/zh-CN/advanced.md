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

独立的原生环境可以并行执行，同一环境及其值的访问会串行执行。使用 Godot `Thread` 调度后台工作，使用脚本值时仍需持有环境引用。

Godot 对象和 addon 数据仍受各自的线程限制。回调在调用线程执行，不要在回调中同步等待另一个需要进入同一环境的线程。其他线程调用 `dispose()` 会等待当前操作结束；V8 的 `terminate_execution()` 可以中断执行。浏览器 Web 后端仍要求使用主线程。

在主线程、启动脚本工作线程之前加载 addon。关于在回调期间释放环境，参见[对象生命周期](lifetime.md#释放与重新初始化)。
