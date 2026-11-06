/**
 * @file compact_document.hpp
 * @brief Low-allocation, document-owned DOM for constrained devices.
 *
 * CompactDocument is the schema-less alternative to jeson::Value when RAM
 * and heap traffic matter more than owning STL containers at every node.
 * Values live in small document-owned pages and are exposed through cheap,
 * read-only CompactValue views. Re-parsing or moving the document invalidates
 * its views; destroying the input JSON does not.
 */
#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "enum.hpp"
#include "errors.hpp"
#include "writer.hpp"

#ifndef JESON_COMPACT_NODE_PAGE_NODES
/// Number of 24-byte nodes allocated per CompactDocument node page.
#define JESON_COMPACT_NODE_PAGE_NODES 64
#endif

#ifndef JESON_COMPACT_STRING_PAGE_BYTES
/// Bytes allocated per ordinary CompactDocument string page.
#define JESON_COMPACT_STRING_PAGE_BYTES 1024
#endif

namespace jeson {

enum class CompactType : std::uint8_t { INVALID, NULL_VALUE, BOOL, INT, UINT, DOUBLE, STRING, ARRAY, OBJECT };

class CompactDocument;
class CompactValue;
class CompactChildren;

/** Aggregate counts used by generic tooling and benchmarks. */
struct CompactStatistics {
    std::size_t objectCount = 0;
    std::size_t arrayCount = 0;
    std::size_t numberCount = 0;
    std::size_t stringCount = 0;
    std::size_t trueCount = 0;
    std::size_t falseCount = 0;
    std::size_t nullCount = 0;
    std::size_t memberCount = 0;
    std::size_t elementCount = 0;
    std::size_t stringLength = 0;
};

/**
 * @brief An owning, read-oriented dynamic JSON document.
 *
 * Nodes and strings are held in bounded pages rather than one allocation per
 * array, object, key, and long string. The input buffer is never borrowed.
 */
class CompactDocument {
public:
    CompactDocument() = default;
    CompactDocument(const CompactDocument&) = delete;
    CompactDocument& operator=(const CompactDocument&) = delete;
    CompactDocument(CompactDocument&&) noexcept = default;
    CompactDocument& operator=(CompactDocument&&) noexcept = default;

    /** Parses and replaces the document. All earlier CompactValue views become invalid. */
    ParseErrors parse(std::string_view t_json) {
        clear();
        m_input = t_json;
        m_inputSize = t_json.size();
        m_position = 0;
        m_root = parseValue(0, 0, 0);
        skipWhitespace();

        ParseErrors errors;
        if (m_root == NONE || m_position != m_input.size()) {
            const std::size_t errorPosition = m_position;
            clear();
            errors.add("", "invalid JSON at byte " + std::to_string(errorPosition));
            return errors;
        }
        m_input = {};
        return errors;
    }

    /** Clears all nodes and invalidates every view into this document. */
    void clear() {
        m_nodePages.clear();
        m_strings.clear();
        m_input = {};
        m_inputSize = 0;
        m_position = 0;
        m_nodeCount = 0;
        m_root = NONE;
    }

    [[nodiscard]] bool empty() const { return m_root == NONE; }
    [[nodiscard]] CompactValue root() const;
    [[nodiscard]] CompactStatistics statistics() const {
        CompactStatistics result;
        if (m_root != NONE) collect(m_root, result);
        return result;
    }

    [[nodiscard]] std::string toJson(WriteOptions t_options = {}) const {
        std::string output;
        output.reserve(m_inputSize);
        if (m_root != NONE) write(m_root, output, t_options, 0);
        return output;
    }

    [[nodiscard]] std::size_t nodeCount() const { return m_nodeCount; }
    [[nodiscard]] std::size_t stringBytes() const { return m_strings.usedBytes(); }
    [[nodiscard]] std::size_t reservedNodeBytes() const {
        return m_nodePages.size() * NODE_PAGE_NODES * sizeof(Node);
    }
    [[nodiscard]] std::size_t reservedStringBytes() const { return m_strings.reservedBytes(); }

private:
    friend class CompactValue;
    friend class CompactChildren;

