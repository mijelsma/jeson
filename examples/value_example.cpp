#include <iostream>
#include <string>

#include <jeson/jeson.hpp>

int main() {
    jeson::Value document;
    const auto errors = jeson::parse(
        R"({"device":"sensor-01","online":true,"samples":[21.5,22.0]})",
        document);
    if (!errors) {
        std::cerr << errors.toString() << '\n';
        return 1;
    }

    std::cout << jeson::toJson(document, jeson::pretty) << '\n';
}
