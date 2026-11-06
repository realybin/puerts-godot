# IDE

We recommend developing in an editor rather than relying entirely on vibe coding/agentic coding.

## JetBrains CLion

We recommend [JetBrains CLion](https://www.jetbrains.com/clion/). We also thank JetBrains for providing a free open-source license that supports our development; CLion's excellent developer experience has accelerated our work.

Add the `compiledb=yes` argument when [building](build.md):

```diff
--scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.7
++scons target=template_debug platform=windows arch=x86_64 precision=single api_version=4.7 compiledb=yes
```

Then open the project root in JetBrains CLion.

### Debugging JavaScript/TypeScript

For backends that support a debugger, you can debug with JetBrains CLion's [Run/Debug Configuration: Attach to Node.js/Chrome | CLion Documentation](https://www.jetbrains.com/help/clion/2026.2/run-debug-configuration-node-js-remote-debug.html) feature (JetBrains Rider is also supported).

## See also

* [v8-inspector.md](v8-inspector.md)
* [Compilation database | CLion Documentation](https://www.jetbrains.com/help/clion/2026.2/compilation-database.html)
