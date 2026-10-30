# 原生 Addon

Addon 通过 PESAPI 将 C/C++ 类型暴露给脚本。同一个 Addon 可以用于 V8、Node.js、QuickJS 和 Lua 后端，目前支持 Windows、Linux、macOS 桌面动态库。

## 编写 Addon

使用当前项目的 `thirdparty/puerts/unity/native/puerts/include/pesapi.h`，将 Addon 构建为动态库。通过 PESAPI 函数表注册类型和实现回调。

完整示例见 [counter.cpp](../../tests/native_addon/counter.cpp) 和 [CMakeLists.txt](../../tests/native_addon/CMakeLists.txt)。以注册 `Example.Counter` 及其 `increment()` 方法为例：

```cpp
static int counter_type_id;

static void initialize(pesapi_registry_api *api, pesapi_registry registry) {
    api->define_class(registry, &counter_type_id, nullptr,
        "Example", "Counter", construct_counter, finalize_counter, nullptr, false);
    api->set_property_info_size(registry, &counter_type_id, 1, 0, 0, 0);
    api->set_method_info(registry, &counter_type_id, 0,
        "increment", false, increment_counter, nullptr, false);
}

PESAPI_MODULE(PuertsTestAddon, initialize)
```

`PESAPI_MODULE` 导出注册入口，加载时调用一次。`define_class()` 指定类型标识、基类、命名空间、类型名、构造和析构回调；类型标识必须是库生命周期内地址稳定且唯一的指针。

`set_property_info_size()` 的四个数量依次是实例方法、静态方法、实例属性、静态属性。随后分别按索引填写成员信息。上例注册了一个实例方法 `increment()`。

| 回调 | 职责 |
|---|---|
| `construct_counter` | 创建原生实例并返回指针 |
| `increment_counter` | 通过 `get_native_holder_ptr(info)` 获取实例，执行方法并用 `add_return()` 返回结果 |
| `finalize_counter` | 在脚本包装对象被回收或环境释放时，释放原生实例 |

注册入口名和类型名分别使用：

| 注册内容 | 本例的名称 | 使用位置 |
|---|---|---|
| `PESAPI_MODULE` 的模块名 | `PuertsTestAddon` | `PuertsAddon.load()` 的第二个参数 |
| `define_class()` 的完整类型名 | `Example.Counter` | `load_addon_type()` 或 `PAddon` 导入 |

类型命名空间可传 `nullptr`，此时直接使用类型名。建议为自己的类型指定命名空间，避免与 Godot 或其他 Addon 重名。

编写回调时还需注意：

- `copy_str = false` 用于生命周期足够长的字符串，例如字符串字面量；临时字符串应请求复制。
- 使用回调传入的 `pesapi_ffi` 和当前环境。脚本值在创建它的环境内使用；长期持有时，使用 PESAPI 引用 API 并及时释放。
- 在回调内部处理 C++ 异常，脚本错误通过 `throw_by_string()` 报告。

在仓库根目录构建示例（Windows x64）：

```powershell
cmake -S tests/native_addon -B build/addon -A x64
cmake --build build/addon --config Debug
```

产物为 `build/addon/Debug/PuertsTestAddon.dll`。Addon 的 CPU 架构必须与 Godot 一致，PESAPI 版本必须与当前项目一致。

## 加载和使用

在主线程、执行脚本之前加载动态库：

```gdscript
var err := PuertsAddon.load(
    "res://native/PuertsTestAddon.dll", "PuertsTestAddon")
if err != OK:
    return
```

将构建产物复制到示例中的 `native/` 目录。Linux 和 macOS 使用相应的 `.so` 或 `.dylib` 路径。

`PuertsAddon.load(path, module_name)` 返回 Godot `Error`。路径支持绝对路径、`res://` 和 `user://`，模块名对应 `PESAPI_MODULE` 的第一个参数。库或依赖加载失败返回 `ERR_CANT_OPEN`，找不到匹配当前 PESAPI 版本的注册入口返回 `ERR_METHOD_NOT_FOUND`。

类型注册后由所有环境共享。请在启动阶段完成加载，再启动脚本执行。同一路径和模块名重复加载返回 `OK`。

导出项目时，将库和依赖部署到磁盘目录，并使用对应路径加载。替换动态库后需重启进程。

JavaScript 使用完整类型名获取类：

```javascript
const Counter = load_addon_type("Example.Counter");
const counter = new Counter();
counter.increment(); // 1
```

Lua：

```lua
local Counter = load_addon_type("Example.Counter")
local counter = Counter()
counter:increment() -- 1
```

`load_addon_type(name)` 要求一个非空字符串，类型不存在或参数错误时抛出脚本异常。

## TypeScript 导入

配置 [Babel 重写插件](../../src/PuertsRuntime/ECMAScript/babel-plugin-godot-import-rewrite/README.md)后，可从 `PAddon` 导入类型：

```ts
import * as Addon from "PAddon";
import { "Example.Counter" as Counter } from "PAddon";

const a = new Addon.PlainCounter();
const b = new Counter();
b.increment();
```

示例 Addon 同时提供了无命名空间的 `PlainCounter` 和带命名空间的 `Example.Counter`。也可以通过 `Addon["Example.Counter"]` 访问后者。

插件会将类型引用直接重写为加载调用，例如：

```ts
const Counter = Addon["Example.Counter"];
```

```js
const Counter = load_addon_type("Example.Counter");
```

动态类型名使用 `load_addon_type(name)`。

## 编写 dts

为 Addon 提供独立的声明文件，例如 `types/example-addon.d.ts`：

```ts
declare module "PAddon" {
  class Counter {
    constructor();
    increment(): number;
  }

  export { Counter as "Example.Counter" };

  export class PlainCounter {
    constructor();
    increment(): number;
  }
}
```

导出名必须与原生注册的完整类型名一致，成员签名对应 Addon 实际暴露的成员。字符串导出名需要 TypeScript 5.6 或更新版本。多个 Addon 可以各自声明 `PAddon`，TypeScript 会合并这些声明。

按上例直接在声明文件中编写 `declare module`。签名中引用 Godot 类型时，使用 `import("godot").类型名`，保持声明文件顶层没有 `import` 或 `export` 语句。

将 Addon 声明和生成器输出的 `godot.d.ts` 放入 TypeScript 已包含的目录。例如已有项目使用 `types/` 存放声明时：

```json
{
  "include": ["src/**/*.ts", "types/**/*.d.ts"]
}
```

声明加载后，`new Counter()`、`counter.increment()` 等用法就能获得补全和类型检查。
