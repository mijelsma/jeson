# JESON

JESON is a small, header-only C++20 JSON library for typed embedded configuration, commands, and telemetry. Its schema syntax is inspired by [reflect-cpp](https://github.com/getml/reflect-cpp) and Python's Pydantic: describe the wire name and C++ type once, then parse, validate, access, and serialize the same object.

```cpp
#include <jeson/jeson.hpp>

struct Device {
    jeson::Field<"id", jeson::Required<std::string>> id;
    jeson::Field<"port", jeson::Validator<int,
        jeson::Minimum<1>, jeson::Maximum<65535>>> port = 8080;
    jeson::Field<"enabled", bool> enabled = true;
};

Device device;
auto errors = jeson::parse(R"({"id":"sensor-01","port":9000})", device);
if (errors) {
    device.port = 9001;              // ordinary assignment; no .value()
    auto json = jeson::toJson(device, jeson::pretty);
}
```

There is no registration macro, generated schema, runtime type table, DOM, exception-based error path, or compiled library to link.

## What it supports

- Macro-free reflection of aggregate structs through `jeson::Field<"json_name", T>`
- `bool`, integral and floating-point values, strings, enums, nested structs, `std::optional`, and `std::vector`
- Transparent nested access (`settings.wifi.ssid`, not `settings.wifi.value().ssid`)
- Pydantic-style constraints: `Minimum`, `Maximum`, `MinSize`, and `MaxSize`
- Required input keys with `Required<T>`
- Automatic enum names, plus explicit mappings for custom or sparse values
- Partial update models with `Patch<T>` and per-section completeness policies
- Accumulated, path-aware errors instead of stopping at the first bad field
- Compact, pretty, and omit-null serialization
- Lenient handling of unknown keys for forward/backward-compatible device configuration

JESON parses directly into the destination object in one recursive pass. It does use the standard containers selected by your model—such as `std::string` and `std::vector`—so “header-only” does not mean “zero allocation.”

## Requirements

- C++20
- A standard library with `std::string`, `std::string_view`, `std::optional`, and `std::vector`
- GCC, Clang/AppleClang, or MSVC

The default reflection limit is 32 direct fields per struct. Nested structs each get their own limit. See [configuration](#configuration) to raise it.

## Integrate it

### Copy the headers

Copy `include/jeson` into your project's include directory and add:

```cpp
#include <jeson/jeson.hpp>
```

This is the smallest and most portable route for custom toolchains and bare firmware builds.

### CMake / FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    jeson
    GIT_REPOSITORY https://github.com/mijelsma/jeson.git
    GIT_TAG 0.1.0
)
FetchContent_MakeAvailable(jeson)

target_link_libraries(your_target PRIVATE jeson::jeson)
```

For a checkout, `add_subdirectory(path/to/jeson)` exposes the same target. Installed packages support `find_package(jeson CONFIG REQUIRED)` followed by `jeson::jeson`.

### ESP-IDF

Use the repository as a component (for example under `components/jeson`) or declare it in `idf_component.yml`. The root `CMakeLists.txt` detects ESP-IDF and registers the public headers automatically.

### PlatformIO

```ini
lib_deps = https://github.com/mijelsma/jeson.git#0.1.0
build_unflags = -std=gnu++11
build_flags = -std=gnu++20
```

### Arduino

Install the repository as a ZIP or clone it into the sketchbook `libraries/JESON` directory. Select a board core/toolchain that supports C++20, then include `<jeson/jeson.hpp>`. Many older Arduino cores still default to C++11 and cannot compile JESON.

### Other build systems

The repository also ships `meson.build`, `BUILD.bazel`, `MODULE.bazel`, `conanfile.py`, `library.json`, `library.properties`, and Zephyr module metadata. Detailed snippets and platform notes are in [docs/integration.md](docs/integration.md).

## Model syntax

```cpp
enum class Mode { OFF, ECO, COMFORT };

struct Network {
    jeson::Field<"ssid", std::string> ssid;
    jeson::Field<"password", std::optional<std::string>> password;
};

struct Settings {
    jeson::Field<"device_id", jeson::Required<std::string>> deviceId;
    jeson::Field<"network", Network> network;
    jeson::Field<"mode", Mode> mode = Mode::ECO;
    jeson::Field<"sample_rate", jeson::Validator<unsigned,
        jeson::Minimum<1>, jeson::Maximum<3600>>> sampleRate = 60;
    jeson::Field<"channels", std::vector<int>> channels;
};
```

Parsing keeps defaults for omitted ordinary fields and reports all validation failures:

```cpp
Settings settings;
auto errors = jeson::parse(payload, settings);
if (!errors) {
    log(errors.toString());  // e.g. /sample_rate: must be <= 3600
    publish(errors.toJson());
    return;
}
```

Serialization is compact by default:

```cpp
jeson::toJson(settings);
jeson::toJson(settings, jeson::pretty);
jeson::toJson(settings, jeson::compact_omit_nulls);
jeson::toJson(settings, jeson::WriteOptions{true, 4, true});
```

See the focused programs in [`examples/`](examples) and the [feature guide](docs/features.md) for enums, patches, validation, writer options, and error handling. `examples/ArduinoBasic` is ready to open from an Arduino-compatible installation.

## Behavior worth knowing

- Unknown object keys are intentionally ignored.
- Ordinary omitted keys retain the destination object's current value; wrap a value in `Required<T>` when presence matters.
- Validators run on parsed/untrusted input. Direct C++ assignment is unchecked; call `Validator::set()` for checked in-process updates.
- Automatic enum discovery probes values `0..31`. Use `EnumNames<T>` for custom names, negative/sparse enums, or a wider probe range.
- The parser accepts UTF-8 JSON text and decodes `\uXXXX` escapes, including surrogate pairs.
- `std::string_view` is supported for writing. Parsing into it is deliberately unsupported because JESON does not own a stable backing buffer.

## Configuration

Set the reflection limit before the first JESON include:

```cpp
#define JESON_MAX_FIELDS 64
#include <jeson/jeson.hpp>
```

Then regenerate the checked-in structured-binding overloads with `python3 tools/generate_reflect.py 64` when maintaining a fork. The distributed headers contain overloads through 32 fields.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The top-level build includes all examples and tests. When JESON is consumed with `add_subdirectory` or `FetchContent`, those are disabled by default. Override with `JESON_BUILD_TESTS` or `JESON_BUILD_EXAMPLES`.

## License

JESON is available under the [MIT License](LICENSE).
