# IDE

我们建议使用编辑器进行开发而不是完全的 Vibe Coding/Agentic Coding。

## JetBrains CLion

我们建议使用 [JetBrains CLion](https://www.jetbrains.com/clion/)，也感谢 JetBrains 提供的免费开源许可证帮助我们开发，
CLion 的优秀开发体验加速了我们的开发工作。

在构建 [build.md](build.md) 时加入参数 `compiledb=yes`

```diff
--scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.7
++scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.7 compiledb=yes
```

然后用 JetBrains CLion 在本项目根目录打开。

### 调试 JavaScript/TypeScript

我们在支持调试器的后端中，可以使用 JetBrains CLion 的（JetBrains Rider也支持） [Run/Debug Configuration: Attach to Node.js/Chrome | CLion Documentation](https://www.jetbrains.com/help/clion/2026.2/run-debug-configuration-node-js-remote-debug.html) 功能进行调试。

## See also

* [v8-inspector.md](v8-inspector.md)
* [Compilation database | CLion Documentation](https://www.jetbrains.com/help/clion/2026.2/compilation-database.html)
