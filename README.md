# JESON

JESON is a small, header-only C++20 JSON library built first for constrained
firmware: ESP32, Nordic nRF/Zephyr, and similar devices where flash, RAM, heap
traffic, and integration effort all matter. It keeps the convenient typed
model style of [reflect-cpp](https://github.com/getml/reflect-cpp) and Python's
Pydantic without requiring a large reflection framework or a generic JSON tree
for normal configuration, command, and telemetry messages.

Describe the JSON name, C++ type, default, and validation rules in one place;
then parse, validate, use, and serialize the same ordinary C++ object:

```cpp
#include <jeson/jeson.hpp>

struct Device {
    jeson::Field<"id", jeson::Required<std::string>> id;
    jeson::Field<"port", jeson::Validator<int,
        jeson::Minimum<1>, jeson::Maximum<65535>>> port = 8080;
    jeson::Field<"enabled", bool> enabled = true;
};

Device device;
auto result = jeson::parse(R"({"id":"sensor-01","port":9000})", device);
if (result) {
    device.port = 9001;              // ordinary assignment; no .value()
    auto json = jeson::toJson(device, jeson::pretty);
}
```

The normal typed path has no registration macro, generated schema, runtime type
table, intermediate DOM, exception-based error path, or compiled library to
link.

## Which API should I use?

These are three different use cases, not fast and slow versions of the same
API:

| Your JSON | Use | What it does |
|---|---|---|
| The fields are known at compile time | **Typed API** (`Field`, structs, `parse(json, model)`) | Parses directly into your model. This is the normal and lowest-RAM firmware path. |
| The fields are not known and you need to edit the document | **Mutable DOM** (`jeson::Value`) | Builds a familiar, editable tree using strings, vectors, and variants. |
| The fields are not known and you only need to inspect, forward, or serialize the document | **Compact DOM** (`jeson::CompactDocument`) | Builds a read-only paged tree with fewer allocations on larger documents. |

In short: **known schema → typed API; unknown and editable → `Value`; unknown
and read-only → `CompactDocument`.** `Value` here means the generic JSON tree;
it is unrelated to assigning or reading a typed `Field`.

The compact-DOM work in 0.2.0 does not make typed parsing faster or smaller.
Typed parsing already bypassed both DOMs and is still the recommended path for
device configuration, commands, and telemetry. The typed benchmark was added
to show that separate path, not to claim a before/after typed improvement.

## Designed for constrained firmware

- **Small typed path:** JSON is parsed directly into the destination aggregate;
  an intermediate DOM and a second model-mapping pass are not required.
- **Low integration cost:** copy one header folder or use the supplied metadata
  for ESP-IDF, Zephyr, PlatformIO, Arduino, CMake, and desktop build systems.
- **Readable application code:** fields behave like their wrapped type, so
  `config.interval = 1000` and `if (config.enabled)` work without `.get()` or
  `.value()` boilerplate.
- **Schema where it is useful:** required keys, defaults, nested models, enums,
  constraints, optionals, vectors, and partial updates live next to the field.
- **Useful failure reports:** syntax, type, and validation errors are
  accumulated with JSON paths instead of disappearing behind a boolean result.
- **Controlled escape hatches:** use mutable `jeson::Value` for general dynamic
  work, or read-oriented `jeson::CompactDocument` when a generic document must
  fit into less RAM. Known-schema firmware should keep using the typed path.

JESON deliberately uses familiar standard C++ types rather than inventing an
allocator or container ecosystem. `std::string` and `std::vector` can allocate,
so exact RAM and flash usage still depends on the model, standard library,
compiler flags, and target toolchain. Measure the final firmware image on the
actual device.

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
- Optional insertion-ordered `jeson::Value` DOM for mutable schema-less JSON
- Optional paged `jeson::CompactDocument` DOM for low-allocation, read-oriented schema-less JSON

JESON parses directly into the destination object in one recursive pass.

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
    GIT_TAG 0.2.0
)
FetchContent_MakeAvailable(jeson)

target_link_libraries(your_target PRIVATE jeson::jeson)
```

For a checkout, `add_subdirectory(path/to/jeson)` exposes the same target. Installed packages support `find_package(jeson CONFIG REQUIRED)` followed by `jeson::jeson`.

### ESP-IDF

Use the repository as a component (for example under `components/jeson`) or declare it in `idf_component.yml`. The root `CMakeLists.txt` detects ESP-IDF and registers the public headers automatically.

### PlatformIO

```ini
lib_deps = https://github.com/mijelsma/jeson.git#0.2.0
build_unflags = -std=gnu++11
build_flags = -std=gnu++20
```

### Arduino

Install the repository as a ZIP or clone it into the sketchbook `libraries/JESON` directory. Select a board core/toolchain that supports C++20, then include `<jeson/jeson.hpp>`. Many older Arduino cores still default to C++11 and cannot compile JESON.

### Other build systems

The repository also ships `meson.build`, `BUILD.bazel`, `MODULE.bazel`, `conanfile.py`, `library.json`, `library.properties`, and Zephyr module metadata. Detailed snippets and platform notes are in [docs/integration.md](docs/integration.md).

## Model syntax

The model is the mapping. `Field<"device_id", T>` connects the wire name to the
C++ member, while wrappers such as `Required<T>` and `Validator<T, ...>` add
input rules. There is no second registration list to keep synchronized and no
handwritten `json["device_id"]` conversion function.

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

Once parsed, these are normal members with normal nested access:

```cpp
Settings settings;
auto result = jeson::parse(payload, settings);