    static constexpr std::uint32_t NONE = std::numeric_limits<std::uint32_t>::max();
    static constexpr std::size_t NODE_PAGE_NODES = JESON_COMPACT_NODE_PAGE_NODES;
    static constexpr std::size_t STRING_PAGE_BYTES = JESON_COMPACT_STRING_PAGE_BYTES;
    static_assert(NODE_PAGE_NODES > 0);
    static_assert(STRING_PAGE_BYTES > 0);

    struct Node {
        std::uint64_t data;
        std::uint32_t firstChild;
        std::uint32_t nextSibling;
        std::uint32_t keyOffset;
        std::uint32_t meta;
    };
    static_assert(sizeof(Node) == 24, "CompactDocument node layout changed");

    class StringPages {
    public:
        void clear() {
            m_pages.clear();
            m_large.clear();
            m_usedBytes = 0;
            m_largeReservedBytes = 0;
            m_pagePosition = 0;
        }

        [[nodiscard]] std::size_t usedBytes() const { return m_usedBytes; }
        [[nodiscard]] std::size_t reservedBytes() const {
            return m_pages.size() * STRING_PAGE_BYTES + m_largeReservedBytes;
        }

        bool append(std::string_view t_text, std::uint32_t& t_offset) {
            if (t_text.empty()) {
                t_offset = 0;
                return true;
            }
            if (t_text.size() > STRING_PAGE_BYTES) {
                if (m_large.size() >= 0x7fffffffU) return false;
                auto storage = std::unique_ptr<char[]>(new char[t_text.size()]);
                std::memcpy(storage.get(), t_text.data(), t_text.size());
                t_offset = 0x80000000U | static_cast<std::uint32_t>(m_large.size());
                m_large.push_back(std::move(storage));
                m_usedBytes += t_text.size();
                m_largeReservedBytes += t_text.size();
                return true;
            }
            if (m_pages.empty() || m_pagePosition + t_text.size() > STRING_PAGE_BYTES) {
                m_pages.push_back(std::unique_ptr<char[]>(new char[STRING_PAGE_BYTES]));
                m_pagePosition = 0;
            }
            t_offset = static_cast<std::uint32_t>((m_pages.size() - 1) * STRING_PAGE_BYTES + m_pagePosition);
            std::memcpy(m_pages.back().get() + m_pagePosition, t_text.data(), t_text.size());
            m_pagePosition += t_text.size();
            m_usedBytes += t_text.size();
            return true;
        }

        [[nodiscard]] std::string_view at(std::uint32_t t_offset, std::uint32_t t_length) const {
            if (t_length == 0) return {};
            if ((t_offset & 0x80000000U) != 0) {
                return {m_large[t_offset & 0x7fffffffU].get(), t_length};
            }
            const std::size_t page = t_offset / STRING_PAGE_BYTES;
            const std::size_t position = t_offset % STRING_PAGE_BYTES;
            return {m_pages[page].get() + position, t_length};
        }

    private:
        std::vector<std::unique_ptr<char[]>> m_pages;
        std::vector<std::unique_ptr<char[]>> m_large;
        std::size_t m_usedBytes = 0;
        std::size_t m_largeReservedBytes = 0;
        std::size_t m_pagePosition = 0;
    };

    [[nodiscard]] Node& node(std::uint32_t t_index) {
        return m_nodePages[t_index / NODE_PAGE_NODES][t_index % NODE_PAGE_NODES];
    }
    [[nodiscard]] const Node& node(std::uint32_t t_index) const {
        return m_nodePages[t_index / NODE_PAGE_NODES][t_index % NODE_PAGE_NODES];
    }

