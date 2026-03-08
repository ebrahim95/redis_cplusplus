/**
 * @file resp_deserializer.cpp
 * @brief Parses raw RESP (Redis Serialization Protocol) bytes into plain C++ strings.
 *
 * RESP uses a single prefix byte to declare the type of the value that follows.
 * This file implements a recursive-descent style parser with one dedicated
 * function per RESP type. The public entry point is de_serial(), which inspects
 * the prefix byte and dispatches to the appropriate parser.
 *
 * Supported RESP types:
 *   '+'  Simple string  →  the string content
 *   '-'  Error          →  the error message
 *   ':'  Integer        →  the number as a string
 *   '$'  Bulk string    →  the string content, "(null)", or "(empty)"
 *   '*'  Array          →  space-separated concatenation of all element values
 *
 * All internal helpers are file-scoped (static) and not visible outside
 * this translation unit.
 */

#include "resp_deserializer.h"
#include <string>
#include <vector>
#include <stdexcept>
using namespace std;


// =============================================================================
// § 1. Internal helpers
// =============================================================================

/**
 * @brief Extracts a single line of content from a RESP message.
 *
 * Reads from @p start up to (but not including) the first \\r\\n. Used for
 * simple types (+, -, :) whose entire value fits on one line.
 *
 * @param data   The full raw RESP string.
 * @param start  Index of the first content character (one past the type prefix).
 * @return       The content between @p start and the first \\r\\n, or the
 *               remainder of @p data if no \\r\\n is found.
 *
 * @example
 *   read_line("+OK\r\n", 1)  →  "OK"
 *   read_line("-ERR\r\n", 1) →  "ERR"
 */
static string read_line(const string& data, size_t start) {
    size_t end = data.find("\r\n", start);
    if (end == string::npos) return data.substr(start);
    return data.substr(start, end - start);
}

/**
 * @brief Parses a RESP bulk string and advances the position cursor.
 *
 * A bulk string is length-prefixed, allowing it to contain arbitrary bytes
 * including spaces and \\r\\n sequences. The format is:
 * @code
 *   $<length>\r\n<content>\r\n
 * @endcode
 *
 * Special cases:
 *   - Length -1 ($-1\\r\\n) is a null bulk string — the key does not exist.
 *   - Length  0 ($0\\r\\n\\r\\n) is an empty bulk string — the key exists but
 *     holds an empty value. These are semantically distinct in Redis.
 *
 * @param data  The full raw RESP string.
 * @param pos   On entry: index of the '$' character.
 *              On exit:  index of the first byte after the trailing \\r\\n,
 *                        ready for the next element to be parsed.
 * @return      The string content, "(null)", or "(empty)".
 * @throws      std::runtime_error if the length field is missing or malformed.
 */
static string parse_bulk_string(const string& data, size_t& pos) {
    size_t crlf = data.find("\r\n", pos);
    if (crlf == string::npos) throw runtime_error("malformed bulk string length");

    int length = stoi(data.substr(pos + 1, crlf - pos - 1));

    /// Null bulk string: $-1\r\n — the value does not exist.
    if (length == -1) {
        pos = crlf + 2;
        return "(null)";
    }

    /// Empty bulk string: $0\r\n\r\n — the value exists but has no content.
    if (length == 0) {
        pos = crlf + 4; ///< +4 skips the length-line \r\n and the content-line \r\n.
        return "(empty)";
    }

    size_t content_start = crlf + 2; ///< Content begins immediately after the first \r\n.
    string content = data.substr(content_start, length);

    pos = content_start + length + 2; ///< Advance past content and its trailing \r\n.
    return content;
}

/**
 * @brief Parses a RESP integer element and advances the position cursor.
 *
 * Format: @code :<number>\r\n @endcode
 *
 * The integer is returned as a string rather than an int because de_serial()
 * produces a unified string output for all types. The caller can convert with
 * stoi() if a numeric value is needed.
 *
 * @param data  The full raw RESP string.
 * @param pos   On entry: index of the ':' character.
 *              On exit:  index of the first byte after the trailing \\r\\n.
 * @return      The integer digits as a string, e.g. "42".
 * @throws      std::runtime_error if no \\r\\n terminator is found.
 */
