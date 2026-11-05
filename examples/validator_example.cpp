/**
 * @file validator_example.cpp
 * @brief jeson::Validator (range/size constraints) and jeson::Required
 * (mandatory fields), including both accepted and rejected input.
 */
#include <cstdint>
#include <iostream>
#include <string>

#include "jeson/jeson.hpp"

struct Reading {
    // Required<T>: parsing fails if this key is absent.
    jeson::Field<"sensor_id", jeson::Required<std::string>> sensorID;

    // Validator<T, Constraints...>: parsing fails if the value violates
    // any listed constraint. Combine several with more template arguments.
    jeson::Field<"temperature", jeson::Validator<int16_t, jeson::Minimum<-40>, jeson::Maximum<85>>> temperature = 20;
    jeson::Field<"humidity", jeson::Validator<uint8_t, jeson::Minimum<0>, jeson::Maximum<100>>> humidity          = 50;

    // MinSize/MaxSize work the same way, on anything with .size() (strings, vectors, ...).
    jeson::Field<"label", jeson::Validator<std::string, jeson::MinSize<1>, jeson::MaxSize<32>>> label = "unlabeled";
};

static void tryParse(const char* label, const std::string& input) {
    Reading r;
    auto errors = jeson::parse(input, r);
    std::cout << label << ": " << (errors.ok() ? "ok" : errors.toString()) << "\n";
}

int main() {
    std::cout << "--- valid input ---\n";
    tryParse("in range", R"({"sensor_id":"s-01","temperature":22,"humidity":45})");

    std::cout << "\n--- Required<T> ---\n";
    tryParse("missing sensor_id", R"({"temperature":22})");
    tryParse("present sensor_id", R"({"sensor_id":"s-02"})");

    std::cout << "\n--- Minimum/Maximum ---\n";
    tryParse("temperature too low", R"({"sensor_id":"s-03","temperature":-50})");
    tryParse("temperature too high", R"({"sensor_id":"s-03","temperature":90})");
    tryParse("humidity out of range", R"({"sensor_id":"s-03","humidity":150})");

    std::cout << "\n--- MinSize/MaxSize ---\n";
    tryParse("empty label", R"({"sensor_id":"s-04","label":""})");
    tryParse("label too long", R"({"sensor_id":"s-04","label":"this-label-is-way-too-long-for-the-limit"})");

    std::cout << "\n--- direct C++ assignment is NOT re-validated (see validator.hpp) ---\n";
    Reading r;
    r.temperature = 999; // accepted unchecked - constraints apply at the JSON parse boundary only
    std::cout << "r.temperature.value.value() = " << r.temperature.value.value() << "\n";

    return 0;
}