    [[nodiscard]] static CompactType typeOf(const Node& t_node) {
        return static_cast<CompactType>(t_node.meta & 0xffU);
    }
    [[nodiscard]] static std::uint32_t keyLengthOf(const Node& t_node) { return t_node.meta >> 8U; }
    [[nodiscard]] static std::uint32_t low(std::uint64_t t_value) { return static_cast<std::uint32_t>(t_value); }
    [[nodiscard]] static std::uint32_t high(std::uint64_t t_value) { return static_cast<std::uint32_t>(t_value >> 32U); }
    [[nodiscard]] static std::uint64_t pair(std::uint32_t t_low, std::uint32_t t_high) {
        return static_cast<std::uint64_t>(t_low) | (static_cast<std::uint64_t>(t_high) << 32U);
    }

    template <typename T>
    [[nodiscard]] static std::uint64_t bits(T t_value) {
        static_assert(sizeof(T) == sizeof(std::uint64_t));
        std::uint64_t result = 0;
        std::memcpy(&result, &t_value, sizeof(result));
        return result;
    }

    [[nodiscard]] std::uint32_t addNode(CompactType t_type,
                                        std::uint32_t t_keyOffset,
                                        std::uint32_t t_keyLength,
                                        std::uint64_t t_data = 0) {
        if (t_keyLength > 0x00ffffffU || m_nodeCount >= NONE) return NONE;
        if (m_nodeCount % NODE_PAGE_NODES == 0) {
            m_nodePages.push_back(std::unique_ptr<Node[]>(new Node[NODE_PAGE_NODES]));
        }
        const std::uint32_t index = static_cast<std::uint32_t>(m_nodeCount++);
        Node& result = node(index);
        result.data = t_data;
        result.firstChild = NONE;
        result.nextSibling = NONE;
        result.keyOffset = t_keyOffset;
        result.meta = (t_keyLength << 8U) | static_cast<std::uint8_t>(t_type);
        return index;
    }

    void appendChild(std::uint32_t t_parentIndex, std::uint32_t t_childIndex) {
        Node& parent = node(t_parentIndex);
        const std::uint32_t count = high(parent.data);
        if (count == 0) {
            parent.firstChild = t_childIndex;
        } else {
            node(low(parent.data)).nextSibling = t_childIndex;
        }
        parent.data = pair(t_childIndex, count + 1);
    }

    void skipWhitespace() {
        while (m_position < m_input.size()) {
            const char c = m_input[m_position];
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') break;
            ++m_position;
        }
    }
    bool consume(char t_expected) {
        if (m_position >= m_input.size() || m_input[m_position] != t_expected) return false;
        ++m_position;
        return true;
    }
    bool consume(std::string_view t_expected) {
        if (m_input.substr(m_position, t_expected.size()) != t_expected) return false;
        m_position += t_expected.size();
        return true;
    }

    [[nodiscard]] static bool isDigit(char t_c) { return t_c >= '0' && t_c <= '9'; }
    [[nodiscard]] static bool validNumber(std::string_view t_text) {
        if (t_text.empty()) return false;
        std::size_t i = 0;
        if (t_text[i] == '-') {
            ++i;
            if (i == t_text.size()) return false;
        }
        if (t_text[i] == '0') {
            ++i;
            if (i < t_text.size() && isDigit(t_text[i])) return false;
        } else if (t_text[i] >= '1' && t_text[i] <= '9') {
            do { ++i; } while (i < t_text.size() && isDigit(t_text[i]));
        } else {
            return false;
        }
        if (i < t_text.size() && t_text[i] == '.') {
            ++i;
            const std::size_t start = i;
            while (i < t_text.size() && isDigit(t_text[i])) ++i;
            if (i == start) return false;
        }
        if (i < t_text.size() && (t_text[i] == 'e' || t_text[i] == 'E')) {
            ++i;
            if (i < t_text.size() && (t_text[i] == '+' || t_text[i] == '-')) ++i;
            const std::size_t start = i;
            while (i < t_text.size() && isDigit(t_text[i])) ++i;
            if (i == start) return false;
        }
        return i == t_text.size();
    }