if (result) {
    settings.sampleRate = 300;
    settings.network.ssid = "workshop";
    send(jeson::toJson(settings, jeson::compact_omit_nulls));
} else {
    log(result.toString());
}
```

Omitted ordinary fields retain their C++ defaults, `Required<T>` makes presence
mandatory, and unknown input keys are ignored for forward-compatible firmware.
The same model is used for serialization, so field names and enum conversion do
not have to be implemented twice.

## How the API compares

These libraries solve overlapping but different problems. JESON is optimized
for known-schema firmware messages; it is not intended to replace every DOM or
streaming API.

| Library | Typical programming model | Difference from JESON |
|---|---|---|
| **JESON** | Declare a typed aggregate and parse directly into it; optional mutable and compact DOMs | Validation, defaults, mapping, and serialization share one compact model. Requires C++20; chosen standard containers can allocate. |
| **reflect-cpp** | Broad compile-time reflection and serialization across JSON and other formats | Closest high-level style and much broader in scope. JESON intentionally offers a smaller JSON-only surface for firmware. |
| **ArduinoJson** | Embedded-focused `JsonDocument`/variant API | Excellent dynamic document API and allocator control. Typed schema rules and model validation normally remain application code. |
| **RapidJSON** | Fast DOM, SAX, and writer APIs with allocator control | More low-level control and streaming options. Mapping a document into a typed application model is generally manual. |
| **nlohmann/json** | Expressive STL-like dynamic JSON value | Very convenient general-purpose DOM and large ecosystem. JESON avoids making that generic tree the normal typed path. |
| **cJSON** | Portable C DOM made of nodes and pointers | Useful from C and easy to port. Typed conversion, validation, ownership, and cleanup are explicit application responsibilities. |

The [JESON nativejson benchmark](https://github.com/mijelsma/nativejson-benchmark)
measures a reproducibly pinned version of all six libraries. Its binary-size,
speed, memory, and conformance results use comparable schema-less DOM adapters;
they do not measure JESON's direct-to-model advantage or prove one library is
best for every workload.

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

For documents whose schema is not known at compile time and must be mutated,
use the **mutable DOM**, whose C++ type is `jeson::Value`:

```cpp
jeson::Value document;
auto errors = jeson::parse(R"({"device":"sensor-01","samples":[21.5,22.0]})", document);
if (errors) {
    auto compact = jeson::toJson(document);
    auto formatted = jeson::toJson(document, jeson::pretty);
}
```

`Value::Object` is an insertion-ordered vector of key/value pairs. The typed API remains the recommended embedded path because it avoids constructing this generic tree.

When the document is schema-less but read-only or read-mostly, the **compact
DOM**, whose owning C++ type is `jeson::CompactDocument`, stores
24-byte nodes and strings in pages instead of allocating an STL container at
every level:

```cpp
jeson::CompactDocument document;
auto errors = jeson::parse(payload, document);
if (errors) {
    auto root = document.root();
    log(root["device"].stringValue());
    for (auto sample : root["samples"].children()) {
        consume(sample.doubleValue());
    }
    send(jeson::toJson(document));
}
```

`CompactValue` is a small read-only view; the document owns all decoded text,
so the input buffer may be released after parsing. Views are invalidated by
clearing, re-parsing, moving, or destroying their document. The default page
sizes favor embedded messages and can be tuned before including JESON with
`JESON_COMPACT_NODE_PAGE_NODES` and `JESON_COMPACT_STRING_PAGE_BYTES`.

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

## Benchmarks

The reproducible cross-library benchmark lives in the JESON-maintained fork of [nativejson-benchmark](https://github.com/mijelsma/nativejson-benchmark). It compares JESON with cJSON, reflect-cpp, RapidJSON, nlohmann/json, and ArduinoJson for parsing, stringify, pretty-print, parse allocations/peak memory, conformance, and per-adapter binary size.

The DOM lane measures the mutable DOM (`Value`) and compact DOM
(`CompactDocument`). These apply only to unknown-schema documents. A separate
application-model lane measures the unchanged typed API and compares JESON and
reflect-cpp direct typed parsing with the DOM-plus-manual-mapping path required
by the other pinned adapters. Exact dependency commits, compiler flags,
commands, and result interpretation are recorded in the benchmark repository
rather than embedding machine-specific numbers here.

## License

JESON is available under the [MIT License](LICENSE).
