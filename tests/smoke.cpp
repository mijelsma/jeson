#include <jeson/jeson.hpp>

#include <string>

struct SmokeModel {
    jeson::Field<"name", jeson::Required<std::string>> name;
    jeson::Field<"count", int> count = 1;
};

int main() {
    SmokeModel model;
    const auto errors = jeson::parse(R"({"name":"jeson","count":2})", model);
    return errors.ok() && jeson::toJson(model) == R"({"name":"jeson","count":2})" ? 0 : 1;
}

