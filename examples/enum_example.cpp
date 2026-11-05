/**
 * @file enum_example.cpp
 * @brief Every enum, zero setup, always serializes as its name - see
 * enum.hpp's @file comment for how this works.
 */
#include <cstdint>
#include <iostream>
#include <string>

#include "jeson/jeson.hpp"

// Default "magic enums" automatically use the enumerator name as their string representation.
enum class Priority : uint8_t
{
    LOW,
    MEDIUM,
    HIGH
};

// Opt-in override: rename a wire name from the default magic enum C++ spelling
enum class Status : uint8_t
{
    PENDING,
    ACTIVE,
    DONE
};

template<>
struct jeson::EnumNames<Status>
{
    static constexpr bool has_mapping                            = true;
    static constexpr std::pair<Status, std::string_view> table[] = {
        {Status::PENDING, "pending"},
        {Status::ACTIVE, "active"},
        {Status::DONE, "done"},
    };
};

// To limit compile time and binary size, the default enum probe range is 0..31.
// Larger enum values (the error codes here) need an explicitly wider probe.
enum class ErrorCode : int
{
    OK        = 0,
    NOT_FOUND = 100,
    TIMEOUT   = 200
};

template<>
struct jeson::EnumNames<ErrorCode> : jeson::internal::MagicEnumNames<ErrorCode, 256>
{
};

// The main json object with all their fields
struct Task
{
    jeson::Field<"priority", Priority> priority     = Priority::LOW;
    jeson::Field<"status", Status> status           = Status::PENDING;
    jeson::Field<"error_code", ErrorCode> errorCode = ErrorCode::OK;
};

int main() {
    // Simply assign enum values to their fields
    Task task;
    task.priority  = Priority::HIGH;
    task.status    = Status::ACTIVE;
    task.errorCode = ErrorCode::TIMEOUT;

    // Every field writes as its name. Note the overrides in status and error code
    std::cout << "[1] Pretty JSON with enum strings:\n";
    std::cout << jeson::toJson(task, jeson::pretty) << "\n";

    //
    std::cout << "[2] Valid Parsed JSON:\n";
    Task parsed;
    auto errors = jeson::parse(R"({"priority":"MEDIUM","status":"done"})", parsed);
    std::cout << "\nok=" << errors.ok() << " " << jeson::toJson(parsed) << "\n";

    // An unrecognized name is rejected with a clear error, for any enum:
    std::cout << "[2] Invalid ENUM name parsing\n";
    Task bad;
    auto badErrors = jeson::parse(R"({"priority":"SOME_INVALID_NAME"})", bad);
    std::cout << "\nbad input: " << badErrors.toString() << "\n";

    return 0;
}