    [[nodiscard]] static int hexValue(char t_c) {
        if (t_c >= '0' && t_c <= '9') return t_c - '0';
        if (t_c >= 'a' && t_c <= 'f') return t_c - 'a' + 10;
        if (t_c >= 'A' && t_c <= 'F') return t_c - 'A' + 10;
        return -1;
    }
    bool readHex4(std::size_t t_start, std::uint32_t& t_output) const {
        if (t_start + 4 > m_input.size()) return false;
        t_output = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            const int digit = hexValue(m_input[t_start + i]);
            if (digit < 0) return false;
            t_output = (t_output << 4U) | static_cast<std::uint32_t>(digit);
        }
        return true;
    }
    static void appendUtf8(std::uint32_t t_codePoint, std::string& t_output) {
        if (t_codePoint <= 0x7f) {
            t_output += static_cast<char>(t_codePoint);
        } else if (t_codePoint <= 0x7ff) {
            t_output += static_cast<char>(0xc0 | (t_codePoint >> 6U));
            t_output += static_cast<char>(0x80 | (t_codePoint & 0x3f));
        } else if (t_codePoint <= 0xffff) {
            t_output += static_cast<char>(0xe0 | (t_codePoint >> 12U));
            t_output += static_cast<char>(0x80 | ((t_codePoint >> 6U) & 0x3f));
            t_output += static_cast<char>(0x80 | (t_codePoint & 0x3f));
        } else {
            t_output += static_cast<char>(0xf0 | (t_codePoint >> 18U));
            t_output += static_cast<char>(0x80 | ((t_codePoint >> 12U) & 0x3f));
            t_output += static_cast<char>(0x80 | ((t_codePoint >> 6U) & 0x3f));
            t_output += static_cast<char>(0x80 | (t_codePoint & 0x3f));
        }
    }

