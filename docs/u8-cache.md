# StringName Cache

`PuertsStringNameUtf8Cache` caches the UTF-8 strings used for global names, properties, methods, and chunk names.

Call `clear()` to discard cached entries. Returned UTF-8 values and existing script values remain valid after clearing. Environments on different threads can share a cache.
