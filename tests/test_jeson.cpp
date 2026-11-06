/**
 * @file test_jeson.cpp
 * @brief Full jeson test suite, one file -> one binary (cest generates its
 * own main() per translation unit, so multi-file linking isn't possible -
 * see the earlier note in project history for why this is one file).
 *
 * Every struct/field name here is deliberately generic (Item, Container,
 * Reading, Entry, Category...) - none of this is tied to any real product
 * domain, it's purely to exercise jeson's features.
 *
 * Coverage is organized by header, and within each header by feature. Every
 * feature gets both a true-positive test (valid input works) and a
 * true-negative test (invalid input is correctly rejected with a sensible
 * error), not just happy-path coverage.
 *
 *   Run it all:          ./jeson_tests
 *   Sectioned tree view: ./jeson_tests -t
 *   Run a subset:        ./jeson_tests -g "<substring of an it(...) name>"
 *   List every test:     ./jeson_tests --list-tests
 */
#include <cest.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "jeson/jeson.hpp"

namespace rfl = jeson;

// =========================================================================
// Shared generic fixtures
// =========================================================================
namespace fixtures {

struct Leaf {
    rfl::Field<"name", std::string> name;
    rfl::Field<"enabled", bool> enabled = true;
};

struct Container {
    rfl::Field<"leaf", Leaf> leaf;
    rfl::Field<"count", int> count = 0;
};

struct DeepA { rfl::Field<"value", int> value = 0; };
struct DeepB { rfl::Field<"a", DeepA> a; rfl::Field<"label", std::string> label; };
struct DeepC { rfl::Field<"b", DeepB> b; };

struct Reading {
    rfl::Field<"value_a", rfl::Validator<uint16_t, rfl::Minimum<10>, rfl::Maximum<100>>> valueA = 50;
    rfl::Field<"value_b", rfl::Validator<int16_t, rfl::Minimum<-5>, rfl::Maximum<5>>> valueB     = 0;
    rfl::Field<"label", rfl::Validator<std::string, rfl::MinSize<2>, rfl::MaxSize<6>>> label      = "abc";
};

struct Entry {
    rfl::Field<"id", rfl::Required<std::string>> id;
    rfl::Field<"note", std::string> note;
};

struct EntryValidated {
    rfl::Field<"id", rfl::Required<rfl::Validator<std::string, rfl::MinSize<1>>>> id;
};

enum class Category : uint8_t { A, B, C, D };

enum class Status : uint8_t { PENDING, ACTIVE, DONE };

struct Tagged { rfl::Field<"category", Category> category = Category::A; };
struct Labeled { rfl::Field<"status", Status> status = Status::PENDING; };

struct Primitives {
    rfl::Field<"flag", bool> flag           = false;
    rfl::Field<"count", int32_t> count       = 0;
    rfl::Field<"small", uint16_t> small       = 0;
    rfl::Field<"fraction", double> fraction   = 0.0;
    rfl::Field<"text", std::string> text;
};

struct StringViews {
    rfl::Field<"type", std::string_view> type = "thermostat";
    rfl::Field<"mode", std::optional<std::string_view>> mode;
};

struct Point { rfl::Field<"x", int> x; rfl::Field<"y", int> y; };

struct WideTelemetry {
    rfl::Field<"f00", int> f00; rfl::Field<"f01", int> f01;
    rfl::Field<"f02", int> f02; rfl::Field<"f03", int> f03;
    rfl::Field<"f04", int> f04; rfl::Field<"f05", int> f05;
    rfl::Field<"f06", int> f06; rfl::Field<"f07", int> f07;
    rfl::Field<"f08", int> f08; rfl::Field<"f09", int> f09;
    rfl::Field<"f10", int> f10; rfl::Field<"f11", int> f11;
    rfl::Field<"f12", int> f12; rfl::Field<"f13", int> f13;
    rfl::Field<"f14", int> f14; rfl::Field<"f15", int> f15;
    rfl::Field<"f16", int> f16; rfl::Field<"f17", int> f17;
    rfl::Field<"f18", int> f18; rfl::Field<"f19", int> f19;
    rfl::Field<"f20", int> f20; rfl::Field<"f21", int> f21;
    rfl::Field<"f22", int> f22; rfl::Field<"f23", int> f23;
    rfl::Field<"f24", int> f24; rfl::Field<"f25", int> f25;
    rfl::Field<"f26", int> f26; rfl::Field<"f27", int> f27;
};

struct Collection {
    rfl::Field<"numbers", std::vector<int>> numbers;
    rfl::Field<"labels", std::vector<std::string>> labels;
    rfl::Field<"points", std::vector<Point>> points;
    rfl::Field<"note", std::optional<std::string>> note;
    rfl::Field<"rank", std::optional<int>> rank;
};

struct IndependentSection {
    static constexpr rfl::PatchSectionPolicy jesonPatchPolicy = rfl::PatchSectionPolicy::PARTIAL;

    rfl::Field<"name", std::string> name  = "original";
    rfl::Field<"enabled", bool> enabled    = true;
    rfl::Field<"count", int> count         = 7;
    rfl::Field<"label", std::string> label = "initial";
};

struct PatchSettings {
    rfl::Field<"independent", std::optional<IndependentSection>> independent = IndependentSection{};
    rfl::Field<"reading", std::optional<Reading>> reading                    = Reading{};
};

struct PatchEnvelope {
    rfl::Field<"payload", rfl::Required<rfl::Patch<PatchSettings>>> payload = rfl::Required<rfl::Patch<PatchSettings>>{
        rfl::Patch<PatchSettings>{PatchSettings{}}
    };
};

} // namespace fixtures

template <>
struct jeson::EnumNames<fixtures::Status> {
    static constexpr bool has_mapping = true;
    static constexpr std::pair<fixtures::Status, std::string_view> table[] = {
        {fixtures::Status::PENDING, "PENDING"},
        {fixtures::Status::ACTIVE, "ACTIVE"},
        {fixtures::Status::DONE, "DONE"},
    };
};

