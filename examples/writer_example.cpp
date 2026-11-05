/**
 * @file writer_example.cpp
 * @brief jeson::toJson()'s formatting options: compact (default), pretty,
 * and compact_omit_nulls - plus string escaping.
 */
#include <iostream>
#include <optional>
#include <string>

#include "jeson/jeson.hpp"

struct Note {
    jeson::Field<"title", std::string> title;
    jeson::Field<"body", std::string> body;
    jeson::Field<"pinned", bool> pinned = false;
    jeson::Field<"archived_at", std::optional<std::string>> archivedAt; // left unset below
};

int main() {
    Note note;
    note.title  = "Reminder";
    note.body   = "Contains a \"quoted\" word and a\nnewline.";
    note.pinned = true;

    std::cout << "--- compact (the default) ---\n"
              << jeson::toJson(note) << "\n";

    std::cout << "\n--- pretty (jeson::pretty) ---\n"
              << jeson::toJson(note, jeson::pretty) << "\n";

    std::cout << "\n--- pretty with a custom indent width ---\n"
              << jeson::toJson(note, jeson::WriteOptions{true, 4}) << "\n";

    std::cout << "\n--- unset optional: default writes null ---\n"
              << jeson::toJson(note) << "\n";

    std::cout << "\n--- unset optional: compact_omit_nulls drops the key entirely ---\n"
              << jeson::toJson(note, jeson::compact_omit_nulls) << "\n";

    return 0;
}
