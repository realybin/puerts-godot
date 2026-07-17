# puerts-godot-dts-gen

This is a tool to generate godot.d.ts for language server.

## Usage

> Note: Experimental, you should not publish this package.

```bash
cd tools/puerts-godot-dts-gen
npm install
npm run build
node dist/index.js --input ../../godot-cpp/gdextension/extension_api.json --output ./output/godot.d.ts
```

Or:

```bash
cd tools/puerts-godot-dts-gen
npm install
npm run build
npm run generate
```

## Documentation

When the input JSON contains documentation, the generator emits it as JSDoc. Generate a documented API with Godot, then pass it through the same `--input` interface:

```bash
godot --headless --dump-extension-api-with-docs
node dist/index.js --input ./extension_api.json --output ./output/godot.d.ts
```

## Native primitive types

Generated signatures keep JavaScript primitive values assignable as `boolean`, `number`, and `string`, while exposing their native meaning through documented aliases. This includes general aliases such as `Bool`, `String`, `Int`, `Float`, and `Bitfield`, plus metadata-driven aliases such as `Float32`, `Float64`, `Int32`, and `UInt64`. The generator uses `extension_api.json` `meta` values when available and derives `Real` from the API header precision.

Godot utility functions, singletons, and global enums are declared only under the separately bound `GlobalScope` class. API global constants that have no runtime binding are not emitted.
