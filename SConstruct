#!/usr/bin/env python
import os
import shutil
import subprocess
import sys

from methods import print_error
from misc.scripts.manifest import BACKENDS, MANIFEST, PLATFORMS, TARGETS
from misc.scripts.scons.puerts_layout import (
    collect_ios_dependency_archives,
    load_backend_config,
    resolve_puerts_paths,
)

# You can find documentation for SCons and SConstruct files at:
# https://scons.org/documentation.html

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

requested_backend_argument = ARGUMENTS.pop("backends", None)
env = SConscript("godot-cpp/SConstruct")

puerts_path = os.path.join("thirdparty", "puerts", "unity", "native")
if not os.path.exists(puerts_path):
    print_error(
        "Puerts submodule not found. Please run 'git submodule update --init --recursive' to initialize the Puerts submodule."
    )
    sys.exit(1)

env.Append(CPPPATH=["src/"])
env.Append(CPPPATH=[os.path.join(puerts_path, "puerts", "include")])
env.Append(CPPPATH=[os.path.join("thirdparty", "EASTL", "include")])
env.Append(CPPPATH=[os.path.join("thirdparty", "EABase", "include", "Common")])
if env["platform"] == "windows":
    env.Append(CCFLAGS=["/bigobj"])


available_backends = set(MANIFEST.supported_backends(env["platform"]))
if not available_backends:
    print_error(f"No puerts backend matrix configured for platform: {env['platform']}")
    sys.exit(1)

if requested_backend_argument is not None:
    requested_backends = {token.strip().lower() for token in requested_backend_argument.split(",") if token.strip()}
    unknown_backends = requested_backends - BACKENDS.keys()
    if unknown_backends:
        print_error(f"Unknown backends: {', '.join(sorted(unknown_backends))}")
        sys.exit(1)
    available_backends &= requested_backends
    if not available_backends:
        print_error("No requested backends are available for this platform")
        sys.exit(1)

available_backends = set(MANIFEST.resolve_backends(available_backends))

BACKEND_CONFIG_PATH = os.path.join("thirdparty", "puerts", "unity", "cli", "backends.json")
BACKEND_CONFIG = load_backend_config(BACKEND_CONFIG_PATH)
EASTL_SOURCES = [
    os.path.join("thirdparty", "EASTL", "source", "hashtable.cpp"),
    os.path.join("thirdparty", "EASTL", "source", "fixed_pool.cpp"),
    os.path.join("thirdparty", "EASTL", "source", "numeric_limits.cpp"),
]
# thirdparty/puerts/unity/native/puerts/CMakeLists.txt builds hashtable.cpp
# into libPuertsCore.a on iOS/Web, so only supplement missing objects there.
EASTL_STATIC_CORE_SUPPLEMENTAL_SOURCES = [s for s in EASTL_SOURCES if "hashtable" not in s]

default_args = []
built_libraries = {}


def _embed_web_runtime(target, source, env):
    with open(str(source[0]), encoding="utf-8") as runtime_file:
        runtime_source = runtime_file.read()
    if ')PUERTS_WEB"' in runtime_source:
        raise ValueError("Web runtime contains the reserved raw-string delimiter")
    os.makedirs(os.path.dirname(str(target[0])), exist_ok=True)
    with open(str(target[0]), "w", encoding="utf-8", newline="\n") as header:
        header.write(
            "// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors\n"
            "// SPDX-License-Identifier: BSD-3-Clause\n"
            "// Generated from puerts_web_runtime.js by SCons.\n"
            "#pragma once\n"
            'inline constexpr char puerts_web_runtime_source[] = R"PUERTS_WEB(\n' + runtime_source + '\n)PUERTS_WEB";\n'
        )


def fail_missing_puerts_artifact(path):
    puerts_config = "Debug" if env["target"] != "template_release" else "Release"
    print_error(
        "Missing puerts artifact: "
        + path
        + "\nRun: python misc/scripts/make_puerts.py --platform "
        + env["platform"]
        + " --arch "
        + env["arch"]
        + " --config "
        + puerts_config
    )
    sys.exit(1)


def _ensure_loader_rpath_action(target, source=None, env=None, **_kwargs):
    if not target:
        return

    target_path = str(target[0])
    if not (target_path.endswith(".dylib") and os.path.isfile(target_path)):
        return

    install_name_tool = shutil.which("install_name_tool")
    if not install_name_tool:
        raise RuntimeError(f"install_name_tool not found while fixing rpath for: {target_path}")

    otool = shutil.which("otool")
    if otool:
        inspect = subprocess.run([otool, "-l", target_path], capture_output=True, text=True, check=False)
        if inspect.returncode == 0 and "@loader_path" in inspect.stdout:
            return

    update = subprocess.run(
        [install_name_tool, "-add_rpath", "@loader_path", target_path],
        capture_output=True,
        text=True,
        check=False,
    )
    if update.returncode != 0:
        raise RuntimeError(f"failed to add @loader_path rpath for {target_path}: {update.stderr.strip()}")


