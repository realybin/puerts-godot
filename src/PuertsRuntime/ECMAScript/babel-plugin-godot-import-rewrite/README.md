# babel-plugin-godot-import-rewrite

Rewrite ESM imports from `"godot"`.

## Rewrite

Input:

```ts
import Godot, { Node as GodotNode } from "godot";
```

Output:

```js
const __godot_module__ = new Proxy(Object.create(null), {
  get(_target, key) {
    return load_type(String(key));
  }
});
const Godot = __godot_module__;
const GodotNode = load_type("Node");
```

## Result

- Default and namespace imports become `__godot_module__`
- Named imports become `load_type("TypeName")`
- The final sourcemap still points to the original TypeScript source
