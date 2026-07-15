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

## Generate versioned project bindings

First generate the `godot-cpp` headers for the same API version from the project
root, then build this generator:

```bash
scons api_version=4.5 godot-cpp/gen/include/godot_cpp/core/version.hpp
cd tools/puerts-godot-binding-gen
npm ci
npm run build
```

Run these three commands to update the project bindings for Godot 4.5:

```bash
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-5.json --godot-version-macro 4.5 --classes-json ./config/puerts_builtin_classes.json --output ../../src/PuertsCore/puerts_builtin_bindings.4_5.generated.inc
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-5.json --godot-version-macro 4.5 --classes-json ./config/puerts_object_profile_classes.json --output ../../src/PuertsCore/puerts_object_profile_bindings.4_5.generated.inc
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-5.json --godot-version-macro 4.5 --target globalscope --output ../../src/PuertsCore/puerts_global_scope.4_5.generated.inc
```

For Godot 4.6, use `api_version=4.6`, `extension_api-4-6.json`, macro
`4.6`, and suffix `4_6`. For Godot 4.7, use `api_version=4.7`,
`extension_api.json`, macro `4.7`, and suffix `4_7`. Each generated file wraps
its contents in the corresponding `GODOT_VERSION_MAJOR` and
`GODOT_VERSION_MINOR` condition and is committed to the repository.

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
