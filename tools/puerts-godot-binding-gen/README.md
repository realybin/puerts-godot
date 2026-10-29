# puerts-godot-binding-gen

Generate static bindings from Godot API JSON and explicit binding rules.

## Project commands

Run from this directory:

```bash
npm ci
npm run generate
```

`generate` builds the generator and updates all nine project binding files for Godot 4.5, 4.6, and 4.7.
After `npm run build`, individual outputs can be generated with:

```bash
npm run generate:4.7
npm run generate:4.7:builtin
npm run generate:4.7:object
npm run generate:4.7:globalscope
```

The same commands exist for `4.5` and `4.6`.

## CLI

```bash
node dist/index.js --api ../../godot-cpp/gdextension/extension_api-4-7.json --profile config/puerts_builtin.json --output output/bindings.inc
node dist/index.js --api ../../godot-cpp/gdextension/extension_api-4-7.json --kind builtin --classes Vector2,Color --output output/math.inc
node dist/index.js --help
```

- `--api` and `--output` are required. Version guards follow the API header.
- Use `--profile`, or select directly with `--kind builtin|object|globalscope`.
- builtin/object require either `--classes A,B` or `--all`. globalscope accepts neither.
- `--rules` overrides the bundled `config/puerts_binding_rules.json`.
- CLI paths are relative to the current directory. The default rules path is relative to the tool.

Profiles specify selection and an optional C++ registration function:

```json
{
  "kind": "object",
  "classes": ["Object"],
  "register_function": "register_puerts_object_profile_bindings_generated"
}
```

Replace `classes` with `"all": true` to select all classes of that kind. Unknown fields, duplicate arguments, and conflicting selection options are errors.

## Binding rules

`config/puerts_binding_rules.json` is shared by builtin and Object generation:

- `methods`: native method names, signatures for ambiguous overloads, and explicit trailing defaults. `reason` explains each exception.
- `properties`: member expressions and accessor methods.
- `excluded_methods` / `excluded_properties`: known godot-cpp gaps; no fallback is generated.

Numeric arguments convert to types extracted from C++ declarations. Script scalars remain signed int64 and double. Default argument rules verify the API type and value before use. Missing native members fail C++ compilation; stale selected method/property rules fail generation.

Run focused generator tests with `npm test`.
