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
