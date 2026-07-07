#!/usr/bin/env python
import os
import shutil
import subprocess
import sys

from methods import print_error
from scripts.scons.puerts_layout import (
    collect_ios_dependency_archives,
    load_backend_config,
    resolve_puerts_paths,
)
from scripts.scons.puerts_matrix import map_puerts_arch, supported_backends

# You can find documentation for SCons and SConstruct files at:
# https://scons.org/documentation.html

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

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


class ProjectInfo:
    def __init__(self, project_name, source_dir, backend_dir, libs, extra_sources=None):
        self.project_name = project_name
        self.source_dir = source_dir
        self.backend_dir = backend_dir
        self.libs = libs
        self.extra_sources = extra_sources or []


project_infos = [
    ProjectInfo(
        project_name="PuertsCore",
        source_dir="PuertsCore",
        backend_dir="puerts",
        libs=["PuertsCore"],
        extra_sources=[],
    ),
    ProjectInfo(
        project_name="PuertsV8",
        source_dir="PuertsV8",
        backend_dir="papi-v8",
        libs=["PapiV8"],
    ),
    ProjectInfo(
        project_name="PuertsNodejs",
        source_dir="PuertsNodejs",
        backend_dir="papi-nodejs",
        libs=["PapiNodejs"],
    ),
    ProjectInfo(
        project_name="PuertsQuickjs",
        source_dir="PuertsQuickjs",
        backend_dir="papi-quickjs",
        libs=["PapiQuickjs"],
    ),
    ProjectInfo(
        project_name="PuertsLua",
        source_dir="PuertsLua",
        backend_dir="papi-lua",
        libs=["PapiLua"],
    ),
]

available_backends = supported_backends(env["platform"])
if not available_backends:
    print_error(f"No puerts backend matrix configured for platform: {env['platform']}")
    sys.exit(1)

BACKEND_CONFIG_PATH = os.path.join("thirdparty", "puerts", "unity", "cli", "backends.json")
BACKEND_CONFIG = load_backend_config(BACKEND_CONFIG_PATH)
EASTL_SOURCES = [
    os.path.join("thirdparty", "EASTL", "source", "hashtable.cpp"),
    os.path.join("thirdparty", "EASTL", "source", "fixed_pool.cpp"),
    os.path.join("thirdparty", "EASTL", "source", "numeric_limits.cpp"),
]
EASTL_IOS_SUPPLEMENTAL_SOURCES = [
    # thirdparty/puerts/unity/native/puerts/CMakeLists.txt builds hashtable.cpp
    # into libPuertsCore.a on iOS, so only supplement missing objects here.
    os.path.join("thirdparty", "EASTL", "source", "fixed_pool.cpp"),
    os.path.join("thirdparty", "EASTL", "source", "numeric_limits.cpp"),
]

default_args = []


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

    try:
        lib_path, runtime_binary = resolve_puerts_paths(
            puerts_root=puerts_path,
            godot_platform=env["platform"],
            godot_arch=env["arch"],
            godot_target=env["target"],
            backend_dir=project_info.backend_dir,
            lib_name=project_info.libs[0],
        )
    except ValueError as error:
        print_error(str(error))
        sys.exit(1)

    if not os.path.isdir(lib_path):
        print_error(
            "Missing puerts build directory: "
            + lib_path
            + "\nRun: python scripts/make_puerts.py --platform "
            + env["platform"]
            + " --arch "
            + env["arch"]
            + " --config "
            + ("Debug" if env["target"] != "template_release" else "Release")
        )
        sys.exit(1)

    project_env = env.Clone()
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
        main_archive = os.path.join(lib_path, f"lib{project_info.libs[0]}.a")
        if project_info.libs and not os.path.isfile(main_archive):
            print_error(
                "Missing puerts iOS main static archive: "
                + main_archive
                + "\nRun: python scripts/make_puerts.py --platform ios --arch "
                + env["arch"]
                + " --config "
                + ("Debug" if env["target"] != "template_release" else "Release")
            )
            sys.exit(1)
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
                print_error(
                    "Missing puerts iOS core static archive: "
                    + puerts_core_archive
                    + "\nRun: python scripts/make_puerts.py --platform ios --arch "
                    + env["arch"]
                    + " --config "
                    + ("Debug" if env["target"] != "template_release" else "Release")
                )
                sys.exit(1)
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
    else:
        project_env.Append(LIBS=project_info.libs)

    sources = Glob("src/{}/*.cpp".format(project_info.source_dir))
    sources.extend(project_info.extra_sources)
    if project_info.backend_dir == "puerts":
        if env["platform"] == "ios":
            # iOS libPuertsCore.a already carries part of EASTL (e.g. hashtable.cpp).
            # Link only supplemental EASTL objects to avoid duplicate symbols.
            sources.extend(EASTL_IOS_SUPPLEMENTAL_SOURCES)
        elif env["platform"] != "web":
            # Non-web builds need local EASTL object definitions.
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
    if env["platform"] == "macos":
        project_env.AddPostAction(library, _ensure_loader_rpath_action)
    default_args.append(library)

    if runtime_binary:
        if not os.path.isfile(runtime_binary):
            print_error(
                "Missing puerts runtime binary: "
                + runtime_binary
                + "\nRun: python scripts/make_puerts.py --platform "
                + env["platform"]
                + " --arch "
                + env["arch"]
                + " --config "
                + ("Debug" if env["target"] != "template_release" else "Release")
            )
            sys.exit(1)
        installed = project_env.Install("bin/", [runtime_binary])
        if env["platform"] == "macos":
            project_env.AddPostAction(installed, _ensure_loader_rpath_action)
        default_args.extend(installed)


Default(*default_args)
