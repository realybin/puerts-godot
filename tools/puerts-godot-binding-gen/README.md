# puerts-godot-binding-gen

Generate C++ builtin binding code for `puerts_builtin_binding.cpp`.

Pass class list from JSON config for full programmatic generation.

## Use

```bash
npm install
npm run build
```

## Generate

```bash
# single class
node dist/index.js --input ../../godot-cpp/gdextension/extension_api.json --classes PackedByteArray --output ./output/packed_byte_array.binding.inc

# all classes currently used by puerts
node dist/index.js --input ../../godot-cpp/gdextension/extension_api.json --classes-json ./config/puerts_builtin_classes.json --output ./output/puerts_builtin_bindings.generated.inc

# object profile (from extension_api.classes)
node dist/index.js --input ../../godot-cpp/gdextension/extension_api.json --classes-json ./config/puerts_object_profile_classes.json --output ./output/puerts_object_profile_bindings.generated.inc

# custom register entry name (optional)
node dist/index.js --input ../../godot-cpp/gdextension/extension_api.json --classes-json ./config/puerts_builtin_classes.json --register-function register_puerts_builtin_bindings_generated --output ./output/puerts_builtin_bindings.generated.inc

# class list auto-read from puerts file, optional
node dist/index.js --input ../../godot-cpp/gdextension/extension_api.json --classes-from-puerts-file ../../src/PuertsCore/puerts_builtin_binding.cpp --output ./output/puerts_builtin_bindings.generated.inc

# all builtin classes in JSON
node dist/index.js --input ../../godot-cpp/gdextension/extension_api.json --all-builtin --output ./output/all_builtin_bindings.generated.inc
```

## Replace

```bash
npm run generate:puerts-all
copy /Y output\\puerts_builtin_bindings.generated.inc ..\\..\\src\\PuertsCore\\puerts_builtin_bindings.generated.inc
```

## NPM shortcuts

```bash
npm run generate:packed-byte-array
npm run generate:puerts-all
npm run generate:puerts-object-profile
```

## Output

The generated file ends with:

- `void <register_function>()`

If `--classes-json` is an object config, you can set:

- `source`: `"builtin"` or `"classes"`
- `register_function`: custom generated register entry function name

Constructor generation rule:
- when `constructors` is empty and `is_instantiable != false`, emit `.constructor<>()`

You can copy the generated class registration functions and this entry function into `src/PuertsCore/puerts_builtin_binding.cpp` for replacement.
