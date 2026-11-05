/**
 * @file errors_example.cpp
 * @brief jeson::ParseErrors: accumulation across an entire document,
 * toJson() (machine-readable) vs toString() (plain, for logs/status
 * messages), and merge().
 */
#include <iostream>
#include <string>

#include "jeson/jeson.hpp"

struct Config {
    jeson::Field<"id", jeson::Required<std::string>> id;
    jeson::Field<"retries", jeson::Validator<int, jeson::Minimum<0>, jeson::Maximum<10>>> retries = 3;
    jeson::Field<"timeout_ms", jeson::Validator<int, jeson::Minimum<100>>> timeoutMs               = 1000;
};

int main() {
    std::cout << "--- errors accumulate across the whole document, not just the first ---\n";
    {
        Config c;
        auto errors = jeson::parse(R"({"retries":99,"timeout_ms":1})", c);
        // Three independent problems: missing id, retries out of range,
        // timeout_ms out of range - all three are reported, not just one.
        std::cout << "error count: " << errors.list().size() << "\n";
        for (const auto& e : errors.list()) {
            std::cout << "  " << e.path << ": " << e.message << "\n";
        }
    }

    std::cout << "\n--- toJson(): machine-readable, for an API response ---\n";
    {
        Config c;
        auto errors = jeson::parse(R"({})", c);
        std::cout << errors.toJson() << "\n";
    }

    std::cout << "\n--- toString(): plain text, safe to embed in another JSON string field ---\n";
    {
        Config c;
        auto errors = jeson::parse(R"({})", c);

        // A status-message struct you'd actually publish over MQTT/HTTP:
        struct StatusMessage {
            jeson::Field<"ok", bool> ok;
            jeson::Field<"message", std::string> message;
        };
        StatusMessage status;
        status.ok      = errors.ok();
        status.message = errors.ok() ? "applied" : ("rejected: " + errors.toString());

        std::cout << jeson::toJson(status) << "\n";
        // Notice there's no nested-JSON-inside-a-JSON-string mess here -
        // toString() never contains '{', '}', or unescaped quotes.
    }

    std::cout << "\n--- merge(): combining errors from two independent parses ---\n";
    {
        jeson::ParseErrors combined;

        Config a;
        combined.merge(jeson::parse(R"({"retries":-1})", a));

        Config b;
        combined.merge(jeson::parse(R"({"timeout_ms":0})", b));

        std::cout << "combined error count: " << combined.list().size() << "\n";
        std::cout << combined.toString() << "\n";
    }

    return 0;
}
