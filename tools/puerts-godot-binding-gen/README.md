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
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --classes PackedByteArray --output ./output/packed_byte_array.binding.inc

# all classes currently used by puerts
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --classes-json ./config/puerts_builtin_classes.json --output ./output/puerts_builtin_bindings.generated.inc

# object profile (from extension_api.classes)
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --classes-json ./config/puerts_object_profile_classes.json --output ./output/puerts_object_profile_bindings.generated.inc

# custom register entry name (optional)
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --classes-json ./config/puerts_builtin_classes.json --register-function register_puerts_builtin_bindings_generated --output ./output/puerts_builtin_bindings.generated.inc

# class list auto-read from puerts file, optional
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --classes-from-puerts-file ../../src/PuertsCore/puerts_builtin_binding.cpp --output ./output/puerts_builtin_bindings.generated.inc

# all builtin classes in JSON
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --all-builtin --output ./output/all_builtin_bindings.generated.inc
```

## Generate versioned project bindings

First generate the `godot-cpp` headers for the same API version from the project
root, then build this generator:

```bash
scons api_version=4.7 godot-cpp/gen/include/godot_cpp/core/version.hpp
cd tools/puerts-godot-binding-gen
npm ci
npm run build
```

Run these three commands to update the project bindings for Godot 4.7:

```bash
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --godot-version-macro 4.7 --classes-json ./config/puerts_builtin_classes.json --output ../../src/PuertsCore/puerts_builtin_bindings.4_7.generated.inc
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --godot-version-macro 4.7 --classes-json ./config/puerts_object_profile_classes.json --output ../../src/PuertsCore/puerts_object_profile_bindings.4_7.generated.inc
node dist/index.js --input ../../godot-cpp/gdextension/extension_api-4-7.json --godot-version-macro 4.7 --target globalscope --output ../../src/PuertsCore/puerts_global_scope.4_7.generated.inc
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

Generated registrations use the fluent API from `puerts_static_binding.h`; they do not call internal callback or conversion APIs. Each constructor overload emits a separate `.constructor<Args...>()` call. When `constructors` is empty and `is_instantiable != false`, the generator emits `.constructor<>()`.
