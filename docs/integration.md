# Integration guide

JESON is header-only. Every integration ultimately needs two things: the `include` directory on the compiler's include path and C++20 enabled. It has no third-party runtime dependencies.

## CMake

### As a subdirectory

```cmake
add_subdirectory(external/jeson)
target_link_libraries(firmware PRIVATE jeson::jeson)
```

`JESON_BUILD_TESTS` and `JESON_BUILD_EXAMPLES` default to off when JESON is a child project.

### As an installed package

Install JESON once:

```sh
cmake -S . -B build -DJESON_BUILD_TESTS=OFF -DJESON_BUILD_EXAMPLES=OFF
cmake --install build --prefix /your/prefix
```

Consume it from another project:

```cmake
find_package(jeson 0.2 CONFIG REQUIRED)
target_link_libraries(app PRIVATE jeson::jeson)
```

If the prefix is non-standard, configure the consumer with `-DCMAKE_PREFIX_PATH=/your/prefix`.

### With FetchContent

Pin a release tag or commit rather than a moving branch:

```cmake
include(FetchContent)
FetchContent_Declare(
  jeson
  GIT_REPOSITORY https://github.com/mijelsma/jeson.git
  GIT_TAG 0.2.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(jeson)
target_link_libraries(app PRIVATE jeson::jeson)
```

## Direct compiler use

```sh
c++ -std=c++20 -I/path/to/jeson/include main.cpp -o app
```

For a cross-compiler, use the corresponding compiler driver. JESON does not need a separate archive or link step.

## ESP-IDF

Clone or add JESON as a submodule beneath the application's `components` directory:

```text
your_app/
├── CMakeLists.txt
├── components/
│   └── jeson/
└── main/
```

The root `CMakeLists.txt` detects the ESP-IDF component context and calls `idf_component_register`. JESON requires a toolchain/libstdc++ configuration with C++20 enabled. Add this to the application component if necessary:

```cmake
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
```

## PlatformIO

Use the repository directly until a PlatformIO Registry release exists:

```ini
[env:device]
platform = espressif32
framework = espidf
lib_deps = https://github.com/mijelsma/jeson.git#0.2.0
build_unflags = -std=gnu++11
build_flags = -std=gnu++20
```

The included `library.json` points PlatformIO at `include/` and applies the C++20 language mode.

## Arduino

The repository contains `library.properties` and a small discovery header below `src/`, while the implementation remains in `include/jeson`. Install the full repository as a ZIP or Git checkout; do not copy only `src/`.

Arduino compatibility depends on the board core's compiler and standard library. JESON needs C++20 class-type non-type template parameters, so older AVR and older board packages are not compatible even if a flag is forced.

## Zephyr

Add JESON as a west project with `west.yml`, or place it in a module directory and add that directory to `ZEPHYR_EXTRA_MODULES`. The shipped `zephyr/module.yml` adds `include/` to the application.

Enable C++ and C++20 in the application/toolchain configuration. A typical application source can then include:

```cpp
#include <jeson/jeson.hpp>
```

## Meson

As a subproject, obtain the overridden dependency after adding JESON as `subprojects/jeson`:

```meson
jeson_sp = subproject('jeson')
jeson_dep = dependency('jeson')
executable('app', 'main.cpp', dependencies: jeson_dep)
```

The root project declaration requests C++20.

## Bazel / Bzlmod

The public target is `@jeson//:jeson`. If JESON is checked into the workspace:

```python
cc_binary(
    name = "app",
    srcs = ["main.cpp"],
    deps = ["//external/jeson:jeson"],
)
```

Set `--cxxopt=-std=c++20` (and the equivalent host option where needed) in the consuming workspace's Bazel configuration.

## Conan 2

Create a local package from a checkout:

```sh
conan create . --build=missing
```

Then require `jeson/0.2.0` and use the generated CMake target `jeson::jeson`.

## pkg-config

The CMake install creates `jeson.pc`. After adding the installation's pkg-config directory to `PKG_CONFIG_PATH`:

```sh
c++ -std=c++20 main.cpp $(pkg-config --cflags jeson) -o app
```

## Toolchain checklist

- Compile every translation unit that sees a JESON model as C++20.
- Keep model types as public aggregate structs; user-declared constructors, private members, and virtual functions prevent aggregate reflection.
- Put `#define JESON_MAX_FIELDS ...` in a common configuration header before any JESON header if changing the field cap.
- Prefer explicit `EnumNames<T>` mappings for wire protocols whose enum spellings must remain stable across C++ refactors.
