## Backends

Capability checks are based on the backend function table exposed to `PuertsEnvironment`; unsupported environment calls emit an error through the configured error callback.

| Backend                | Language     | Supported Capabilities                                               | Not Supported Capabilities                                           |
|------------------------|--------------|----------------------------------------------------------------------|----------------------------------------------------------------------|
| `PuertsV8Backend`      | `ecmascript` | `tick`, `debugger`, `low_memory_notification`, `terminate_execution` | -                                                                    |
| `PuertsNodejsBackend`  | `ecmascript` | `tick`, `debugger`, `low_memory_notification`, `terminate_execution` | -                                                                    |
| `PuertsQuickjsBackend` | `ecmascript` | `low_memory_notification`                                            | `tick`, `debugger`, `terminate_execution`                            |
| `PuertsLuaBackend`     | `lua`        | -                                                                    | `tick`, `debugger`, `low_memory_notification`, `terminate_execution` |