// =========================================================================
// field.hpp / reflect.hpp - no-macro discovery, transparent nested access
// =========================================================================
describe("struct member discovery requires no registration macro", []() {
    using namespace fixtures;

    it("[positive] parses a nested struct with no JESON_STRUCT/JESON_MEMBER of any kind", []() {
        Container c;
        auto errors = jeson::parse(R"({"leaf":{"name":"x","enabled":false},"count":5})", c);
        expect(errors.ok()).toBe(true);
        expect(c.count == 5).toBe(true);
    });

    it("[positive] gives direct nested dot access with zero .value() calls, at 2 levels deep", []() {
        Container c;
        c.leaf.name    = "widget";
        c.leaf.enabled = false;
        c.count         = 7;
        expect(c.leaf.name == std::string("widget")).toBe(true);
        expect((bool)c.leaf.enabled).toBe(false);
        expect((int)c.count).toBe(7);
    });

    it("[positive] transparent access still works at 3 levels of nesting", []() {
        DeepC d;
        d.b.a.value = 42;
        d.b.label    = "deep";
        expect((int)d.b.a.value).toBe(42);
        expect(d.b.label == std::string("deep")).toBe(true);
    });

    it("[positive] parses correctly through 3 levels of nesting from JSON", []() {
        DeepC d;
        auto errors = jeson::parse(R"({"b":{"a":{"value":9},"label":"z"}})", d);
        expect(errors.ok()).toBe(true);
        expect((int)d.b.a.value).toBe(9);
    });

    it("[negative] a type mismatch 3 levels deep is pathed correctly and does not crash", []() {
        DeepC d;
        auto errors = jeson::parse(R"({"b":{"a":{"value":"not-a-number"},"label":"z"}})", d);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].path).toBe("/b/a/value");
    });

    it("[positive] round-trips parse -> write -> parse with plain access throughout", []() {
        Container original;
        original.leaf.name    = "roundtrip";
        original.leaf.enabled = true;
        original.count         = 3;

        std::string json = jeson::toJson(original);
        Container restored;
        auto errors = jeson::parse(json, restored);

        expect(errors.ok()).toBe(true);
        expect(std::string(restored.leaf.name) == std::string(original.leaf.name)).toBe(true);
        expect((int)restored.count).toBe(3);
    });

    it("[positive] whole-struct assignment works via the implicit converting constructor", []() {
        Container c;
        c.leaf = Leaf{"assigned", false};
        expect(c.leaf.name == std::string("assigned")).toBe(true);
        expect((bool)c.leaf.enabled).toBe(false);
    });

    it("[positive] reflects and serializes aggregates wider than 25 fields", []() {
        WideTelemetry telemetry;
        telemetry.f00 = 1;
        telemetry.f27 = 28;

        expect(jeson::internal::count_fields<WideTelemetry>()).toBe(28U);
        const std::string json = jeson::toJson(telemetry);
        expect(json.find(R"("f00":1)") != std::string::npos).toBe(true);
        expect(json.find(R"("f27":28)") != std::string::npos).toBe(true);
    });
});

// =========================================================================
// required.hpp - mandatory fields
// =========================================================================
describe("Required<T> marks a field as mandatory", []() {
    using namespace fixtures;

    it("[negative] fails with a clear message when the key is entirely absent", []() {
        Entry e;
        auto errors = jeson::parse(R"({})", e);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].path).toBe("/id");
        expect(errors.list()[0].message).toBe("missing required field");
    });

    it("[positive] succeeds when the key is present", []() {
        Entry e;
        auto errors = jeson::parse(R"({"id":"abc"})", e);
        expect(errors.ok()).toBe(true);
        expect(e.id.value.value == std::string("abc")).toBe(true);
    });

    it("[positive] does not flag ordinary (non-Required) sibling fields as missing", []() {
        Entry e;
        auto errors = jeson::parse(R"({"id":"abc"})", e);
        expect(errors.ok()).toBe(true);
        expect(e.note == std::string("")).toBe(true);
    });

    it("[positive] an empty string still counts as present - Required only checks key presence", []() {
        Entry e;
        auto errors = jeson::parse(R"({"id":""})", e);
        expect(errors.ok()).toBe(true);
    });

    it("[negative] combined with Validator: missing key still fails on 'missing', not on size", []() {
        EntryValidated e;
        auto errors = jeson::parse(R"({})", e);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("missing required field");
    });

    it("[negative] combined with Validator: present-but-empty fails the size check, not 'missing'", []() {
        EntryValidated e;
        auto errors = jeson::parse(R"({"id":""})", e);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("size must be >= 1");
    });

    it("[positive] combined with Validator: present and valid succeeds", []() {
        EntryValidated e;
        auto errors = jeson::parse(R"({"id":"ok"})", e);
        expect(errors.ok()).toBe(true);
    });
});

// =========================================================================
// patch.hpp - partial root with per-section completeness policies
// =========================================================================
describe("Patch<T> selects sections without duplicating their schema", []() {
    using namespace fixtures;

    it("[positive] keeps omitted root sections unset", []() {
        rfl::Patch<PatchSettings> patch{PatchSettings{}};
        auto errors = jeson::parse(R"({"independent":{"name":"updated"}})", patch);

        expect(errors.ok()).toBe(true);
        expect(patch->independent.value.has_value()).toBe(true);
        expect(patch->reading.value.has_value()).toBe(false);
    });

    it("[positive] updates one field in a partial section and retains its baseline siblings", []() {
        PatchSettings baseline;
        baseline.independent.value->enabled = false;
        baseline.independent.value->count   = 42;
        baseline.independent.value->label   = "preserved";
        rfl::Patch<PatchSettings> patch{baseline};

        auto errors = jeson::parse(R"({"independent":{"name":"updated"}})", patch);

        expect(errors.ok()).toBe(true);
        expect(patch->independent.value->name == std::string("updated")).toBe(true);
        expect((bool)patch->independent.value->enabled).toBe(false);
        expect((int)patch->independent.value->count).toBe(42);
        expect(patch->independent.value->label == std::string("preserved")).toBe(true);
    });

    it("[positive] allows every field in a partial section to be set independently", []() {
        PatchSettings baseline;

        rfl::Patch<PatchSettings> enabledPatch{baseline};
        expect(jeson::parse(R"({"independent":{"enabled":false}})", enabledPatch).ok()).toBe(true);
        expect((bool)enabledPatch->independent.value->enabled).toBe(false);
        expect(enabledPatch->independent.value->name == std::string("original")).toBe(true);

        rfl::Patch<PatchSettings> countPatch{baseline};
        expect(jeson::parse(R"({"independent":{"count":99}})", countPatch).ok()).toBe(true);
        expect((int)countPatch->independent.value->count).toBe(99);
        expect(countPatch->independent.value->label == std::string("initial")).toBe(true);

        rfl::Patch<PatchSettings> labelPatch{baseline};
        expect(jeson::parse(R"({"independent":{"label":"changed"}})", labelPatch).ok()).toBe(true);
        expect(labelPatch->independent.value->label == std::string("changed")).toBe(true);
        expect((int)labelPatch->independent.value->count).toBe(7);
    });

    it("[negative] still rejects missing fields in a complete section", []() {
        rfl::Patch<PatchSettings> patch{PatchSettings{}};
        auto errors = jeson::parse(R"({"reading":{"value_a":60}})", patch);

        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].path).toBe("/reading/value_b");
        expect(errors.list()[0].message).toBe("missing required field");
    });

    it("[positive] accepts mixed partial and complete sections in one patch", []() {
        rfl::Patch<PatchSettings> patch{PatchSettings{}};
        auto errors = jeson::parse(R"({"independent":{"enabled":false},"reading":{"value_a":60,"value_b":2,"label":"valid"}})", patch);

        expect(errors.ok()).toBe(true);
        expect(patch->independent.value.has_value()).toBe(true);
        expect(patch->reading.value.has_value()).toBe(true);
    });

    it("[negative] rejects an explicitly null section", []() {
        rfl::Patch<PatchSettings> patch{PatchSettings{}};
        auto errors = jeson::parse(R"({"independent":null})", patch);

        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].path).toBe("/independent");
        expect(errors.list()[0].message).toBe("expected complete object, got null");
    });

    it("[negative] requires a baseline so partial fields cannot fall back to defaults accidentally", []() {
        rfl::Patch<PatchSettings> patch;
        auto errors = jeson::parse(R"({"independent":{"name":"unsafe"}})", patch);

        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("patch requires a baseline object");
    });

    it("[positive] composes with Required<T> in a command envelope", []() {
        PatchEnvelope envelope;
        auto errors = jeson::parse(R"({"payload":{"independent":{"name":"command"}}})", envelope);

        expect(errors.ok()).toBe(true);
        expect(envelope.payload.value.value->independent.value.has_value()).toBe(true);
        expect(envelope.payload.value.value->reading.value.has_value()).toBe(false);
    });
});

