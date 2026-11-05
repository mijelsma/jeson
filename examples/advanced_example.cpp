/**
 * @file advanced_example.cpp
 * @brief Every jeson feature working together in one realistic scenario:
 * multiple people in an array, nested addresses, every enum working with
 * zero setup,
 * validators, a required field, optional fields, and every writer option.
 *
 * See the other examples for each feature in isolation:
 * simple_example, nested_and_arrays_example, enum_example,
 * validator_example, writer_example, errors_example.
 */
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "jeson/jeson.hpp"

// ---- every enum here just works, zero setup -------------------------

enum class Gender : uint8_t
{
    MALE,
    FEMALE,
    OTHER
};
enum class MaritalStatus : uint8_t
{
    SINGLE,
    MARRIED,
    DIVORCED,
    WIDOWED
};
enum class Role : uint8_t
{
    MEMBER,
    ADMIN,
    OWNER
};

// ---- structs: no macro needed anywhere below ---------------------------

struct Address
{
    jeson::Field<"street", std::string> street;
    jeson::Field<"city", std::string> city;
    jeson::Field<"zip_code", jeson::Validator<std::string, jeson::MinSize<3>, jeson::MaxSize<10>>> zipCode = "00000";
    jeson::Field<"country", std::string> country                                                           = "Unknown";
};

struct Person
{
    jeson::Field<"id", jeson::Required<std::string>> id;
    jeson::Field<"name", std::string> name;
    jeson::Field<"age", jeson::Validator<uint8_t, jeson::Minimum<0>, jeson::Maximum<130>>> age = 0;
    jeson::Field<"gender", Gender> gender                                                      = Gender::OTHER;
    jeson::Field<"marital_status", MaritalStatus> maritalStatus                                = MaritalStatus::SINGLE;
    jeson::Field<"role", Role> role                                                            = Role::MEMBER;
    jeson::Field<"address", Address> address;
    jeson::Field<"email", std::optional<std::string>> email;
    jeson::Field<"hobbies", std::vector<std::string>> hobbies;
};

// The array-of-structs case: a group holding multiple people.
struct Group
{
    jeson::Field<"group_name", std::string> groupName;
    jeson::Field<"members", std::vector<Person>> members;
};

static void printErrors(const jeson::ParseErrors& t_errors) {
    std::cout << "  toJson():   " << t_errors.toJson() << "\n";
    std::cout << "  toString(): " << t_errors.toString() << "\n";
}

int main() {
    std::cout << "--- 1) building a person by hand: no macro, no .value() anywhere ---\n";
    {
        Person alice;
        alice.id            = "p-001";
        alice.name          = "Alice Johnson";
        alice.age           = 34;
        alice.gender        = Gender::FEMALE;
        alice.maritalStatus = MaritalStatus::MARRIED;
        alice.role          = Role::ADMIN;
        alice.email         = "alice@example.com";
        alice.hobbies       = {"chess", "hiking"};
        alice.address       = Address{"12 Main St", "Springfield", "12345", "USA"}; // whole-struct assignment

        std::cout << "alice.name=" << alice.name << " alice.address.city=" << alice.address.city << "\n";
        std::cout << jeson::toJson(alice, jeson::pretty) << "\n";
    }

    std::cout << "\n--- 2) multiple people in one array, parsed from JSON ---\n";
    {
        std::string input = R"({
            "group_name": "Book Club",
            "members": [
                {"id":"p-001","name":"Alice Johnson","age":34,"gender":"FEMALE","marital_status":"MARRIED","role":"ADMIN",
                 "address":{"street":"12 Main St","city":"Springfield","zip_code":"12345"},"hobbies":["chess","hiking"]},
                {"id":"p-002","name":"Bob Smith","age":41,"gender":"MALE","marital_status":"SINGLE","role":"MEMBER",
                 "address":{"street":"9 Oak Ave","city":"Shelbyville","zip_code":"54321"},"email":"bob@example.com"},
                {"id":"p-003","name":"Cara Lee","age":29,"marital_status":"DIVORCED","role":"OWNER",
                 "address":{"street":"1 Elm Rd","city":"Ogdenville","zip_code":"99999"}}
            ]
        })";
        Group group;
        auto errors = jeson::parse(input, group);
        std::cout << "ok=" << errors.ok() << " member_count=" << group.members->size() << "\n";
        for (auto& p: *group.members) {
            std::cout << "  " << p.name << " (" << p.address.city << ")\n";
        }
    }

    std::cout << "\n--- 3) validator rejects an out-of-range age ---\n";
    {
        Person p;
        auto errors = jeson::parse(R"({"id":"p-004","age":200})", p);
        printErrors(errors);
    }

    std::cout << "\n--- 4) validator rejects a zip code that's too short ---\n";
    {
        Person p;
        auto errors = jeson::parse(R"({"id":"p-005","address":{"zip_code":"1"}})", p);
        printErrors(errors);
    }

    std::cout << "\n--- 5) Required<T>: missing id fails, present id succeeds ---\n";
    {
        Person missing;
        printErrors(jeson::parse(R"({"name":"No Id"})", missing));

        Person present;
        auto errors = jeson::parse(R"({"id":"p-006","name":"Has Id"})", present);
        std::cout << "  ok=" << errors.ok() << "\n";
    }

    std::cout << "\n--- 6) one bad member in an array doesn't lose the others ---\n";
    {
        std::string input = R"({"group_name":"Mixed","members":[
            {"id":"p-007","name":"Good One","age":30},
            {"id":"p-008","name":"Bad One","age":"not-a-number"},
            {"id":"p-009","name":"Good Two","age":50}
        ]})";
        Group group;
        auto errors = jeson::parse(input, group);
        std::cout << "ok=" << errors.ok() << " (one error expected, still 3 members parsed)\n";
        printErrors(errors);
        std::cout << "  member_count=" << group.members->size() << "\n";
    }

    std::cout << "\n--- 7) every enum just works, zero setup ---\n";
    {
        Person p;
        p.gender        = Gender::MALE;
        p.maritalStatus = MaritalStatus::WIDOWED;
        p.role          = Role::OWNER;
        std::cout << jeson::toJson(p) << "\n";
    }

    std::cout << "\n--- 8) optional email: null by default, omitted with compact_omit_nulls ---\n";
    {
        Person p;
        p.name = "No Email";
        std::cout << "default:            " << jeson::toJson(p).substr(0, 60) << "...\n";
        std::cout << "compact_omit_nulls: " << jeson::toJson(p, jeson::compact_omit_nulls).substr(0, 60) << "...\n";
    }

    std::cout << "\n--- 9) round-trip: write a whole group, parse it back ---\n";
    {
        Group original;
        original.groupName = "Round Trip Test";
        Person a;
        a.id   = "p-010";
        a.name = "First";
        Person b;
        b.id             = "p-011";
        b.name           = "Second";
        original.members = {a, b};

        std::string json = jeson::toJson(original, jeson::pretty);
        Group restored;
        auto errors = jeson::parse(json, restored);
        std::cout << "ok=" << errors.ok() << " restored_count=" << restored.members->size() << "\n";
    }

    return 0;
}
