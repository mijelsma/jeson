# Feature guide

## Fields and aggregate reflection

Every serializable member is a `jeson::Field<"wire_name", T>`. JESON discovers aggregate members in declaration order, so there is no second registration list to maintain.

Leaf fields implicitly convert to and assign from their value type. A field wrapping another reflected struct publicly derives from that struct, which keeps nested access natural:

```cpp
struct WiFi {
    jeson::Field<"ssid", std::string> ssid;
};

struct Config {
    jeson::Field<"wifi", WiFi> wifi;
};

Config config;
config.wifi.ssid = "lab";
```

Model types must be C++ aggregates with public members and no user-declared constructors or virtual functions.

## Required values and validation

`Required<T>` checks whether the JSON key exists. It does not decide whether the value is semantically non-empty. Compose it with `Validator` for that:

```cpp
jeson::Field<"name",
    jeson::Required<jeson::Validator<std::string, jeson::MinSize<1>>>> name;
```

Available constraints are inclusive `Minimum<N>` and `Maximum<N>`, plus `MinSize<N>` and `MaxSize<N>` for strings and containers. `Validator::set()` can apply the same checks to an in-process update.

## Enums

Sequential enum values in the default `0..31` probe range are written and parsed by their C++ spelling automatically. Wire protocols often benefit from an explicit stable mapping:

```cpp
enum class Mode { Off, On };

template <>
struct jeson::EnumNames<Mode> {
    static constexpr bool has_mapping = true;
    static constexpr std::pair<Mode, std::string_view> table[] = {
        {Mode::Off, "off"},
        {Mode::On, "on"},
    };
};
```

To retain automatic names but widen the positive range:

```cpp
template <>
struct jeson::EnumNames<ErrorCode>
    : jeson::internal::MagicEnumNames<ErrorCode, 256> {};
```

Use an explicit table for negative values, sparse ranges that would be expensive to probe, aliases, and custom spellings.

## Optional values and vectors

`std::optional<T>` accepts a value or JSON `null`. An absent ordinary optional stays unchanged. Serialization writes an unset optional as `null` unless `compact_omit_nulls` or an equivalent `WriteOptions` value is used.

`std::vector<T>` supports every other supported value type, including nested model structs. Parsing replaces the vector contents.

## Patches

`Patch<T>` distinguishes omitted root sections from supplied sections while reusing the ordinary settings schema. Direct members of `T` must be optional sections. A supplied section is complete by default; mark a section type as partial to retain its baseline siblings:

```cpp
struct Network {
    static constexpr jeson::PatchSectionPolicy jesonPatchPolicy =
        jeson::PatchSectionPolicy::PARTIAL;

    jeson::Field<"ssid", std::string> ssid;
    jeson::Field<"enabled", bool> enabled = true;
};
```

Always construct a patch with a baseline object before parsing. This prevents omitted fields from silently falling back to unrelated defaults. See `examples/patch_example.cpp` and the patch tests for complete compositions.

## Errors

`jeson::parse` returns `ParseErrors`. It accumulates independent field errors and records JSON-pointer-like paths:

```cpp
auto errors = jeson::parse(payload, config);
for (const auto& error : errors.list()) {
    // error.path, error.message
}
```

`toString()` is intended for logs. `toJson()` produces a stable API-friendly object with `ok` and `errors` keys.

## Writer options

`jeson::toJson(value)` emits compact JSON. Pass `jeson::pretty`, `jeson::compact_omit_nulls`, or a custom `WriteOptions{pretty, indentBy, omitNulls}`.

## Schema-less documents

The schema-less APIs are escape hatches for JSON whose keys and shape are not
known when the program is compiled. They are separate from the typed API:

| API | Mutable? | Best fit |
|---|---:|---|
| Typed aggregate (`Field`, structs) | Through normal C++ members | Known device configuration, commands, and telemetry; no DOM is built. |
| Mutable DOM (`jeson::Value`) | Yes | Unknown JSON that must be created or edited. |
| Compact DOM (`jeson::CompactDocument`) | No | Unknown JSON that must be inspected or selectively reserialized with lower allocation overhead on larger documents. |

`jeson::Value` is the C++ type behind the mutable DOM. Objects retain insertion
order and use familiar `std::variant`, `std::vector`, and `std::string` storage.
Its name does not refer to a typed field value.

`jeson::CompactDocument` owns the compact DOM. It is the read-oriented option
for constrained devices. It owns decoded keys and strings, stores JSON values
in fixed-size pages, and exposes lightweight `CompactValue` handles:

```cpp
jeson::CompactDocument document;
if (jeson::parse(payload, document)) {
    auto root = document.root();
    auto name = root["name"].stringValue();
    for (auto item : root["items"].children()) {
        use(item);
    }
}
```

This is mainly useful in gateways, protocol bridges, and diagnostics that must
traverse messages whose shape is not known at compile time. Do not use it for a
known schema (use a typed aggregate), for editing (use `Value`), or for blind
pass-through (forward the original input without parsing it).

Missing keys and out-of-range indexes return an invalid handle. Scalar accessors
accept a fallback value. `children()` iterates arrays and object members; for an
object child, `key()` returns the member name. The document can serialize itself
or any valid subtree without first creating a `Value` tree.

The default 64-node pages consume 1,536 bytes and ordinary string pages consume
1,024 bytes. Define `JESON_COMPACT_NODE_PAGE_NODES` or
`JESON_COMPACT_STRING_PAGE_BYTES` before the first JESON include to tune this
granularity. Smaller pages reduce slack for tiny documents; larger pages reduce
allocation traffic for very large documents.

Because the first pages have a fixed cost, the compact DOM is not guaranteed to
use less memory for tiny documents. Its benefit is lower per-node overhead and
far fewer allocations as generic documents grow.

Compact views are invalidated when their document is cleared, parsed again,
moved, or destroyed. The compact DOM intentionally does not provide mutation or
the typed API's path-specific validation diagnostics; choose `Value` or a typed
aggregate when those features matter more than generic parse RAM.

The compact DOM is not used internally by typed parsing. The 0.2.0 compact-DOM
optimization therefore changes schema-less parsing only; the typed API and its
performance are unchanged.