// =========================================================================
// validator.hpp - Minimum / Maximum / MinSize / MaxSize
// =========================================================================
describe("Minimum / Maximum numeric constraints", []() {
    using namespace fixtures;

    it("[positive] accepts a value exactly at the minimum boundary", []() {
        Reading r;
        expect(jeson::parse(R"({"value_a":10})", r).ok()).toBe(true);
    });

    it("[positive] accepts a value exactly at the maximum boundary", []() {
        Reading r;
        expect(jeson::parse(R"({"value_a":100})", r).ok()).toBe(true);
    });

    it("[negative] rejects a value one below the minimum", []() {
        Reading r;
        auto errors = jeson::parse(R"({"value_a":9})", r);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("must be >= 10");
    });

    it("[negative] rejects a value one above the maximum", []() {
        Reading r;
        auto errors = jeson::parse(R"({"value_a":101})", r);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("must be <= 100");
    });

    it("[positive] a signed field accepts its full negative..positive range", []() {
        Reading r;
        expect(jeson::parse(R"({"value_b":-5})", r).ok()).toBe(true);
        expect(jeson::parse(R"({"value_b":5})", r).ok()).toBe(true);
        expect(jeson::parse(R"({"value_b":0})", r).ok()).toBe(true);
    });

    it("[negative] a signed field rejects values outside its range on both sides", []() {
        Reading r;
        expect(jeson::parse(R"({"value_b":-6})", r).ok()).toBe(false);
        expect(jeson::parse(R"({"value_b":6})", r).ok()).toBe(false);
    });

    it("[negative] leaves the underlying value at its previous value when validation fails", []() {
        Reading r;
        jeson::parse(R"({"value_a":999})", r);
        expect(r.valueA.value.value() == 50).toBe(true); // default, rejected value never applied
    });

    it("[positive] a direct C++ assignment applies unchecked, via the implicit conversion chain", []() {
        Reading r;
        r.valueA = 75; // no .set(), no intermediate Validator{} needed
        expect(r.valueA.value.value() == 75).toBe(true);
    });
});

describe("MinSize / MaxSize string-length constraints", []() {
    using namespace fixtures;

    it("[positive] accepts a string exactly at MinSize", []() {
        Reading r;
        expect(jeson::parse(R"({"label":"ab"})", r).ok()).toBe(true);
    });

    it("[positive] accepts a string exactly at MaxSize", []() {
        Reading r;
        expect(jeson::parse(R"({"label":"abcdef"})", r).ok()).toBe(true);
    });

    it("[positive] accepts a string strictly within the range", []() {
        Reading r;
        auto errors = jeson::parse(R"({"label":"mid"})", r);
        expect(errors.ok()).toBe(true);
        expect(r.label.value.value() == std::string("mid")).toBe(true);
    });

    it("[negative] rejects a string shorter than MinSize", []() {
        Reading r;
        auto errors = jeson::parse(R"({"label":"a"})", r);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("size must be >= 2");
    });

    it("[negative] rejects a string longer than MaxSize", []() {
        Reading r;
        auto errors = jeson::parse(R"({"label":"way-too-long"})", r);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("size must be <= 6");
    });
});

describe("Validator implicitly converts to its underlying value", []() {
    using namespace fixtures;

    it("[positive] can be compared directly without an explicit .value() call", []() {
        Reading r;
        jeson::parse(R"({"value_a":33})", r);
        const uint16_t& asRaw = r.valueA.value;
        expect(asRaw == 33).toBe(true);
    });
});

// =========================================================================
// enum.hpp - every plain enum, zero setup, always serializes as its name
// =========================================================================
describe("a plain enum, with zero setup, serializes as its own name automatically", []() {
    using namespace fixtures;

    it("[positive] writes the enumerator's own spelling, not a number", []() {
        Tagged t;
        t.category         = Category::C;
        std::string json = jeson::toJson(t);
        expect(json).toBe(std::string(R"({"category":"C"})"));
    });

    it("[positive] parses the name back into the enum", []() {
        Tagged t;
        auto errors = jeson::parse(R"({"category":"D"})", t);
        expect(errors.ok()).toBe(true);
        expect(t.category == Category::D).toBe(true);
    });

    it("[positive] round-trips through write then parse", []() {
        Tagged original;
        original.category = Category::B;
        std::string json = jeson::toJson(original);
        Tagged restored;
        auto errors = jeson::parse(json, restored);
        expect(errors.ok()).toBe(true);
        expect(restored.category == Category::B).toBe(true);
    });

    it("[negative] rejects a bare integer - no annotation means no int fallback anymore", []() {
        Tagged t;
        auto errors = jeson::parse(R"({"category":2})", t);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("expected string (enum)");
    });

    it("[negative] rejects a name that isn't a real enumerator", []() {
        Tagged t;
        auto errors = jeson::parse(R"({"category":"Z"})", t);
        expect(errors.ok()).toBe(false);
    });
});

