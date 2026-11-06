#include <iostream>
#include <jeson/jeson.hpp>

int main() {
    jeson::CompactDocument document;
    const auto errors = jeson::parse(
        R"({"device":"sensor-01","samples":[21.5,22.0],"online":true})",
        document);
    if (!errors) {
        std::cerr << errors.toString() << '\n';
        return 1;
    }

    const auto root = document.root();
    std::cout << "device: " << root["device"].stringValue() << '\n';
    for (const auto sample : root["samples"].children()) {
        std::cout << "sample: " << sample.doubleValue() << '\n';
    }
    std::cout << jeson::toJson(document, jeson::pretty) << '\n';
}
