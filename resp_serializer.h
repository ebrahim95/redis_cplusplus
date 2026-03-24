#ifndef RESP_SERIALIZER_H
#define RESP_SERIALIZER_H

#include <string> 
using namespace std; 

void raw_string_print(const string& value);

// Serialize a simple string in RESP format (prefixed with '+')
string serialize_simple_string(const string& value);

// Serialize an error string in RESP format (prefixed with '-')
string serialize_error_string(const string& value);

// Serialize an integer in RESP format (prefixed with ':')
string serialize_int(const int& value);

// Serialize a bulk string in RESP format (prefixed with '$' and length)
string serialize_bulk_string(const string& value);

// Serialize a space-separated string into a RESP array format
// Integers are serialized as RESP integers, strings as RESP bulk strings
string serialize_array(const string& value);


#endif