    bool parseString(std::uint32_t& t_offset, std::uint32_t& t_length) {
        if (!consume('"')) return false;
        const std::size_t start = m_position;
        while (m_position < m_input.size()) {
            const char c = m_input[m_position];
            if (c == '"') {
                const std::string_view raw = m_input.substr(start, m_position - start);
                ++m_position;
                if (raw.size() > std::numeric_limits<std::uint32_t>::max() || !m_strings.append(raw, t_offset)) return false;
                t_length = static_cast<std::uint32_t>(raw.size());
                return true;
            }
            if (c == '\\' || static_cast<unsigned char>(c) < 0x20) break;
            ++m_position;
        }

        std::string decoded(m_input.substr(start, m_position - start));
        while (m_position < m_input.size() && m_input[m_position] != '"') {
            char c = m_input[m_position];
            if (c == '\\' && m_position + 1 < m_input.size()) {
                ++m_position;
                switch (m_input[m_position]) {
                    case 'b': decoded += '\b'; break;
                    case 'f': decoded += '\f'; break;
                    case 'n': decoded += '\n'; break;
                    case 'r': decoded += '\r'; break;
                    case 't': decoded += '\t'; break;
                    case '"': decoded += '"'; break;
                    case '\\': decoded += '\\'; break;
                    case '/': decoded += '/'; break;
                    case 'u': {
                        std::uint32_t codePoint = 0;
                        if (!readHex4(m_position + 1, codePoint)) return false;
                        if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
                            if (m_position + 10 >= m_input.size() || m_input[m_position + 5] != '\\' || m_input[m_position + 6] != 'u') return false;
                            std::uint32_t lowSurrogate = 0;
                            if (!readHex4(m_position + 7, lowSurrogate) || lowSurrogate < 0xdc00 || lowSurrogate > 0xdfff) return false;
                            codePoint = 0x10000 + ((codePoint - 0xd800) << 10U) + (lowSurrogate - 0xdc00);
                            m_position += 10;
                        } else {
                            if (codePoint >= 0xdc00 && codePoint <= 0xdfff) return false;
                            m_position += 4;
                        }
                        appendUtf8(codePoint, decoded);
                        break;
                    }
                    default: return false;
                }
            } else {
                if (static_cast<unsigned char>(c) < 0x20) return false;
                decoded += c;
            }
            ++m_position;
        }
        if (!consume('"') || decoded.size() > std::numeric_limits<std::uint32_t>::max()) return false;
        if (!m_strings.append(decoded, t_offset)) return false;
        t_length = static_cast<std::uint32_t>(decoded.size());
        return true;
    }

    [[nodiscard]] std::uint32_t parseValue(std::uint32_t t_keyOffset, std::uint32_t t_keyLength, unsigned t_depth) {
        if (t_depth > 512) return NONE;
        skipWhitespace();

        if (consume("null")) return addNode(CompactType::NULL_VALUE, t_keyOffset, t_keyLength);
        if (consume("true")) return addNode(CompactType::BOOL, t_keyOffset, t_keyLength, 1);
        if (consume("false")) return addNode(CompactType::BOOL, t_keyOffset, t_keyLength);

        if (m_position < m_input.size() && m_input[m_position] == '"') {
            std::uint32_t offset = 0;
            std::uint32_t length = 0;
            if (!parseString(offset, length)) return NONE;
            return addNode(CompactType::STRING, t_keyOffset, t_keyLength, pair(offset, length));
        }

        if (consume('[')) {
            const std::uint32_t index = addNode(CompactType::ARRAY, t_keyOffset, t_keyLength);
            if (index == NONE) return NONE;
            skipWhitespace();
            if (consume(']')) return index;
            while (true) {
                const std::uint32_t child = parseValue(0, 0, t_depth + 1);
                if (child == NONE) return NONE;
                appendChild(index, child);
                skipWhitespace();
                if (consume(',')) {
                    skipWhitespace();
                    if (m_position < m_input.size() && m_input[m_position] == ']') return NONE;
                    continue;
                }
                if (!consume(']')) return NONE;
                return index;
            }
        }

        if (consume('{')) {
            const std::uint32_t index = addNode(CompactType::OBJECT, t_keyOffset, t_keyLength);
            if (index == NONE) return NONE;
            skipWhitespace();
            if (consume('}')) return index;
            while (true) {
                std::uint32_t memberKeyOffset = 0;
                std::uint32_t memberKeyLength = 0;
                if (!parseString(memberKeyOffset, memberKeyLength)) return NONE;
                skipWhitespace();
                if (!consume(':')) return NONE;
                const std::uint32_t child = parseValue(memberKeyOffset, memberKeyLength, t_depth + 1);
                if (child == NONE) return NONE;
                appendChild(index, child);
                skipWhitespace();
                if (consume(',')) {
                    skipWhitespace();
                    if (m_position < m_input.size() && m_input[m_position] == '}') return NONE;
                    continue;
                }
                if (!consume('}')) return NONE;
                return index;
            }
        }

        const std::size_t start = m_position;
        while (m_position < m_input.size()) {
            const char c = m_input[m_position];
            if (!isDigit(c) && c != '-' && c != '+' && c != '.' && c != 'e' && c != 'E') break;
            ++m_position;
        }
        const std::string_view number = m_input.substr(start, m_position - start);
        if (!validNumber(number)) return NONE;

        if (number.find_first_of(".eE") != std::string_view::npos) {
            double value = 0;
            if (!parseDouble(number, value)) return NONE;
            return addNode(CompactType::DOUBLE, t_keyOffset, t_keyLength, bits(value));
        }
        if (number.front() == '-') {
            std::int64_t value = 0;
            const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
            if (result.ec == std::errc() && result.ptr == number.data() + number.size()) {
                return addNode(CompactType::INT, t_keyOffset, t_keyLength, bits(value));
            }
        } else {
            std::uint64_t value = 0;
            const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
            if (result.ec == std::errc() && result.ptr == number.data() + number.size()) {
                return addNode(CompactType::UINT, t_keyOffset, t_keyLength, value);
            }
        }
        double wideValue = 0;
        if (!parseDouble(number, wideValue)) return NONE;
        return addNode(CompactType::DOUBLE, t_keyOffset, t_keyLength, bits(wideValue));
    }

    [[nodiscard]] static bool parseDouble(std::string_view t_number, double& t_output) {
        char stack[128];
        std::string fallback;
        const char* text = nullptr;
        if (t_number.size() < sizeof(stack)) {
            std::memcpy(stack, t_number.data(), t_number.size());
            stack[t_number.size()] = '\0';
            text = stack;
        } else {
            fallback.assign(t_number);
            text = fallback.c_str();
        }
        char* end = nullptr;
        t_output = std::strtod(text, &end);
        return end == text + t_number.size() && std::isfinite(t_output);
    }

    void collect(std::uint32_t t_index, CompactStatistics& t_result) const {
        const Node& current = node(t_index);
        switch (typeOf(current)) {
            case CompactType::NULL_VALUE: ++t_result.nullCount; break;
            case CompactType::BOOL: current.data ? ++t_result.trueCount : ++t_result.falseCount; break;
            case CompactType::INT:
            case CompactType::UINT:
            case CompactType::DOUBLE: ++t_result.numberCount; break;
            case CompactType::STRING:
                ++t_result.stringCount;
                t_result.stringLength += high(current.data);
                break;
            case CompactType::ARRAY:
                ++t_result.arrayCount;
                t_result.elementCount += high(current.data);
                break;
            case CompactType::OBJECT:
                ++t_result.objectCount;
                t_result.memberCount += high(current.data);
                break;
            case CompactType::INVALID: return;
        }
        if (typeOf(current) == CompactType::ARRAY || typeOf(current) == CompactType::OBJECT) {
            std::uint32_t child = current.firstChild;
            while (child != NONE) {
                if (typeOf(current) == CompactType::OBJECT) {
                    ++t_result.stringCount;
                    t_result.stringLength += keyLengthOf(node(child));
                }
                collect(child, t_result);
                child = node(child).nextSibling;
            }
        }
    }

    static void writeIndent(std::string& t_output, const WriteOptions& t_options, unsigned t_depth) {
        if (!t_options.pretty) return;
        t_output += '\n';
        t_output.append(static_cast<std::size_t>(t_depth) * t_options.indentBy, ' ');
    }
    static void writeEscaped(std::string_view t_input, std::string& t_output) {
        t_output += '"';
        for (char c : t_input) {
            switch (c) {
                case '"': t_output += "\\\""; break;
                case '\\': t_output += "\\\\"; break;
                case '\b': t_output += "\\b"; break;
                case '\f': t_output += "\\f"; break;
                case '\n': t_output += "\\n"; break;
                case '\r': t_output += "\\r"; break;
                case '\t': t_output += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        constexpr char hex[] = "0123456789abcdef";
                        const auto value = static_cast<unsigned char>(c);
                        t_output += "\\u00";
                        t_output += hex[(value >> 4U) & 0x0fU];
                        t_output += hex[value & 0x0fU];
                    } else {
                        t_output += c;
                    }
            }
        }
        t_output += '"';
    }
    template <typename Integer>
    static void writeInteger(Integer t_value, std::string& t_output) {
        char buffer[32];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), t_value);
        if (result.ec == std::errc()) t_output.append(buffer, result.ptr);
    }
    static void writeDouble(double t_value, std::string& t_output) {
        char buffer[64];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), t_value, std::chars_format::general);
        if (result.ec != std::errc()) {
            const int length = std::snprintf(buffer, sizeof(buffer), "%.17g", t_value);
            if (length > 0) t_output.append(buffer, static_cast<std::size_t>(length));
            return;
        }
        const std::string_view formatted(buffer, static_cast<std::size_t>(result.ptr - buffer));
        const auto exponent = formatted.find_first_of("eE");
        if (exponent != std::string_view::npos && exponent + 1 < formatted.size() && formatted[exponent + 1] == '+') {
            t_output.append(formatted.substr(0, exponent + 1));
            t_output.append(formatted.substr(exponent + 2));
        } else {
            t_output.append(formatted);
        }
        if (formatted.find_first_of(".eE") == std::string_view::npos) t_output += ".0";
    }
    void write(std::uint32_t t_index, std::string& t_output, const WriteOptions& t_options, unsigned t_depth) const {
        const Node& current = node(t_index);
        const CompactType currentType = typeOf(current);
        switch (currentType) {
            case CompactType::NULL_VALUE: t_output += "null"; return;
            case CompactType::BOOL: t_output += current.data ? "true" : "false"; return;
            case CompactType::INT: {
                std::int64_t value = 0;
                std::memcpy(&value, &current.data, sizeof(value));
                writeInteger(value, t_output);
                return;
            }
            case CompactType::UINT: writeInteger(current.data, t_output); return;
            case CompactType::DOUBLE: {
                double value = 0;
                std::memcpy(&value, &current.data, sizeof(value));
                writeDouble(value, t_output);
                return;
            }
            case CompactType::STRING:
                writeEscaped(m_strings.at(low(current.data), high(current.data)), t_output);
                return;
            case CompactType::ARRAY: t_output += '['; break;
            case CompactType::OBJECT: t_output += '{'; break;
            case CompactType::INVALID: return;
        }
        std::uint32_t child = current.firstChild;
        bool first = true;
        while (child != NONE) {
            if (!first) t_output += ',';
            first = false;
            writeIndent(t_output, t_options, t_depth + 1);
            const Node& item = node(child);
            if (currentType == CompactType::OBJECT) {
                writeEscaped(m_strings.at(item.keyOffset, keyLengthOf(item)), t_output);
                t_output += ':';
                if (t_options.pretty) t_output += ' ';
            }
            write(child, t_output, t_options, t_depth + 1);
            child = item.nextSibling;
        }
        if (!first) writeIndent(t_output, t_options, t_depth);
        t_output += currentType == CompactType::ARRAY ? ']' : '}';
    }

    std::vector<std::unique_ptr<Node[]>> m_nodePages;
    StringPages m_strings;
    std::string_view m_input;
    std::size_t m_inputSize = 0;
    std::size_t m_position = 0;
    std::size_t m_nodeCount = 0;
    std::uint32_t m_root = NONE;
};