describe("jeson::EnumNames<T> overrides the automatic default with custom names", []() {
    using namespace fixtures;

    it("[positive] writes the registered name, not the enumerator's own spelling", []() {
        Labeled l;
        l.status           = Status::ACTIVE;
        std::string json = jeson::toJson(l);
        expect(json).toBe(std::string(R"({"status":"ACTIVE"})"));
    });

    it("[positive] parses the registered name back into the enum", []() {
        Labeled l;
        auto errors = jeson::parse(R"({"status":"DONE"})", l);
        expect(errors.ok()).toBe(true);
        expect(l.status == Status::DONE).toBe(true);
    });

    it("[positive] round-trips through write then parse", []() {
        Labeled original;
        original.status = Status::PENDING;
        std::string json = jeson::toJson(original);
        Labeled restored;
        auto errors = jeson::parse(json, restored);
        expect(errors.ok()).toBe(true);
        expect(restored.status == Status::PENDING).toBe(true);
    });

    it("[negative] rejects a name that isn't in the registered table", []() {
        Labeled l;
        auto errors = jeson::parse(R"({"status":"UNKNOWN_VALUE"})", l);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message.find("UNKNOWN_VALUE") != std::string::npos).toBe(true);
    });

    it("[negative] is case-sensitive - a differently-cased name is rejected", []() {
        Labeled l;
        auto errors = jeson::parse(R"({"status":"active"})", l);
        expect(errors.ok()).toBe(false);
    });

    it("[negative] rejects a bare integer", []() {
        Labeled l;
        auto errors = jeson::parse(R"({"status":1})", l);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("expected string (enum)");
    });

    it("[negative] leaves the field at its previous value when parsing fails", []() {
        Labeled l;
        l.status = Status::DONE;
        jeson::parse(R"({"status":"NOPE"})", l);
        expect(l.status == Status::DONE).toBe(true);
    });
});

// =========================================================================
// parser.hpp - primitive leaf values (bool / int / float / string)
// =========================================================================
describe("parsing boolean values", []() {
    using namespace fixtures;

    it("[positive] accepts true", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"flag":true})", p);
        expect(errors.ok()).toBe(true);
        expect((bool)p.flag).toBe(true);
    });

    it("[positive] accepts false", []() {
        Primitives p;
        p.flag = true;
        auto errors = jeson::parse(R"({"flag":false})", p);
        expect(errors.ok()).toBe(true);
        expect((bool)p.flag).toBe(false);
    });

    it("[negative] rejects a quoted \"true\" string as a boolean", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"flag":"true"})", p);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("expected boolean");
    });

    it("[negative] rejects a bare number as a boolean", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"flag":1})", p);
        expect(errors.ok()).toBe(false);
    });
});

describe("parsing integers", []() {
    using namespace fixtures;

    it("[positive] accepts a positive value", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"count":42})", p);
        expect(errors.ok()).toBe(true);
        expect((int)p.count).toBe(42);
    });

    it("[positive] accepts a negative value on a signed field", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"count":-42})", p);
        expect(errors.ok()).toBe(true);
        expect((int)p.count).toBe(-42);
    });

    it("[positive] accepts zero", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"small":0})", p);
        expect(errors.ok()).toBe(true);
        expect((int)p.small).toBe(0);
    });

    it("[negative] rejects a string where a number is expected", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"count":"42"})", p);
        expect(errors.ok()).toBe(false);
        expect(errors.list().size()).toBe(1u);
        expect(errors.list()[0].message).toBe("expected number");
    });

    it("[negative] rejects an integer outside the destination type's range", []() {
        Primitives p;
        p.small = 12;
        auto errors = jeson::parse(R"({"small":70000})", p);
        expect(errors.ok()).toBe(false);
        expect((int)p.small).toBe(12);
    });

    it("[negative] rejects a negative number for an unsigned destination", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"small":-1})", p);
        expect(errors.ok()).toBe(false);
    });

    it("[negative] rejects a fractional number for an integer destination", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"count":1.5})", p);
        expect(errors.ok()).toBe(false);
    });

    it("[negative] a numeric type mismatch doesn't cascade into unrelated document-level errors", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"count":"42","text":"still readable"})", p);
        expect(errors.ok()).toBe(false);
        expect(errors.list().size()).toBe(1u);
        expect(p.text == std::string("still readable")).toBe(true);
    });
});

describe("parsing floating point numbers", []() {
    using namespace fixtures;

    it("[positive] accepts a decimal value", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"fraction":3.5})", p);
        expect(errors.ok()).toBe(true);
        expect((double)p.fraction == 3.5).toBe(true);
    });

    it("[positive] accepts an integer-looking value into a double field", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"fraction":4})", p);
        expect(errors.ok()).toBe(true);
        expect((double)p.fraction == 4.0).toBe(true);
    });

    it("[positive] accepts negative values and exponent notation", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"fraction":-1.5e2})", p);
        expect(errors.ok()).toBe(true);
        expect((double)p.fraction == -150.0).toBe(true);
    });

    it("[negative] rejects number spellings outside the JSON grammar", []() {
        Primitives p;
        expect(jeson::parse(R"({"fraction":01})", p).ok()).toBe(false);
        expect(jeson::parse(R"({"fraction":+1})", p).ok()).toBe(false);
        expect(jeson::parse(R"({"fraction":1e})", p).ok()).toBe(false);
    });

    it("[negative] rejects floating-point overflow", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"fraction":1e999})", p);
        expect(errors.ok()).toBe(false);
    });
});

describe("parsing strings", []() {
    using namespace fixtures;

    it("[positive] accepts a plain string", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"text":"hello"})", p);
        expect(errors.ok()).toBe(true);
        expect(p.text == std::string("hello")).toBe(true);
    });

    it("[positive] accepts an empty string", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"text":""})", p);
        expect(errors.ok()).toBe(true);
        expect(p.text == std::string("")).toBe(true);
    });

    it("[positive] decodes common escape sequences", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"text":"line1\nline2\ttabbed\b\f\r"})", p);
        expect(errors.ok()).toBe(true);
        expect(p.text == std::string("line1\nline2\ttabbed\b\f\r")).toBe(true);
    });

    it("[positive] decodes an escaped quote and backslash", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"text":"say \"hi\" \\ ok"})", p);
        expect(errors.ok()).toBe(true);
        expect(p.text == std::string("say \"hi\" \\ ok")).toBe(true);
    });

    it("[positive] decodes BMP Unicode escapes and surrogate pairs to UTF-8", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"text":"\u20ac \ud83d\ude80"})", p);
        expect(errors.ok()).toBe(true);
        expect(p.text == std::string("\xe2\x82\xac \xf0\x9f\x9a\x80")).toBe(true);
    });

    it("[negative] rejects malformed Unicode escapes", []() {
        Primitives p;
        expect(jeson::parse(R"({"text":"\u12xz"})", p).ok()).toBe(false);
        expect(jeson::parse(R"({"text":"\ud83d"})", p).ok()).toBe(false);
    });

    it("[negative] rejects a number where a string is expected", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"text":123})", p);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("expected string");
    });
});

describe("whitespace tolerance", []() {
    using namespace fixtures;

    it("[positive] accepts arbitrary whitespace between tokens", []() {
        Primitives p;
        auto errors = jeson::parse("{ \"flag\" : true ,\n \"text\"\t: \"ok\" }", p);
        expect(errors.ok()).toBe(true);
        expect((bool)p.flag).toBe(true);
    });

    it("[positive] accepts leading/trailing whitespace around the whole document", []() {
        Primitives p;
        auto errors = jeson::parse("   {\"flag\":true}   ", p);
        expect(errors.ok()).toBe(true);
    });
});

