/**
 * @file patch_example.cpp
 * @brief Apply partial command/config sections without duplicating schemas.
 */
#include <iostream>
#include <optional>
#include <string>

#include "jeson/jeson.hpp"

struct Network {
    // A supplied Network section may update any subset of its fields.
    static constexpr jeson::PatchSectionPolicy jesonPatchPolicy =
        jeson::PatchSectionPolicy::PARTIAL;

    jeson::Field<"ssid", std::string> ssid = "factory";
    jeson::Field<"enabled", bool> enabled = true;
};

struct Limits {
    // The default COMPLETE policy requires every field when this section
    // appears in a patch.
    jeson::Field<"minimum", int> minimum = 0;
    jeson::Field<"maximum", int> maximum = 100;
};

struct Settings {
    jeson::Field<"network", std::optional<Network>> network = Network{};
    jeson::Field<"limits", std::optional<Limits>> limits = Limits{};
};

int main() {
    Settings current;
    current.network.value->ssid = "workshop";
    current.network.value->enabled = true;

    // The baseline preserves fields omitted inside a PARTIAL section and
    // lets Patch distinguish omitted root sections from supplied ones.
    jeson::Patch<Settings> patch{current};
    const auto errors = jeson::parse(R"({"network":{"enabled":false}})", patch);
    if (!errors) {
        std::cerr << errors.toString() << '\n';
        return 1;
    }

    std::cout << jeson::toJson(patch, jeson::pretty) << '\n';
    return 0;
}

