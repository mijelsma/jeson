#!/usr/bin/env python3
"""Regenerates the tie_fields_n(...) overload chain in include/jeson/reflect.hpp.

Each overload structured-binding-decomposes a struct with exactly N members
into a std::tuple of references. There's no way to write this generically in
C++ (the number of names in a structured binding must be a literal), so it's
generated mechanically instead - once, checked into the repo, not run at
build time.

Usage:
    python3 tools/generate_reflect.py [max_fields]

Defaults to 32 fields (matching the current JESON_MAX_FIELDS default in
reflect.hpp). Prints the generated overloads to stdout; paste them into
reflect.hpp in place of the existing block, and update JESON_MAX_FIELDS's
default value to match if you changed the count.
"""
import sys


def generate(max_fields: int) -> str:
    lines = ["template <typename T> auto tie_fields_n(T&, std::integral_constant<std::size_t, 0>) { return std::tie(); }"]
    for n in range(1, max_fields + 1):
        names = [f"m{i}" for i in range(n)]
        binding = ", ".join(names)
        tie_args = ", ".join(names)
        lines.append(
            f"template <typename T> auto tie_fields_n(T& t, std::integral_constant<std::size_t, {n}>) "
            f"{{ auto& [{binding}] = t; return std::tie({tie_args}); }}"
        )
    return "\n".join(lines)


if __name__ == "__main__":
    max_fields = int(sys.argv[1]) if len(sys.argv) > 1 else 32
    print(generate(max_fields))
