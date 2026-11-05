/**
 * @file errors.hpp
 * @brief Accumulated parse/validation errors, serializable back to JSON.
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace jeson {

/// A single parse or validation failure.
struct ParseError {
    std::string path;    ///< JSON-pointer-ish path, e.g. "/wifi/ssid"
    std::string message; ///< Human-readable reason.
};

/**
 * @brief Accumulates every error found while parsing a document (not just
 * the first), and offers two ways to report them back out.
 */
class ParseErrors {
public:
    /**
     * @brief Records one error.
     * @param t_path JSON-pointer-ish path to the offending field.
     * @param t_message Human-readable reason.
     */
    void add(std::string t_path, std::string t_message) { m_errors.push_back({std::move(t_path), std::move(t_message)}); }

    /**
     * @brief Moves every error from t_other into this instance.
     * @param t_other The errors to absorb; left empty afterward.
     */
    void merge(ParseErrors&& t_other) {
        for (auto& e : t_other.m_errors) {
            m_errors.push_back(std::move(e));
        }
    }

    /// @return True if no errors were recorded.
    [[nodiscard]] bool ok() const { return m_errors.empty(); }
    /// @copydoc ok
    explicit operator bool() const { return ok(); }

    /// @return Every recorded error, in the order they were found.
    [[nodiscard]] const std::vector<ParseError>& list() const { return m_errors; }

    /**
     * @brief Serializes the error list as a small, stable JSON shape you
     * can hand straight back over MQTT/HTTP:
     * `{"ok":false,"errors":[{"path":...,"message":...}]}`.
     */
    [[nodiscard]] std::string toJson() const {
        std::string out;
        out += "{\"ok\":";
        out += (ok() ? "true" : "false");
        out += ",\"errors\":[";
        for (std::size_t i = 0; i < m_errors.size(); ++i) {
            if (i != 0) {
                out += ",";
            }
            out += "{\"path\":\"";
            out += escape(m_errors[i].path);
            out += "\",\"message\":\"";
            out += escape(m_errors[i].message);
            out += "\"}";
        }
        out += "]}";
        return out;
    }

    /**
     * @brief Plain, human-readable one-liner - for logs, status messages,
     * or embedding inside another JSON string field (a "message" field,
     * say) where nested JSON would just mean escaping quotes for no
     * benefit.
     *
     * Empty on success. Otherwise each error renders as "path: message"
     * (or just "message" if path is empty, e.g. a top-level parse
     * failure), joined by t_separator.
     *
     * @code
     * errors.toString()     // "/response_id: expected string; /response_id: size must be >= 1"
     * errors.toString("\n") // one error per line
     * @endcode
     *
     * @param t_separator Text inserted between consecutive errors.
     */
    [[nodiscard]] std::string toString(std::string_view t_separator = "; ") const {
        std::string out;
        for (std::size_t i = 0; i < m_errors.size(); ++i) {
            if (i != 0) {
                out += t_separator;
            }
            if (!m_errors[i].path.empty()) {
                out += m_errors[i].path;
                out += ": ";
            }
            out += m_errors[i].message;
        }
        return out;
    }

private:
    /// Escapes characters that would otherwise break toJson()'s output.
    static std::string escape(const std::string& t_in) {
        std::string out;
        out.reserve(t_in.size());
        for (char c : t_in) {
            switch (c) {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        constexpr char hex[] = "0123456789abcdef";
                        const auto uc = static_cast<unsigned char>(c);
                        out += "\\u00";
                        out += hex[(uc >> 4) & 0x0f];
                        out += hex[uc & 0x0f];
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    }

    std::vector<ParseError> m_errors;
};

} // namespace jeson
