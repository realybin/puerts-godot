# babel-plugin-godot-import-rewrite

Rewrites Puerts type and helper imports into runtime global function calls.

| Source | Exports | Rewrite |
|---|---|---|
| `godot` | Godot types | `load_type("TypeName")` |
| `puerts` | `load_type`, `load_addon_type`, `to_callable`, `PLogger` | Direct references to the globals |
| `PAddon` | Addon types | `load_addon_type("FullTypeName")` |

## Configuration

Import the plugin in your `babel.config.mjs`, adjusting the path for your project:

```js
import rewrite from "./path/to/babel-plugin-godot-import-rewrite/index.mjs";

export default {
  plugins: [rewrite, "@babel/plugin-transform-typescript"],
};
```

For TypeScript, install `@babel/core` and `@babel/plugin-transform-typescript` and process `.ts` files with Babel. Run this plugin before converting ESM to another module format.

## Example

Input:

```ts
import { Node } from "godot";
import { to_callable as wrap } from "puerts";
import * as Addon from "PAddon";

const Client = Addon.WebSocketPP;
const callback = wrap(fn); // But we still recommend using name `to_callable` for clarity.
```

Output:

```js
const Node = load_type("Node");
const Client = load_addon_type("WebSocketPP");
const callback = to_callable(fn);
```

Default and namespace imports from `godot` use a Proxy:

```ts
import Godot from "godot";
const Node = Godot.Node;
```

Accessing `Node` calls `load_type("Node")` through the Proxy.

`puerts` and `PAddon` support named imports, aliases, and static namespace member access.

Use string import names or string member access for namespaced Addon types:

```ts
import { "Example.Counter" as Counter } from "PAddon";
import * as Addon from "PAddon";

const OtherCounter = Addon["Example.Counter"];
```

Type-only imports are removed.

See [Native addons](../../../../docs/addons.md) for loading and TypeScript declarations.