describe("malformed top-level documents", []() {
    using namespace fixtures;

    it("[negative] reports trailing data after an otherwise valid value", []() {
        Primitives p;
        auto errors = jeson::parse(R"({"flag":true} garbage)", p);
        expect(errors.ok()).toBe(false);
    });

    it("[negative] reports an error for a completely empty document", []() {
        Primitives p;
        auto errors = jeson::parse("", p);
        expect(errors.ok()).toBe(false);
    });
});

// =========================================================================
// parser.hpp / writer.hpp - std::optional, std::vector, arrays of structs
// =========================================================================
describe("std::optional fields", []() {
    using namespace fixtures;

    it("[positive] stays nullopt when the key is absent", []() {
        Collection c;
        auto errors = jeson::parse(R"({})", c);
        expect(errors.ok()).toBe(true);
        expect(c.note->has_value()).toBe(false);
    });

    it("[positive] is set when the key is present with a value", []() {
        Collection c;
        auto errors = jeson::parse(R"({"note":"hi"})", c);
        expect(errors.ok()).toBe(true);
        expect(c.note->has_value()).toBe(true);
        expect(**c.note == std::string("hi")).toBe(true);
    });

    it("[positive] becomes nullopt when the JSON value is explicitly null", []() {
        Collection c;
        c.note      = "was set";
        auto errors = jeson::parse(R"({"note":null})", c);
        expect(errors.ok()).toBe(true);
        expect(c.note->has_value()).toBe(false);
    });

    it("[negative] still validates the wrapped type when present", []() {
        Collection c;
        auto errors = jeson::parse(R"({"rank":"not-a-number"})", c);
        expect(errors.ok()).toBe(false);
    });
});

describe("std::vector<int> fields", []() {
    using namespace fixtures;

    it("[positive] parses a list of numbers", []() {
        Collection c;
        auto errors = jeson::parse(R"({"numbers":[1,2,3]})", c);
        expect(errors.ok()).toBe(true);
        expect(c.numbers->size()).toBe(3u);
        expect((*c.numbers)[2] == 3).toBe(true);
    });

    it("[positive] accepts an empty array", []() {
        Collection c;
        auto errors = jeson::parse(R"({"numbers":[]})", c);
        expect(errors.ok()).toBe(true);
        expect(c.numbers->size()).toBe(0u);
    });

    it("[positive] clears any previous contents before parsing", []() {
        Collection c;
        c.numbers   = std::vector<int>{9, 9, 9};
        auto errors = jeson::parse(R"({"numbers":[1]})", c);
        expect(errors.ok()).toBe(true);
        expect(c.numbers->size()).toBe(1u);
    });

    it("[negative] reports a pathed, index-qualified error and keeps the rest of the array", []() {
        Collection c;
        auto errors = jeson::parse(R"({"numbers":[1,"bad",3]})", c);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].path).toBe("/numbers/1");
    });

    it("[negative] rejects a bare object where an array is expected", []() {
        Collection c;
        auto errors = jeson::parse(R"({"numbers":{}})", c);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("expected array");
    });
});

describe("std::vector<std::string> fields", []() {
    using namespace fixtures;

    it("[positive] parses a list of strings", []() {
        Collection c;
        auto errors = jeson::parse(R"({"labels":["a","b","c"]})", c);
        expect(errors.ok()).toBe(true);
        expect(c.labels->size()).toBe(3u);
    });
});

describe("std::vector<Struct> fields (arrays of nested objects)", []() {
    using namespace fixtures;

    it("[positive] parses multiple struct elements", []() {
        Collection c;
        auto errors = jeson::parse(R"({"points":[{"x":1,"y":2},{"x":3,"y":4}]})", c);
        expect(errors.ok()).toBe(true);
        expect(c.points->size()).toBe(2u);
        expect((*c.points)[0].x == 1).toBe(true);
        expect((*c.points)[1].y == 4).toBe(true);
    });

    it("[negative] paths an error inside one element without losing the others", []() {
        Collection c;
        auto errors = jeson::parse(R"({"points":[{"x":1,"y":2},{"x":"bad","y":4}]})", c);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].path).toBe("/points/1/x");
        expect(c.points->size()).toBe(2u);
        expect((*c.points)[0].x == 1).toBe(true);
    });

    it("[negative] a type mismatch on a numeric field produces exactly one error, not a cascade", []() {
        // Regression test: the arithmetic parser's error path used to omit
        // skipValue(), leaving the cursor stuck mid-token and desyncing the
        // rest of the document into several confusing follow-on errors.
        Collection c;
        auto errors = jeson::parse(R"({"points":[{"x":1,"y":2},{"x":"bad","y":4},{"x":5,"y":6}]})", c);
        expect(errors.ok()).toBe(false);
        expect(errors.list().size()).toBe(1u);
        expect(errors.list()[0].message).toBe("expected number");
        expect(c.points->size()).toBe(3u);
        expect((*c.points)[2].x == 5).toBe(true);
        expect((*c.points)[2].y == 6).toBe(true);
    });
});

// =========================================================================
// parser.hpp - object-level structure (unknown keys, malformed syntax)
// =========================================================================
describe("unknown and missing object keys", []() {
    using namespace fixtures;

    it("[positive] silently ignores an unrecognized key alongside valid ones", []() {
        Leaf l;
        auto errors = jeson::parse(R"({"name":"x","extra_unknown_field":123})", l);
        expect(errors.ok()).toBe(true);
        expect(l.name == std::string("x")).toBe(true);
    });

    it("[positive] ignores an unrecognized key even when its value is a nested object", []() {
        Leaf l;
        auto errors = jeson::parse(R"({"extra":{"a":{"b":1}},"name":"y"})", l);
        expect(errors.ok()).toBe(true);
        expect(l.name == std::string("y")).toBe(true);
    });

    it("[positive] leaves plain (non-Required) fields at their default when the key is missing", []() {
        Container c;
        auto errors = jeson::parse(R"({})", c);
        expect(errors.ok()).toBe(true);
        expect(c.leaf.name == std::string("")).toBe(true);
        expect((int)c.count).toBe(0);
    });
});

describe("malformed object syntax", []() {
    using namespace fixtures;

    it("[negative] reports an error for a missing colon after a key", []() {
        Leaf l;
        auto errors = jeson::parse(R"({"name" "x"})", l);
        expect(errors.ok()).toBe(false);
    });

    it("[negative] reports an error for a missing comma between members", []() {
        Container c;
        auto errors = jeson::parse(R"({"count":1 "leaf":{"name":"x"}})", c);
        expect(errors.ok()).toBe(false);
    });

    it("[negative] reports an error for an unterminated object", []() {
        Leaf l;
        auto errors = jeson::parse(R"({"name":"x")", l);
        expect(errors.ok()).toBe(false);
    });

    it("[negative] reports 'expected object' when a struct field is given a non-object value", []() {
        Container c;
        auto errors = jeson::parse(R"({"leaf":"not-an-object"})", c);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message).toBe("expected object");
    });

    it("[negative] a non-string key is rejected rather than looping or crashing", []() {
        Leaf l;
        auto errors = jeson::parse(R"({123:"x"})", l);
        expect(errors.ok()).toBe(false);
    });
});