/** A small, non-owning view of one CompactDocument value. */
class CompactValue {
public:
    CompactValue() = default;

    [[nodiscard]] bool valid() const { return m_document != nullptr && m_index != CompactDocument::NONE; }
    explicit operator bool() const { return valid(); }
    [[nodiscard]] CompactType type() const {
        return valid() ? CompactDocument::typeOf(m_document->node(m_index)) : CompactType::INVALID;
    }
    [[nodiscard]] bool isNull() const { return type() == CompactType::NULL_VALUE; }
    [[nodiscard]] bool isBool() const { return type() == CompactType::BOOL; }
    [[nodiscard]] bool isNumber() const {
        return type() == CompactType::INT || type() == CompactType::UINT || type() == CompactType::DOUBLE;
    }
    [[nodiscard]] bool isString() const { return type() == CompactType::STRING; }
    [[nodiscard]] bool isArray() const { return type() == CompactType::ARRAY; }
    [[nodiscard]] bool isObject() const { return type() == CompactType::OBJECT; }

    [[nodiscard]] bool boolValue(bool t_fallback = false) const {
        return isBool() ? m_document->node(m_index).data != 0 : t_fallback;
    }
    [[nodiscard]] std::int64_t int64Value(std::int64_t t_fallback = 0) const {
        if (type() != CompactType::INT) return t_fallback;
        std::int64_t result = 0;
        const auto data = m_document->node(m_index).data;
        std::memcpy(&result, &data, sizeof(result));
        return result;
    }
    [[nodiscard]] std::uint64_t uint64Value(std::uint64_t t_fallback = 0) const {
        return type() == CompactType::UINT ? m_document->node(m_index).data : t_fallback;
    }
    [[nodiscard]] double doubleValue(double t_fallback = 0.0) const {
        const CompactType currentType = type();
        const auto data = valid() ? m_document->node(m_index).data : 0;
        if (currentType == CompactType::INT) {
            std::int64_t value = 0;
            std::memcpy(&value, &data, sizeof(value));
            return static_cast<double>(value);
        }
        if (currentType == CompactType::UINT) return static_cast<double>(data);
        if (currentType == CompactType::DOUBLE) {
            double value = 0;
            std::memcpy(&value, &data, sizeof(value));
            return value;
        }
        return t_fallback;
    }
    [[nodiscard]] std::string_view stringValue() const {
        if (!isString()) return {};
        const auto data = m_document->node(m_index).data;
        return m_document->m_strings.at(CompactDocument::low(data), CompactDocument::high(data));
    }
    [[nodiscard]] std::string_view key() const {
        if (!valid()) return {};
        const auto& current = m_document->node(m_index);
        return m_document->m_strings.at(current.keyOffset, CompactDocument::keyLengthOf(current));
    }
    [[nodiscard]] std::size_t size() const {
        return (isArray() || isObject()) ? CompactDocument::high(m_document->node(m_index).data) : 0;
    }

