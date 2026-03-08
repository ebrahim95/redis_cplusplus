/**
 * @file resp_deserializer.h
 * @brief Public interface for the RESP deserializer.
 *
 * This header exposes only de_serial() — the single public entry point for
 * parsing raw RESP bytes into plain text. All internal helpers (read_line,
 * parse_bulk_string, parse_integer, parse_array) are static and private to
 * resp_deserializer.cpp. They must never be declared here.
 */

#ifndef RESP_DESERIALIZER_H
#define RESP_DESERIALIZER_H

#include <string>
#include <cstddef>
using namespace std;

/**
 * @brief Deserializes a raw RESP message into a plain-text command string.
 *
 * @param data  A complete, well-formed RESP message ending with \r\n.
 * @return      The deserialized content as a plain string.
 *
 * @example
 *   de_serial("+OK\r\n")                               →  "OK"
 *   de_serial("$5\r\nhello\r\n")                       →  "hello"
 *   de_serial("*2\r\n$4\r\necho\r\n$5\r\nhello\r\n")  →  "echo hello "
 *   de_serial("$-1\r\n")                               →  "(null)"
 */
string de_serial(const string& data);

#endif  // RESP_DESERIALIZER_H
