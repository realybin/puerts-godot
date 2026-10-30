# Native addons

Addons expose C/C++ types to scripts through PESAPI. The same addon works with V8, Node.js, QuickJS, and Lua. Desktop dynamic libraries are supported on Windows, Linux, and macOS.

## Writing an addon

Use this project's `thirdparty/puerts/unity/native/puerts/include/pesapi.h` and build your addon as a dynamic library. Register types and implement callbacks using the PESAPI function tables.

See [counter.cpp](../tests/native_addon/counter.cpp) and [CMakeLists.txt](../tests/native_addon/CMakeLists.txt) for a complete example. To register `Example.Counter` and its `increment()` method:

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

`PESAPI_MODULE` exports the registration entry, which is called once when loading the addon. `define_class()` specifies the type ID, base type, namespace, type name, constructor, and finalizer. Each type ID must be a unique pointer with a stable address for the lifetime of the library.

The four counts in `set_property_info_size()` specify instance methods, static methods, instance properties, and static properties. Fill in each group by index. This example registers one instance method, `increment()`.

| Callback | Responsibility |
|---|---|
| `construct_counter` | Create a native instance and return its pointer |
| `increment_counter` | Get the instance with `get_native_holder_ptr(info)`, execute the method, and return its result with `add_return()` |
| `finalize_counter` | Release the native instance when its script wrapper is collected or the environment is disposed |

The registration entry and script type use separate names:

| Registration | Example name | Used by |
|---|---|---|
| Module name in `PESAPI_MODULE` | `PuertsTestAddon` | Second argument to `PuertsAddon.load()` |
| Full type name in `define_class()` | `Example.Counter` | `load_addon_type()` or imports from `PAddon` |

Pass `nullptr` for the namespace to use the type name directly. Use a namespace for your types to avoid collisions with Godot or other addons.

When implementing callbacks:

- Use `copy_str = false` for strings with a sufficient lifetime, such as string literals. Request a copy for temporary strings.
- Use the supplied `pesapi_ffi` and current environment. Use script values within their original environment. For persistent values, use the PESAPI reference APIs and release the references when finished.
- Handle C++ exceptions inside callbacks and report script errors with `throw_by_string()`.

Build the example from the repository root (Windows x64):

```powershell
cmake -S tests/native_addon -B build/addon -A x64
cmake --build build/addon --config Debug
```

The output is `build/addon/Debug/PuertsTestAddon.dll`. Match the addon's CPU architecture to Godot and its PESAPI version to this project.

## Loading and using an addon

Load the library on the main thread before executing scripts:

```gdscript
var err := PuertsAddon.load(
    "res://native/PuertsTestAddon.dll", "PuertsTestAddon")
if err != OK:
    return
```

Copy the library to the `native/` directory used in this example. On Linux and macOS, use the corresponding `.so` or `.dylib` path.

`PuertsAddon.load(path, module_name)` returns a Godot `Error`. It accepts absolute paths, `res://`, and `user://`. The module name matches the first argument to `PESAPI_MODULE`. It returns `ERR_CANT_OPEN` if the library or a dependency fails to load, or `ERR_METHOD_NOT_FOUND` if the registration entry for the current PESAPI version is missing.

Registered types are shared by all environments. Complete addon loading at startup before executing scripts. Loading the same path and module name again returns `OK`.

When exporting a project, deploy the library and its dependencies to a directory on disk and load them using that path. Restart the process after replacing the library.

In JavaScript, load the class by its full type name:

```javascript
const Counter = load_addon_type("Example.Counter");
const counter = new Counter();
counter.increment(); // 1
```

In Lua:

```lua
local Counter = load_addon_type("Example.Counter")
local counter = Counter()
counter:increment() -- 1
```

`load_addon_type(name)` takes a nonempty string. It throws a script exception if the type is missing or the argument is invalid.

## TypeScript imports

Configure the [Babel rewrite plugin](../src/PuertsRuntime/ECMAScript/babel-plugin-godot-import-rewrite/README.md) to import addon types from `PAddon`:

```ts
import * as Addon from "PAddon";
import { "Example.Counter" as Counter } from "PAddon";

const a = new Addon.PlainCounter();
const b = new Counter();
b.increment();
```

The example addon exposes both `PlainCounter` and the namespaced type `Example.Counter`. You can also access the latter as `Addon["Example.Counter"]`.

The plugin rewrites type references into loading calls. For example:

```ts
const Counter = Addon["Example.Counter"];
```

```js
const Counter = load_addon_type("Example.Counter");
```

Use `load_addon_type(name)` for dynamic type names.

## TypeScript declarations

Provide a separate declaration file for your addon, such as `types/example-addon.d.ts`:

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

Export names must match the full registered type names, and member signatures must match the addon's API. String export names require TypeScript 5.6 or later. Each addon can declare `PAddon`; TypeScript merges these declarations.

Write `declare module` directly in the declaration file as shown above. To reference a Godot type in a signature, use `import("godot").TypeName` and keep the file free of top-level `import` and `export` statements.

Place the addon declarations and generated `godot.d.ts` in a directory included by TypeScript. For example, if your project keeps declarations in `types/`:

```json
{
  "include": ["src/**/*.ts", "types/**/*.d.ts"]
}
```

With the declarations included, expressions such as `new Counter()` and `counter.increment()` receive completion and type checking.