// =========================================================================
// writer.hpp - compact vs pretty, escaping, omit-nulls
// =========================================================================
describe("compact output (the default)", []() {
    using namespace fixtures;

    it("[positive] produces no whitespace between tokens", []() {
        Leaf l;
        l.name         = "x";
        std::string json = jeson::toJson(l);
        expect(json).toBe(std::string(R"({"name":"x","enabled":true})"));
    });

    it("[positive] fits on a single line even for nested structs", []() {
        Container c;
        c.leaf.name       = "n";
        std::string json = jeson::toJson(c);
        expect(json.find('\n') == std::string::npos).toBe(true);
    });

    it("[positive] serializes string views without changing their JSON representation", []() {
        StringViews value;
        value.mode = "schedule";
        expect(jeson::toJson(value)).toBe(std::string(R"({"type":"thermostat","mode":"schedule"})"));
    });
});

describe("pretty output (jeson::pretty)", []() {
    using namespace fixtures;

    it("[positive] adds newlines and default 2-space indentation", []() {
        Leaf l;
        l.name             = "x";
        std::string json = jeson::toJson(l, jeson::pretty);
        expect(json).toBe(std::string("{\n  \"name\": \"x\",\n  \"enabled\": true\n}"));
    });

    it("[positive] indents nested objects one level deeper than their parent", []() {
        Container c;
        c.leaf.name       = "n";
        std::string json = jeson::toJson(c, jeson::pretty);
        expect(json.find("\n  \"leaf\": {") != std::string::npos).toBe(true);
        expect(json.find("\n    \"name\": \"n\"") != std::string::npos).toBe(true);
    });

    it("[positive] respects a custom indent width", []() {
        Leaf l;
        l.name             = "x";
        std::string json = jeson::toJson(l, jeson::WriteOptions{true, 4});
        expect(json.find("\n    \"name\": \"x\"") != std::string::npos).toBe(true);
    });

    it("[positive] still parses correctly after being pretty-printed", []() {
        Container original;
        original.leaf.name = "n";
        original.count       = 3;

        std::string json = jeson::toJson(original, jeson::pretty);
        Container restored;
        auto errors = jeson::parse(json, restored);

        expect(errors.ok()).toBe(true);
        expect(std::string(restored.leaf.name) == std::string(original.leaf.name)).toBe(true);
    });
});

describe("string escaping in output", []() {
    using namespace fixtures;

    it("[positive] escapes double quotes", []() {
        Leaf l;
        l.name             = "say \"hi\"";
        std::string json = jeson::toJson(l);
        expect(json.find(R"(\"hi\")") != std::string::npos).toBe(true);
    });

    it("[positive] escapes backslashes", []() {
        Leaf l;
        l.name             = "a\\b";
        std::string json = jeson::toJson(l);
        expect(json.find("a\\\\b") != std::string::npos).toBe(true);
    });

    it("[positive] escapes newlines and tabs", []() {
        Leaf l;
        l.name             = "line1\nline2\ttab";
        std::string json = jeson::toJson(l);
        expect(json.find("\\n") != std::string::npos).toBe(true);
        expect(json.find("\\t") != std::string::npos).toBe(true);
    });

    it("[positive] escapes every JSON control character and round-trips it", []() {
        Leaf original;
        original.name = std::string("a\b\f\n\r\t") + static_cast<char>(1) + "z";
        const std::string json = jeson::toJson(original);
        expect(json.find("\\b\\f\\n\\r\\t\\u0001") != std::string::npos).toBe(true);

        Leaf restored;
        const auto errors = jeson::parse(json, restored);
        expect(errors.ok()).toBe(true);
        expect(restored.name == original.name.value).toBe(true);
    });
});

describe("empty container edge cases in output", []() {
    using namespace fixtures;

    it("[positive] writes an empty vector as []", []() {
        Collection c;
        std::string json = jeson::toJson(c);
        expect(json.find(R"("numbers":[])") != std::string::npos).toBe(true);
    });

    it("[positive] writes an empty vector as [] even in pretty mode, no extra blank lines", []() {
        Collection c;
        std::string json = jeson::toJson(c, jeson::pretty);
        expect(json.find("[]") != std::string::npos).toBe(true);
        expect(json.find("[\n") != std::string::npos).toBe(false);
    });

    it("[positive] writes an unset optional as null by default", []() {
        Collection c;
        std::string json = jeson::toJson(c);
        expect(json.find(R"("note":null)") != std::string::npos).toBe(true);
    });
});

describe("compact_omit_nulls (rfl::NoOptionals equivalent)", []() {
    using namespace fixtures;

    it("[positive] omits an unset optional's key entirely", []() {
        Collection c;
        std::string json = jeson::toJson(c, jeson::compact_omit_nulls);
        expect(json.find("note") != std::string::npos).toBe(false);
    });

    it("[positive] still includes a set optional's value", []() {
        Collection c;
        c.note              = "present";
        std::string json = jeson::toJson(c, jeson::compact_omit_nulls);
        expect(json.find(R"("note":"present")") != std::string::npos).toBe(true);
    });

    it("[positive] leaves non-optional fields untouched", []() {
        Collection c;
        std::string json = jeson::toJson(c, jeson::compact_omit_nulls);
        expect(json.find(R"("numbers":[])") != std::string::npos).toBe(true);
    });
});

namespace enum_names_test {
enum class Severity : uint8_t { LOW, MEDIUM, CRITICAL };
} // namespace enum_names_test

// jeson::EnumNames<T> specialization - no macro, written at flat file
// scope. No "namespace enum_names_test {" or "namespace jeson {" wrapper
// needed for this to work; the qualified name alone is enough.
template <>
struct jeson::EnumNames<enum_names_test::Severity> {
    static constexpr bool has_mapping = true;
    static constexpr std::pair<enum_names_test::Severity, std::string_view> table[] = {
        {enum_names_test::Severity::LOW, "LOW"},
        {enum_names_test::Severity::MEDIUM, "MEDIUM"},
        {enum_names_test::Severity::CRITICAL, "CRITICAL"},
    };
};

namespace enum_names_test {
struct Alert { rfl::Field<"severity", Severity> severity = Severity::LOW; };
} // namespace enum_names_test

describe("jeson::EnumNames<T> registers an enum from outside its own namespace", []() {
    using namespace enum_names_test;

    it("[positive] writes the registered name correctly", []() {
        Alert a;
        a.severity        = Severity::CRITICAL;
        std::string json = jeson::toJson(a);
        expect(json).toBe(std::string(R"({"severity":"CRITICAL"})"));
    });

    it("[positive] parses the name back into the enum", []() {
        Alert a;
        auto errors = jeson::parse(R"({"severity":"MEDIUM"})", a);
        expect(errors.ok()).toBe(true);
        expect(a.severity == Severity::MEDIUM).toBe(true);
    });

    it("[negative] rejects a name not in the table", []() {
        Alert a;
        auto errors = jeson::parse(R"({"severity":"URGENT"})", a);
        expect(errors.ok()).toBe(false);
        expect(errors.list()[0].message.find("URGENT") != std::string::npos).toBe(true);
    });
});