    [[nodiscard]] CompactValue operator[](std::size_t t_index) const {
        if (!isArray()) return {};
        std::uint32_t child = m_document->node(m_index).firstChild;
        while (child != CompactDocument::NONE && t_index != 0) {
            child = m_document->node(child).nextSibling;
            --t_index;
        }
        return child == CompactDocument::NONE ? CompactValue{} : CompactValue(m_document, child);
    }
    [[nodiscard]] CompactValue operator[](std::string_view t_key) const {
        if (!isObject()) return {};
        std::uint32_t child = m_document->node(m_index).firstChild;
        while (child != CompactDocument::NONE) {
            CompactValue value(m_document, child);
            if (value.key() == t_key) return value;
            child = m_document->node(child).nextSibling;
        }
        return {};
    }

    [[nodiscard]] CompactChildren children() const;
    [[nodiscard]] std::string toJson(WriteOptions t_options = {}) const {
        std::string output;
        if (valid()) m_document->write(m_index, output, t_options, 0);
        return output;
    }

private:
    friend class CompactDocument;
    friend class CompactChildren;
    CompactValue(const CompactDocument* t_document, std::uint32_t t_index) : m_document(t_document), m_index(t_index) {}

    const CompactDocument* m_document = nullptr;
    std::uint32_t m_index = CompactDocument::NONE;
};

