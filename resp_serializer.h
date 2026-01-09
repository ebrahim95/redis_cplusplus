#ifndef RESP_SERIALIZER_H
#define RESP_SERIALIZER_H

#include <string> 
using namespace std; 

void raw_string_print(string value);

// Serialize a simple string in RESP format (prefixed with '+')
string serialize_simple_string(string value);

// Serialize an error string in RESP format (prefixed with '-')
string serialize_error_string(string value);

// Serialize an integer in RESP format (prefixed with ':')
string serialize_int(int value);

// Serialize a bulk string in RESP format (prefixed with '$' and length)
string serialize_bulk_string(string value);

// Check if a string can be converted to an integer
bool isConvertibleToInt (const string &value);

// Serialize a space-separated string into a RESP array format
// Integers are serialized as RESP integers, strings as RESP bulk strings
string serialize_array(string &value);


#endif
