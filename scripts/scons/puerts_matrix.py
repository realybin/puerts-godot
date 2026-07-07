"""Platform and backend mapping helpers used by SConstruct."""

PLATFORM_TO_PUERTS = {
    "windows": "win",
    "macos": "osx",
    "linux": "linux",
    "android": "android",
    "ios": "ios",
    "web": "wasm",
}

ARCH_TO_PUERTS = {
    "windows": {
        "x86_64": "x64",
        "x86_32": "ia32",
        "arm64": "arm64",
    },
    "macos": {
        "x86_64": "x64",
        "arm64": "arm64",
    },
    "linux": {
        "x86_64": "x64",
        "arm64": "arm64",
    },
    "android": {
        "x86_64": "x64",
        "arm64": "arm64",
        "arm32": "armv7",
    },
    "ios": {
        "arm64": "arm64",
    },
    "web": {
        "wasm32": "wasm32",
    },
}

SUPPORTED_BACKENDS = {
    "windows": {"puerts", "papi-v8", "papi-nodejs", "papi-quickjs", "papi-lua"},
    "macos": {"puerts", "papi-v8", "papi-nodejs", "papi-quickjs", "papi-lua"},
    "linux": {"puerts", "papi-v8", "papi-nodejs", "papi-quickjs", "papi-lua"},
    "android": {"puerts", "papi-v8", "papi-nodejs", "papi-quickjs", "papi-lua"},
    "ios": {"puerts", "papi-v8", "papi-nodejs", "papi-quickjs", "papi-lua"},
    "web": {"puerts", "papi-quickjs", "papi-lua"},
}

RUNTIME_TEST_PLATFORMS = {"windows", "macos", "linux"}


def map_puerts_platform(godot_platform):
    return PLATFORM_TO_PUERTS.get(godot_platform)


def map_puerts_arch(godot_platform, godot_arch):
    return ARCH_TO_PUERTS.get(godot_platform, {}).get(godot_arch)


def supported_backends(godot_platform):
    return SUPPORTED_BACKENDS.get(godot_platform, set())


def supports_runtime_tests(godot_platform):
    return godot_platform in RUNTIME_TEST_PLATFORMS