/** Iterable child range; object children expose their member name through CompactValue::key(). */
class CompactChildren {
public:
    class Iterator {
    public:
        using value_type = CompactValue;
        using difference_type = std::ptrdiff_t;

        [[nodiscard]] CompactValue operator*() const { return CompactValue(m_document, m_index); }
        Iterator& operator++() {
            if (m_index != CompactDocument::NONE) m_index = m_document->node(m_index).nextSibling;
            return *this;
        }
        [[nodiscard]] bool operator==(const Iterator& t_other) const {
            return m_document == t_other.m_document && m_index == t_other.m_index;
        }
        [[nodiscard]] bool operator!=(const Iterator& t_other) const { return !(*this == t_other); }

    private:
        friend class CompactChildren;
        Iterator(const CompactDocument* t_document, std::uint32_t t_index) : m_document(t_document), m_index(t_index) {}
        const CompactDocument* m_document = nullptr;
        std::uint32_t m_index = CompactDocument::NONE;
    };

    [[nodiscard]] Iterator begin() const { return Iterator(m_document, m_first); }
    [[nodiscard]] Iterator end() const { return Iterator(m_document, CompactDocument::NONE); }

private:
    friend class CompactValue;
    CompactChildren(const CompactDocument* t_document, std::uint32_t t_first) : m_document(t_document), m_first(t_first) {}
    const CompactDocument* m_document = nullptr;
    std::uint32_t m_first = CompactDocument::NONE;
};

inline CompactValue CompactDocument::root() const {
    return m_root == NONE ? CompactValue{} : CompactValue(this, m_root);
}

inline CompactChildren CompactValue::children() const {
    if (!isArray() && !isObject()) return CompactChildren(nullptr, CompactDocument::NONE);
    return CompactChildren(m_document, m_document->node(m_index).firstChild);
}

} // namespace jeson
