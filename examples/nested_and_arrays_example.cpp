/**
 * @file nested_and_arrays_example.cpp
 * @brief Nested structs, arrays of primitives, and arrays of structs -
 * the three shapes flat examples don't cover.
 */
#include <iostream>
#include <string>
#include <vector>

#include "jeson/jeson.hpp"

struct Address {
    jeson::Field<"street", std::string> street;
    jeson::Field<"city", std::string> city;
};

struct Person {
    jeson::Field<"name", std::string> name;

    // A nested struct field is transparent: no .value() at any depth,
    // person.address.city works exactly like a plain member would.
    jeson::Field<"address", Address> address;

    jeson::Field<"tags", std::vector<std::string>> tags;
};

struct Team {
    jeson::Field<"name", std::string> name;

    // An array of structs - each element gets the full Person treatment.
    jeson::Field<"members", std::vector<Person>> members;
};

int main() {
    std::string input = R"({
        "name": "Backend",
        "members": [
            {"name": "Alice", "address": {"street": "1 Main St", "city": "Springfield"}, "tags": ["lead", "backend"]},
            {"name": "Bob",   "address": {"street": "2 Oak Ave",  "city": "Shelbyville"}, "tags": ["backend"]}
        ]
    })";

    Team team;
    auto errors = jeson::parse(input, team);
    if (!errors.ok()) {
        std::cout << "parse failed: " << errors.toString() << "\n";
        return 1;
    }

    // team.members is Field<"members", std::vector<Person>> - use * or ->
    // to reach vector's own methods (size(), begin()/end(), ...), since
    // those aren't forwarded automatically the way nested-struct access is.
    std::cout << team.name << " has " << team.members->size() << " members:\n";
    for (const auto& person : *team.members) {
        std::cout << "  " << person.name << " lives in " << person.address.city << " (";
        for (std::size_t i = 0; i < person.tags->size(); ++i) {
            std::cout << (*person.tags)[i] << (i + 1 < person.tags->size() ? ", " : "");
        }
        std::cout << ")\n";
    }

    // Building the same shape by hand, without ever touching JSON text:
    Team newTeam;
    newTeam.name = "Frontend";
    Person carol;
    carol.name           = "Carol";
    carol.address.street = "3 Elm Rd";
    carol.address.city   = "Ogdenville";
    carol.tags           = {"lead", "frontend"};
    newTeam.members       = {carol};

    std::cout << "\n" << jeson::toJson(newTeam, jeson::pretty) << "\n";

    return 0;
}