namespace wide_range_test {
// An enum whose values fall outside the automatic default 0..31 probe
// range - jeson::EnumNames<T> : jeson::internal::MagicEnumNames<T, N> asks
// for names still to be auto-derived (no table to write by hand), just
// with a wider probe.
enum class ErrorCode : int { OK = 0, NOT_FOUND = 100, TIMEOUT = 200 };
} // namespace wide_range_test

template <>
struct jeson::EnumNames<wide_range_test::ErrorCode> : jeson::internal::MagicEnumNames<wide_range_test::ErrorCode, 256> {};

namespace wide_range_test {
struct Response { rfl::Field<"code", ErrorCode> code = ErrorCode::OK; };
} // namespace wide_range_test

describe("jeson::internal::MagicEnumNames<T, N> widens the automatic probe range", []() {
    using namespace wide_range_test;

    it("[positive] auto-derives the name for a value above the default 32-value probe range", []() {
        Response r;
        r.code            = ErrorCode::TIMEOUT;
        std::string json = jeson::toJson(r);
        expect(json).toBe(std::string(R"({"code":"TIMEOUT"})"));
    });

    it("[positive] parses the name back into the enum", []() {
        Response r;
        auto errors = jeson::parse(R"({"code":"NOT_FOUND"})", r);
        expect(errors.ok()).toBe(true);
        expect(r.code == ErrorCode::NOT_FOUND).toBe(true);
    });

    it("[negative] rejects a name that isn't a real enumerator", []() {
        Response r;
        auto errors = jeson::parse(R"({"code":"UNKNOWN"})", r);
        expect(errors.ok()).toBe(false);
    });
});


// =========================================================================
// errors.hpp - accumulation across a document, JSON serialization, merge()
// =========================================================================
describe("accumulating multiple errors across one document", []() {
    using namespace fixtures;

    it("[negative] collects every failing field, not just the first", []() {
        Reading r;
        auto errors = jeson::parse(R"({"value_a":1,"value_b":100})", r);
        expect(errors.ok()).toBe(false);
        expect(errors.list().size()).toBe(2u);
    });

    it("[negative] keeps each error's path distinct and correctly attributed", []() {
        Reading r;
        auto errors = jeson::parse(R"({"value_a":1,"value_b":100})", r);
        expect(errors.list()[0].path).toBe("/value_a");
        expect(errors.list()[1].path).toBe("/value_b");
    });

    it("[positive] reports zero errors when everything is valid", []() {
        Reading r;
        auto errors = jeson::parse(R"({"value_a":50,"value_b":0})", r);
        expect(errors.ok()).toBe(true);
        expect(errors.list().size()).toBe(0u);
    });
});

describe("ParseErrors::toJson()", []() {
    using namespace fixtures;

    it("[positive] reports ok:true and an empty errors array on success", []() {
        Leaf l;
        auto errors = jeson::parse(R"({"name":"x"})", l);
        expect(errors.toJson()).toBe(std::string(R"({"ok":true,"errors":[]})"));
    });

    it("[negative] reports ok:false with one entry per accumulated error", []() {
        Entry e;
        auto errors       = jeson::parse(R"({})", e);
        std::string json = errors.toJson();
        expect(json.find("\"ok\":false") != std::string::npos).toBe(true);
        expect(json.find("\"path\":\"/id\"") != std::string::npos).toBe(true);
        expect(json.find("missing required field") != std::string::npos).toBe(true);
    });
});

describe("ParseErrors::toString()", []() {
    using namespace fixtures;

    it("[positive] returns an empty string on success", []() {
        Leaf l;
        auto errors = jeson::parse(R"({"name":"x"})", l);
        expect(errors.toString()).toBe(std::string(""));
    });

    it("[negative] formats a single error as 'path: message'", []() {
        Entry e;
        auto errors = jeson::parse(R"({})", e);
        expect(errors.toString()).toBe(std::string("/id: missing required field"));
    });

    it("[negative] joins multiple errors with '; ' by default", []() {
        Reading r;
        auto errors = jeson::parse(R"({"value_a":1,"value_b":100})", r);
        expect(errors.toString()).toBe(std::string("/value_a: must be >= 10; /value_b: must be <= 5"));
    });

    it("[negative] accepts a custom separator", []() {
        Reading r;
        auto errors = jeson::parse(R"({"value_a":1,"value_b":100})", r);
        expect(errors.toString("\n")).toBe(std::string("/value_a: must be >= 10\n/value_b: must be <= 5"));
    });

    it("[negative] produces plain text with no embedded JSON structure - safe to drop into another string field", []() {
        Entry e;
        auto errors       = jeson::parse(R"({})", e);
        std::string plain = errors.toString();
        expect(plain.find('{') == std::string::npos).toBe(true);
        expect(plain.find('"') == std::string::npos).toBe(true);
    });
});

describe("ParseErrors::merge()", []() {
    it("[positive] combines errors from two separate ParseErrors instances", []() {
        jeson::ParseErrors a;
        a.add("/x", "bad x");
        jeson::ParseErrors b;
        b.add("/y", "bad y");
        a.merge(std::move(b));
        expect(a.list().size()).toBe(2u);
        expect(a.ok()).toBe(false);
    });

    it("[positive] merging an empty ParseErrors changes nothing", []() {
        jeson::ParseErrors a;
        a.add("/x", "bad x");
        jeson::ParseErrors empty;
        a.merge(std::move(empty));
        expect(a.list().size()).toBe(1u);
    });
});

