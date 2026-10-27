# Backends

We currently support:

| Backend                | Language   |
|------------------------|------------|
| `PuertsV8Backend`      | ECMAScript |
| `PuertsNodejsBackend`  | ECMAScript |
| `PuertsQuickjsBackend` | ECMAScript |
| `PuertsLuaBackend`     | Lua        |
| `PuertsWebBackend`     | ECMAScript |

> Use either V8 or Node.js in a process; loading both may conflict, especially on Linux.

## Browser JavaScript (Web)

`PuertsWebBackend` runs in the browser's JavaScript engine and shares the same `globalThis`, including DOM access.

We only support a singleton, so you can use only one `PuertsEnvironment`.

After installing the [Web extensions](build.md#web-builds):

```gdscript
var pool = ClassDB.instantiate("PuertsStringNameUtf8Cache")
var env = ClassDB.instantiate("PuertsEnvironment")
if env.initialize(ClassDB.instantiate("PuertsWebBackend"), pool) != OK:
	return
var title = env.eval("document.title")
if title != null:
	print(title.to_native())
```

> [!NOTE]
> Use `ClassDB.instantiate()` because Web-only extension classes are unavailable in the desktop editor and would make the project unbuildable.
