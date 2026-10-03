# Backends

Pass one backend resource to `PuertsEnvironment.initialize()`. A resource can be shared; each initialization creates an independent runtime.

| Backend | Language | `tick` | Inspector | Low memory | Terminate |
|---------|----------|--------|-----------|------------|-----------|
| `PuertsV8Backend` | ECMAScript | Yes | Yes | Yes | Yes |
| `PuertsNodejsBackend` | ECMAScript | Yes | Yes | Yes | Yes |
| `PuertsQuickjsBackend` | ECMAScript | No | No | Yes | No |
| `PuertsLuaBackend` | Lua | No | No | No | No |

Unsupported hooks report through the error callback. Choose V8 or Node.js for a process; loading both may conflict, especially on Linux. QuickJS and Lua support Web builds.

Each backend exposes `get_backend_id()`, `get_backend_name()`, and `get_language_id()`. `_puerts_get_functions_ptr()` is an internal core hook.