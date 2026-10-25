# StringName 缓存

`PuertsStringNameUtf8Cache` 缓存用于全局名称、属性、方法和代码块名称的 u8 字符串。

调用 `clear()` 可丢弃缓存条目。清除后，已返回的 UTF-8 值和现有脚本值仍然有效。仅在同一线程中使用的环境之间共享缓存。
