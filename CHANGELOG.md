# Changelog

All notable changes to JESON are documented here. The project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.2.0] - 2026-11-06

- Added the opt-in `jeson::Value` dynamic DOM for schema-less documents and generic tooling.
- Added `jeson::CompactDocument`, a read-oriented schema-less DOM with 24-byte nodes, paged storage, document-owned strings, lightweight views, subtree serialization, and configurable page sizes.
- Added nativejson-benchmark integration for performance, memory, conformance, and code-size comparisons.
- Added a separate JSON-to-application-model benchmark that distinguishes native typed parsing from DOM plus manual mapping.
- Reduced the compact DOM's large-document parse peak allocation by about 40% and allocation calls by about 94% relative to `jeson::Value` in the checked benchmark.
- Emit floating-point values with enough significant digits for reliable round trips.

## [0.1.0] - 2026-11-05

- Initial public project structure for the header-only library.
- Typed parsing and serialization for primitives, strings, enums, vectors, optionals, and nested aggregate models.
- Strict JSON number/range checks, complete control-character escaping, and Unicode escape decoding with surrogate-pair support.
- Required fields, compile-time validator policies, patch models, accumulated errors, and pretty/omit-null writer modes.
- Integration metadata for CMake, ESP-IDF, PlatformIO, Arduino, Meson, Bazel, Conan, pkg-config, and Zephyr.

[Unreleased]: https://github.com/mijelsma/jeson/compare/0.2.0...HEAD
[0.2.0]: https://github.com/mijelsma/jeson/releases/tag/0.2.0
[0.1.0]: https://github.com/mijelsma/jeson/releases/tag/0.1.0