static string parse_integer(const string& data, size_t& pos) {
    size_t crlf = data.find("\r\n", pos);
    if (crlf == string::npos) throw runtime_error("malformed integer");

    string value = data.substr(pos + 1, crlf - pos - 1);
    pos = crlf + 2;
    return value;
}

/**
 * @brief Parses a RESP array and advances the position cursor.
 *
 * An array is a sequence of @p count RESP elements of any type. The format is:
 * @code
 *   *<count>\r\n  <element_1> <element_2> ... <element_n>
 * @endcode
 * where each element is itself a fully-formed RESP value.
 *
 * This function handles nested types by delegating each element to the
 * appropriate sub-parser (parse_bulk_string, parse_integer, read_line).
 * Each sub-parser advances @p pos, so the loop always starts at the correct
 * position for the next element.
 *
 * The returned string concatenates all element values separated by spaces.
 * The caller (redis_server.cpp) strips the trailing space after de_serial().
 *
 * @param data  The full raw RESP string.
 * @param pos   On entry: index of the '*' character.
 *              On exit:  index of the first byte after the entire array.
 * @return      Space-separated element values, e.g. "echo hello ".
 * @throws      std::runtime_error if the header is malformed, the array is
 *              shorter than its declared count, or an element type is unsupported.
 */
static string parse_array(const string& data, size_t& pos) {
    size_t crlf = data.find("\r\n", pos);
    if (crlf == string::npos) throw runtime_error("malformed array header");

    int count = stoi(data.substr(pos + 1, crlf - pos - 1));

    /// Null array: *-1\r\n
    if (count == -1) {
        pos = crlf + 2;
        return "(null)";
    }

    pos = crlf + 2; ///< Advance past the *<count>\r\n header.

    string result;

    for (int i = 0; i < count; i++) {
        if (pos >= data.size()) throw runtime_error("array shorter than declared count");

        char type = data[pos];

        string element;
        if (type == '$') {
            element = parse_bulk_string(data, pos);
        } else if (type == ':') {
            element = parse_integer(data, pos);
        } else if (type == '+' || type == '-') {
            /// Simple string or error nested inside an array — uncommon but valid RESP.
            element = read_line(data, pos + 1);
            pos = data.find("\r\n", pos) + 2;
        } else {
            throw runtime_error(string("unsupported array element type: ") + type);
        }

        result += element + " "; ///< Space acts as separator between tokens.
    }

    return result;
}


// =============================================================================
// § 2. Public entry point
// =============================================================================

/**
 * @brief Deserializes a raw RESP message into a plain-text command string.
 *
 * Inspects the first byte of @p data to determine the RESP type, then
 * dispatches to the appropriate internal parser. This is the only function
 * declared in resp_deserializer.h and visible to other translation units.
 *
 * @param data  A complete, well-formed RESP message ending with \\r\\n.
 * @return      The deserialized content as a plain string:
 *              - Simple/error/integer: the value on the first line.
 *              - Bulk string: the content, "(null)", or "(empty)".
 *              - Array: space-separated element values with a trailing space.
 *              - Empty input: "".
 *              - Unknown type prefix: "none".
 *
 * @example
 *   de_serial("+OK\r\n")                              →  "OK"
 *   de_serial("$5\r\nhello\r\n")                      →  "hello"
 *   de_serial("*2\r\n$4\r\necho\r\n$5\r\nhello\r\n") →  "echo hello "
 *   de_serial("$-1\r\n")                              →  "(null)"
 */
string de_serial(const string& data) {
    if (data.empty()) return "";

    char type = data[0];
    size_t pos = 0; ///< Cursor passed by reference into multi-line parsers.

    switch (type) {
        case '+': return read_line(data, 1);        ///< Simple string
        case '-': return read_line(data, 1);        ///< Error
        case ':': return read_line(data, 1);        ///< Integer
        case '$': return parse_bulk_string(data, pos); ///< Bulk string
        case '*': return parse_array(data, pos);       ///< Array
        default:  return "none";
    }
}