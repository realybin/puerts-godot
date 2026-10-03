#!/usr/bin/env python
import os
import shutil
import subprocess
import sys
from collections import namedtuple

from methods import print_error
from scripts.scons.puerts_layout import (
    collect_ios_dependency_archives,
    load_backend_config,
    resolve_puerts_paths,
)
from scripts.scons.puerts_matrix import BACKENDS, map_puerts_arch, supported_backends

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


ProjectInfo = namedtuple("ProjectInfo", "project_name source_dir backend_dir lib_name")
project_infos = [
    ProjectInfo(entry["source"], entry["source"], entry["native"], entry["library"]) for entry in BACKENDS.values()
]

available_backends = supported_backends(env["platform"])
if not available_backends:
    print_error(f"No puerts backend matrix configured for platform: {env['platform']}")
    sys.exit(1)

if requested_backend_argument is not None:
    backend_names = {name: entry["native"] for name, entry in BACKENDS.items()}
    requested_backends = set()
    for token in requested_backend_argument.split(","):
        name = token.strip().lower()
        if not name:
            continue
        backend = backend_names.get(name, name)
        if backend not in backend_names.values():
            print_error(f"Unknown backend: {token}")
            sys.exit(1)
        requested_backends.add(backend)
    available_backends &= requested_backends
    if not available_backends:
        print_error("No requested backends are available for this platform")
        sys.exit(1)
    # All language backends use the core extension's shared type registry.
    available_backends.add("puerts")

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


def _embed_webgl_runtime(target, source, env):
    with open(str(source[0]), encoding="utf-8") as runtime_file:
        runtime_source = runtime_file.read()
    if ')PUERTS_WEBGL"' in runtime_source:
        raise ValueError("WebGL runtime contains the reserved raw-string delimiter")
    os.makedirs(os.path.dirname(str(target[0])), exist_ok=True)
    with open(str(target[0]), "w", encoding="utf-8", newline="\n") as header:
        header.write(
            "// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors\n"
            "// SPDX-License-Identifier: BSD-3-Clause\n"
            "// Generated from puerts_webgl_runtime.js by SCons.\n"
            "#pragma once\n"
            'inline constexpr char puerts_webgl_runtime_source[] = R"PUERTS_WEBGL(\n'
            + runtime_source
            + '\n)PUERTS_WEBGL";\n'
        )


def fail_missing_puerts_artifact(path):
    puerts_config = "Debug" if env["target"] != "template_release" else "Release"
    print_error(
        "Missing puerts artifact: "
        + path
        + "\nRun: python scripts/make_puerts.py --platform "
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
        raise RuntimeError("failed to add @loader_path rpath for {}: {}".format(target_path, update.stderr.strip()))


for project_info in project_infos:
    if project_info.backend_dir not in available_backends:
        print(f"[SConstruct] Skip backend {project_info.backend_dir} on platform {env['platform']}")
        continue

    project_env = env.Clone()
    runtime_binary = None
    if project_info.backend_dir != "webgl":
        try:
            lib_path, runtime_binary = resolve_puerts_paths(
                puerts_root=puerts_path,
                godot_platform=env["platform"],
                godot_arch=env["arch"],
                godot_target=env["target"],
                backend_dir=project_info.backend_dir,
                lib_name=project_info.lib_name,
            )
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
        puerts_arch = map_puerts_arch(env["platform"], env["arch"])
        ios_archives, missing_patterns = collect_ios_dependency_archives(
            backend_config=BACKEND_CONFIG,
            puerts_root=puerts_path,
            backend_dir=project_info.backend_dir,
            puerts_arch=puerts_arch,
        )
        main_archive = os.path.join(lib_path, f"lib{project_info.lib_name}.a")
        if not os.path.isfile(main_archive):
            fail_missing_puerts_artifact(main_archive)
        link_archives = [main_archive]
        if project_info.backend_dir != "puerts":
            puerts_core_lib_path, _ = resolve_puerts_paths(
                puerts_root=puerts_path,
                godot_platform=env["platform"],
                godot_arch=env["arch"],
                godot_target=env["target"],
                backend_dir="puerts",
                lib_name="PuertsCore",
            )
            puerts_core_archive = os.path.join(puerts_core_lib_path, "libPuertsCore.a")
            if not os.path.isfile(puerts_core_archive):
                fail_missing_puerts_artifact(puerts_core_archive)
            link_archives.append(puerts_core_archive)
        if missing_patterns:
            print_error(
                "Missing puerts iOS dependency archives for backend "
                + project_info.backend_dir
                + ":\n  - "
                + "\n  - ".join(missing_patterns)
            )
            sys.exit(1)
        link_archives.extend(ios_archives)
        project_env.Append(LINKFLAGS=[f"-Wl,-force_load,{archive}" for archive in link_archives])
    elif project_info.backend_dir != "webgl":
        project_env.Append(LIBS=[project_info.lib_name])

    sources = Glob("src/{}/*.cpp".format(project_info.source_dir))
    if project_info.backend_dir == "webgl":
        runtime_header = project_env.Command(
            "src/gen/puerts_webgl_runtime.generated.h",
            "src/PuertsWebgl/puerts_webgl_runtime.js",
            _embed_webgl_runtime,
        )
        project_env.Append(CPPPATH=["src/gen"])
        # Link the existing core module so the class registry is shared and
        # the Emscripten dynamic loader loads it before the browser backend.
        project_env.Append(LINKFLAGS=[str(built_libraries["puerts"][0])])
        sources = project_env.SharedObject(sources)
        project_env.Depends(sources, runtime_header)
    if project_info.backend_dir == "puerts":
        if env["platform"] in ("ios", "web"):
            # The native static archive carries part of EASTL (e.g. hashtable.cpp).
            # Link only supplemental EASTL objects to avoid duplicate symbols.
            sources.extend(EASTL_STATIC_CORE_SUPPLEMENTAL_SOURCES)
        else:
            # Builds using the shared PuertsCore library need local definitions.
            sources.extend(EASTL_SOURCES)

    if env["target"] in ["editor", "template_debug"]:
        try:
            doc_sources = Glob("src/{}/doc_classes/*.xml".format(project_info.source_dir))
            if doc_sources:
                doc_data = env.GodotCPPDocData(
                    "src/{}/gen/doc_data.gen.cpp".format(project_info.source_dir),
                    source=doc_sources,
                )
                sources.append(doc_data)
        except AttributeError:
            print("Not including class reference as we're targeting a pre-4.3 baseline.")

    lib_filename = "{}{}{}{}".format(
        env.subst("$SHLIBPREFIX"),
        project_info.project_name,
        env["suffix"],
        env.subst("$SHLIBSUFFIX"),
    )

    library = project_env.SharedLibrary(
        "bin/{}".format(lib_filename),
        source=sources,
    )
    built_libraries[project_info.backend_dir] = library
    if project_info.backend_dir == "webgl":
        project_env.Depends(library, built_libraries["puerts"])
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
