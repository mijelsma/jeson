/**
 * @file simple_example.cpp
 * @brief The smallest useful jeson example: one flat struct, parse it,
 * change a field, write it back out. Start here.
 */
#include <iostream>
#include <string>

#include "jeson/jeson.hpp"

// A struct is "just" a normal C++ struct - the only annotation is wrapping
// each member's type in jeson::Field<"json_key", T>. No base class, no
// registration macro, no separate schema to keep in sync.
struct Device {
    jeson::Field<"name", std::string> name;
    jeson::Field<"port", int> port        = 8080; // ordinary default member initializer
    jeson::Field<"enabled", bool> enabled = true;
};

int main() {
    // ---- parsing ----------------------------------------------------
    std::string input = R"({"name":"sensor-01","port":9000,"enabled":false})";

    Device device;
    jeson::ParseErrors errors = jeson::parse(input, device);

    if (!errors.ok()) {
        std::cout << "parse failed: " << errors.toString() << "\n";
        return 1;
    }

    // ---- plain member access -----------------------------------------
    // No .value(), no unwrapping - fields behave like ordinary members.
    std::cout << "name: " << device.name << "\n";
    std::cout << "port: " << device.port << "\n";

    device.port = 9090; // ordinary assignment

    // ---- writing ------------------------------------------------------
    std::cout << "\nround-tripped:\n" << jeson::toJson(device, jeson::pretty) << "\n";

    return 0;
}
