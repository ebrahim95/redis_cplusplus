/**
 * @file resp_serializer.cpp
 * @brief Converts plain C++ values into RESP (Redis Serialization Protocol) wire format.
 *
 * Every function in this file takes a normal C++ value and wraps it in the
 * RESP framing that Redis clients expect. This is the inverse operation of
 * resp_deserializer.cpp.
 *
 * RESP type prefixes produced by this file:
 *   '+'  Simple string  →  +<value>\r\n
 *   '-'  Error          →  -<message>\r\n
 *   ':'  Integer        →  :<number>\r\n
 *   '$'  Bulk string    →  $<length>\r\n<content>\r\n
 *   '*'  Array          →  *<count>\r\n followed by <count> serialized elements
 *
 * Internal helpers (is_integer, split) are file-scoped (static) and not
 * visible outside this translation unit.
 */

#include "resp_serializer.h"
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
using namespace std;


// =============================================================================
// § 1. Debug utility
// =============================================================================

/**
 * @brief Prints a RESP string to stdout with \\r and \\n shown as visible text.
 *
 * Raw \\r\\n bytes are invisible in a terminal and \\r alone moves the cursor
 * to the start of the line, corrupting the display. This function renders them
 * as the literal four-character sequences "\\r" and "\\n" so the full wire
 * format is readable during debugging.
 *
 * @param value  Any RESP-formatted string to print.
 *
 * @example
 *   raw_string_print("+PONG\r\n")  →  prints: +PONG\r\n
 */
void raw_string_print(const string& value) {
    for (char c : value) {
        if      (c == '\r') cout << "\\r";
        else if (c == '\n') cout << "\\n";
        else                cout << c;
    }
    cout << endl;
}


// =============================================================================
// § 2. Simple serializers
// =============================================================================

/**
 * @brief Serializes a value as a RESP simple string.
 *
 * Simple strings are for short status responses that contain no binary data
 * and no \\r or \\n characters. Use serialize_bulk_string() for arbitrary content.
 *
 * @param value  The status message, e.g. "OK", "PONG".
 * @return       RESP simple string: +<value>\\r\\n
 *
 * @example
 *   serialize_simple_string("PONG")  →  "+PONG\r\n"
 */
string serialize_simple_string(const string& value) {
    return "+" + value + "\r\n";
}

/**
 * @brief Serializes a message as a RESP error response.
 *
 * Error responses signal failure to the client. By Redis convention the
 * message begins with an error type in capitals, e.g. "ERR", "WRONGTYPE",
 * followed by a human-readable description.
 *
 * @param value  The error message, e.g. "ERR unknown command".
 * @return       RESP error: -<value>\\r\\n
 *
 * @example
 *   serialize_error_string("ERR unknown command")  →  "-ERR unknown command\r\n"
 */
string serialize_error_string(const string& value) {
    return "-" + value + "\r\n";
}

/**
 * @brief Serializes an integer as a RESP integer response.
 *
 * Used for commands that return a numeric result, such as INCR, DEL, LLEN.
 *
 * @param value  The integer to serialize.
 * @return       RESP integer: :<value>\\r\\n
 *
 * @example
 *   serialize_int(42)  →  ":42\r\n"
 */
string serialize_int(const int& value) {
    return ":" + to_string(value) + "\r\n";
}

/**
 * @brief Serializes a string as a RESP bulk string.
 *
 * Bulk strings carry an explicit byte count, making them safe for arbitrary
 * content including spaces, \\r\\n sequences, and binary data. This is the
 * correct type for GET, ECHO, and most string-valued responses.
 *
 * @param value  The string content to serialize.
 * @return       RESP bulk string: $<length>\\r\\n<content>\\r\\n
 *
 * @example
 *   serialize_bulk_string("hello")  →  "$5\r\nhello\r\n"
 *   serialize_bulk_string("")       →  "$0\r\n\r\n"
 */
string serialize_bulk_string(const string& value) {
    return "$" + to_string(value.size()) + "\r\n" + value + "\r\n";
}


// =============================================================================
// § 3. Array serializer
// =============================================================================

/**
 * @brief Returns true if @p value represents a valid integer in its entirety.
 *
 * stoi() on its own is insufficient because stoi("123abc") succeeds by
 * consuming only the leading digits and ignoring the rest. We verify that
 * the number of characters consumed equals the full string length.
 *
 * @param value  The string to test.
 * @return       true if @p value can be fully parsed as an integer, false otherwise.
 *
 * @example
 *   is_integer("42")     →  true
 *   is_integer("123abc") →  false
 *   is_integer("")       →  false
 */
static bool is_integer(const string& value) {
    if (value.empty()) return false;
    try {
        size_t chars_parsed;
        stoi(value, &chars_parsed);
        return chars_parsed == value.size();
    } catch (...) {
        return false;
    }
}

/**
 * @brief Splits a string into whitespace-delimited tokens.
 *
 * Uses istringstream with the >> operator, which automatically skips leading
 * and consecutive whitespace. This means "set  key  42" (extra spaces) and
 * "set key 42" produce identical token vectors.
 *
 * @param value  The space-separated input string.
 * @return       A vector of non-empty tokens in order.
 *
 * @example
 *   split("get key")    →  ["get", "key"]
 *   split("set x 42")  →  ["set", "x", "42"]
 */
static vector<string> split(const string& value) {
    vector<string> tokens;
    istringstream stream(value);
    string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * @brief Serializes a space-separated command string into a RESP array.
 *
 * Each whitespace-delimited token is examined individually:
 *   - If it represents a valid integer it is serialized as a RESP integer (:).
 *   - Otherwise it is serialized as a RESP bulk string ($).
 *
 * The resulting elements are preceded by a RESP array header (*<count>\\r\\n).
 *
 * @note This function takes @p value by const reference — no copy is made
 *       and the original string is not modified.
 *
 * @param value  A space-separated string of command tokens,
 *               e.g. "set key 42" or "get key".
 * @return       A complete RESP array string.
 *
 * @example
 *   serialize_array("set key 42")
 *     →  "*3\r\n$3\r\nset\r\n$3\r\nkey\r\n:42\r\n"
 *
 *   serialize_array("get key")
 *     →  "*2\r\n$3\r\nget\r\n$3\r\nkey\r\n"
 */
string serialize_array(const string& value) {
    vector<string> tokens = split(value);

    string elements;
    for (const string& token : tokens) {
        if (is_integer(token)) {
            elements += ":" + token + "\r\n";                               ///< RESP integer
        } else {
            elements += "$" + to_string(token.size()) + "\r\n" + token + "\r\n"; ///< RESP bulk string
        }
    }

    return "*" + to_string(tokens.size()) + "\r\n" + elements; ///< Prepend array header
}