// =========================================================================
// value.hpp - opt-in schema-less DOM for generic tooling
// =========================================================================
describe("jeson::Value dynamic JSON documents", []() {
    it("[positive] parses every JSON value kind while preserving object order", []() {
        jeson::Value value;
        const auto errors = jeson::parse(
            R"({"null":null,"bool":true,"signed":-2,"unsigned":3,"real":1.25,"text":"ok","array":[1,false],"object":{"x":1}})",
            value);

        expect(errors.ok()).toBe(true);
        expect(value.is<jeson::Value::Object>()).toBe(true);
        const auto& object = value.as<jeson::Value::Object>();
        expect(object.size()).toBe(8u);
        expect(object[0].first).toBe(std::string("null"));
        expect(object[0].second.is<jeson::Value::Null>()).toBe(true);
        expect(object[1].second.as<bool>()).toBe(true);
        expect(object[2].second.as<std::int64_t>()).toBe(-2);
        expect(object[3].second.as<std::uint64_t>()).toBe(3u);
        expect(object[4].second.as<double>() == 1.25).toBe(true);
        expect(object[5].second.as<std::string>()).toBe(std::string("ok"));
        expect(object[6].second.as<jeson::Value::Array>().size()).toBe(2u);
        expect(object[7].second.as<jeson::Value::Object>().size()).toBe(1u);
    });

    it("[positive] round-trips dynamic objects, arrays, scalars, and Unicode", []() {
        const std::vector<std::string> documents = {
            R"({"message":"\u20ac","items":[null,true,false,-1,2,3.5]})",
            R"([1,{"nested":[]}])",
            R"("root string")",
            R"(42)",
            R"(null)",
        };

        for (const auto& document : documents) {
            jeson::Value first;
            expect(jeson::parse(document, first).ok()).toBe(true);
            const std::string compact = jeson::toJson(first);

            jeson::Value second;
            expect(jeson::parse(compact, second).ok()).toBe(true);
            expect(jeson::toJson(second)).toBe(compact);
        }
    });

    it("[positive] pretty-prints a dynamic document", []() {
        jeson::Value value;
        expect(jeson::parse(R"({"a":[1,2]})", value).ok()).toBe(true);
        expect(jeson::toJson(value, jeson::pretty))
            .toBe(std::string("{\n  \"a\": [\n    1,\n    2\n  ]\n}"));
    });

    it("[positive] accepts finite subnormals and represents oversized integers as doubles", []() {
        jeson::Value value;
        expect(jeson::parse("[4.9406564584124654e-324,18446744073709551616]", value).ok()).toBe(true);
        const auto& values = value.as<jeson::Value::Array>();
        expect(values[0].is<double>()).toBe(true);
        expect(values[1].is<double>()).toBe(true);
    });

    it("[positive] emits shortest floating-point text while preserving its JSON number kind", []() {
        expect(jeson::toJson(jeson::Value(1.2345))).toBe(std::string("1.2345"));
        expect(jeson::toJson(jeson::Value(0.0))).toBe(std::string("0.0"));
        expect(jeson::toJson(jeson::Value(-0.0))).toBe(std::string("-0.0"));
    });

    it("[negative] rejects malformed schema-less documents", []() {
        for (const std::string input : {"[1,]", "{\"a\":1,}", "{\"a\" 1}", "+1", "tru"}) {
            jeson::Value value;
            expect(jeson::parse(input, value).ok()).toBe(false);
        }
    });

    it("[negative] keeps nested error paths while building them lazily", []() {
        jeson::Value value;
        const auto errors = jeson::parse(R"({"outer":[0,{"bad":tru}]})", value);

        expect(errors.ok()).toBe(false);
        expect(errors.list().empty()).toBe(false);
        expect(errors.list().front().path).toBe(std::string("/outer/1/bad"));
    });
});

// =========================================================================
// compact_document.hpp - low-allocation schema-less DOM
// =========================================================================
describe("jeson::CompactDocument", []() {
    it("[positive] exposes every JSON kind through lightweight views", []() {
        jeson::CompactDocument document;
        const auto errors = jeson::parse(
            R"({"null":null,"bool":true,"signed":-2,"unsigned":3,"real":1.25,"text":"ok","array":[1,false],"object":{"x":1}})",
            document);

        expect(errors.ok()).toBe(true);
        const auto root = document.root();
        expect(root.isObject()).toBe(true);
        expect(root.size()).toBe(8u);
        expect(root["null"].isNull()).toBe(true);
        expect(root["bool"].boolValue()).toBe(true);
        expect(root["signed"].int64Value()).toBe(-2);
        expect(root["unsigned"].uint64Value()).toBe(3u);
        expect(root["real"].doubleValue() == 1.25).toBe(true);
        expect(root["text"].stringValue() == "ok").toBe(true);
        expect(root["array"][0].uint64Value()).toBe(1u);
        expect(root["array"][1].boolValue(true)).toBe(false);
        expect(root["object"]["x"].uint64Value()).toBe(1u);
        expect(root["missing"].valid()).toBe(false);
    });

    it("[positive] iterates array and object children in source order", []() {
        jeson::CompactDocument document;
        expect(jeson::parse(R"({"a":1,"b":2,"c":3})", document).ok()).toBe(true);

        std::string keys;
        std::uint64_t total = 0;
        for (const auto child : document.root().children()) {
            keys += child.key();
            total += child.uint64Value();
        }
        expect(keys).toBe(std::string("abc"));
        expect(total).toBe(6u);
    });

    it("[positive] owns decoded strings including values larger than one page", []() {
        const std::string longText(5000, 'x');
        const std::string input = std::string("{\"escaped\":\"\\u20ac\",\"long\":\"") + longText + "\"}";
        jeson::CompactDocument document;
        expect(jeson::parse(input, document).ok()).toBe(true);

        expect(document.root()["escaped"].stringValue() == "€").toBe(true);
        expect(document.root()["long"].stringValue().size()).toBe(5000u);
        expect(document.root()["long"].stringValue().front()).toBe('x');
    });

    it("[positive] round-trips compact and pretty JSON without a Value tree", []() {
        jeson::CompactDocument document;
        expect(jeson::parse(R"({"message":"hello","items":[null,true,-1,2,3.5]})", document).ok()).toBe(true);
        expect(jeson::toJson(document))
            .toBe(std::string(R"({"message":"hello","items":[null,true,-1,2,3.5]})"));
        expect(jeson::toJson(document.root()["items"]))
            .toBe(std::string("[null,true,-1,2,3.5]"));
        expect(jeson::toJson(document, jeson::pretty))
            .toBe(std::string("{\n  \"message\": \"hello\",\n  \"items\": [\n    null,\n    true,\n    -1,\n    2,\n    3.5\n  ]\n}"));
    });

    it("[positive] reports generic document statistics", []() {
        jeson::CompactDocument document;
        expect(jeson::parse(R"({"a":[1,true,null],"b":"text"})", document).ok()).toBe(true);
        const auto statistics = document.statistics();
        expect(statistics.objectCount).toBe(1u);
        expect(statistics.arrayCount).toBe(1u);
        expect(statistics.numberCount).toBe(1u);
        expect(statistics.trueCount).toBe(1u);
        expect(statistics.nullCount).toBe(1u);
        expect(statistics.memberCount).toBe(2u);
        expect(statistics.elementCount).toBe(3u);
        expect(statistics.stringCount).toBe(3u);
        expect(statistics.stringLength).toBe(6u);
    });

    it("[negative] rejects malformed input and clears partial state", []() {
        jeson::CompactDocument document;
        expect(jeson::parse(R"({"valid":true})", document).ok()).toBe(true);
        const auto errors = jeson::parse(R"({"broken":[1,]})", document);

        expect(errors.ok()).toBe(false);
        expect(errors.list().empty()).toBe(false);
        expect(document.empty()).toBe(true);
        expect(document.root().valid()).toBe(false);
    });
});