platform = PLATFORMS[env["platform"]]
target = TARGETS[env["target"]]

for backend in BACKENDS.values():
    if backend.name not in available_backends:
        print(f"[SConstruct] Skip backend {backend.name} on platform {env['platform']}")
        continue

    project_env = env.Clone()
    runtime_binary = None
    if backend.name != "web":
        try:
            lib_path, runtime_binary = resolve_puerts_paths(puerts_path, platform, env["arch"], target, backend)
        except ValueError as error:
            print_error(str(error))
            sys.exit(1)

        if not os.path.isdir(lib_path):
            fail_missing_puerts_artifact(lib_path)
        project_env.Append(LIBPATH=[lib_path])
    if env["platform"] == "linux":
        # Ensure runtime dependencies are resolved from the extension directory first.
        project_env.Append(LINKFLAGS=["-Wl,-rpath,$$ORIGIN"])

    # iOS backends are produced as static archives (.a). Their transitive native
    # dependencies are not propagated when consumed outside CMake, so we must
    # explicitly pass all archives in the backend output directory to the linker.
    if env["platform"] == "ios":
        puerts_arch = platform.map_arch(env["arch"])
        ios_archives, missing_patterns = collect_ios_dependency_archives(
            backend_config=BACKEND_CONFIG,
            puerts_root=puerts_path,
            backend_dir=backend.native_dir,
            puerts_arch=puerts_arch,
        )
        main_archive = os.path.join(lib_path, f"lib{backend.library}.a")
        if not os.path.isfile(main_archive):
            fail_missing_puerts_artifact(main_archive)
        link_archives = [main_archive]
        if backend.name != "core":
            puerts_core_lib_path, _ = resolve_puerts_paths(puerts_path, platform, env["arch"], target, BACKENDS["core"])
            puerts_core_archive = os.path.join(puerts_core_lib_path, "libPuertsCore.a")
            if not os.path.isfile(puerts_core_archive):
                fail_missing_puerts_artifact(puerts_core_archive)
            link_archives.append(puerts_core_archive)
        if missing_patterns:
            print_error(
                "Missing puerts iOS dependency archives for backend "
                + backend.name
                + ":\n  - "
                + "\n  - ".join(missing_patterns)
            )
            sys.exit(1)
        link_archives.extend(ios_archives)
        project_env.Append(LINKFLAGS=[f"-Wl,-force_load,{archive}" for archive in link_archives])
    elif backend.name != "web":
        project_env.Append(LIBS=[backend.library])

    sources = Glob(f"src/{backend.source_dir}/*.cpp")
    if backend.name == "web":
        runtime_header = project_env.Command(
            "src/gen/puerts_web_runtime.generated.h",
            "src/PuertsWeb/puerts_web_runtime.js",
            _embed_web_runtime,
        )
        project_env.Append(CPPPATH=["src/gen"])
        # Link the existing core module so the class registry is shared and
        # the Emscripten dynamic loader loads it before the browser backend.
        project_env.Append(LINKFLAGS=[str(built_libraries["core"][0])])
        sources = project_env.SharedObject(sources)
        project_env.Depends(sources, runtime_header)
    if backend.name == "core":
        if env["platform"] == "linux":
            project_env.Append(LIBS=["dl"])
        if env["platform"] in ("ios", "web"):
            # The native static archive carries part of EASTL (e.g. hashtable.cpp).
            # Link only supplemental EASTL objects to avoid duplicate symbols.
            sources.extend(EASTL_STATIC_CORE_SUPPLEMENTAL_SOURCES)
        else:
            # Builds using the shared PuertsCore library need local definitions.
            sources.extend(EASTL_SOURCES)

    if env["target"] in ["editor", "template_debug"]:
        try:
            doc_sources = Glob(f"src/{backend.source_dir}/doc_classes/*.xml")
            if doc_sources:
                doc_data = env.GodotCPPDocData(
                    f"src/{backend.source_dir}/gen/doc_data.gen.cpp",
                    source=doc_sources,
                )
                sources.append(doc_data)
        except AttributeError:
            print("Not including class reference as we're targeting a pre-4.3 baseline.")

    lib_filename = "{}{}{}{}".format(
        env.subst("$SHLIBPREFIX"),
        backend.source_dir,
        env["suffix"],
        env.subst("$SHLIBSUFFIX"),
    )

    library = project_env.SharedLibrary(
        f"bin/{lib_filename}",
        source=sources,
    )
    built_libraries[backend.name] = library
    if backend.name == "web":
        project_env.Depends(library, built_libraries["core"])
    if env["platform"] == "macos":
        project_env.AddPostAction(library, _ensure_loader_rpath_action)
    default_args.append(library)

    if runtime_binary:
        if not os.path.isfile(runtime_binary):
            fail_missing_puerts_artifact(runtime_binary)
        installed = project_env.Install("bin/", [runtime_binary])
        if env["platform"] == "macos":
            project_env.AddPostAction(installed, _ensure_loader_rpath_action)
        default_args.extend(installed)


Default(*default_args)
