#include <cstddef>
#include <cstdio>
#include <string>
#include <valarray>
#include <vector>

#include "resp_deserializer.h"
#include <iostream>


// Function to print a string with escaped carriage return and newline characters
void raw_string_print(string value) {
    for (char c : value) {
        if (c == '\r') cout << "\\r";
        else if (c == '\n') cout << "\\n";
        else cout << c;
    }
    cout << endl;
}

// Serialize a simple string in RESP format (prefixed with '+')
string serialize_simple_string(string value) {
        return "+" + value + "\r\n";
}

// Serialize an error string in RESP format (prefixed with '-')
string serialize_error_string(string value) {
        return "-" + value + "\r\n";
}

// Serialize an integer in RESP format (prefixed with ':')
string serialize_int(int value) {
        return ":" + to_string(value) + "\r\n";
}

// Serialize a bulk string in RESP format (prefixed with '$' and length)
string serialize_bulk_string(string value) {
        return "$" + to_string(value.length()) + "\r\n" + value + "\r\n";
}

// Check if a string can be converted to an integer
bool isConvertibleToInt (const string &value) {
    try {
        stoi(value);
        return true;
    } catch (const invalid_argument& e) {
        return false;
    }
}

// Serialize a space-separated string into a RESP array format
// Integers are serialized as RESP integers, strings as RESP bulk strings
string serialize_array(string &value) {

    vector<string> break_up_string = {};
    int length_of_string = value.length();
    string completed_string = ""; 

    int total_count = 0;  // Count of array elements
    int count = 0;        // Character count for current token
    
    // Parse the input string character by character
    for (size_t i = 0; i < length_of_string; i++) {
        if (value[i] != ' ') {
            // Build current token character by character
            count += 1;
            completed_string += string(1, value[i]);
        } else if (value[i] == ' ') {
            // Space encountered - process the completed token
            if (isConvertibleToInt(completed_string)) {
                // Serialize as RESP integer
                completed_string = ":" + completed_string + "\r\n";
            } else {
                // Serialize as RESP bulk string
                completed_string = "$" + to_string(count) + "\r\n" + completed_string + "\r\n";
            }
            break_up_string.push_back( completed_string);
            total_count += 1; 
            count = 0;
            completed_string = "";
        }

        // Handle the last token (no trailing space)
        if (i + 1 == length_of_string) {
            if (isConvertibleToInt(completed_string)) {
                // Serialize as RESP integer
                completed_string = ":" + completed_string + "\r\n";
            } else {
                // Serialize as RESP bulk string
                completed_string = "$" + to_string(count) + "\r\n" + completed_string + "\r\n";
            }
            total_count += 1;
            break_up_string.push_back(completed_string);
        }
    }

    // Add RESP array header with element count
    break_up_string.insert(break_up_string.begin(), "*" + to_string(total_count) + "\r\n");

    // Concatenate all parts into final RESP array string
    string final_string = "";
    for (string value : break_up_string) {
       // cout << value << '\n';
       final_string += value;
    }
    
    return final_string;

}

int main() {
    string null_bulk_string = "$-1\r\n";
    string de_null_bulk_string = de_serial(null_bulk_string);

    string null_array_string = "*-1\r\n";
    string de_null_array_string = de_serial(null_array_string);

    string ping_command = "*1\r\n$4\r\nping\r\n";
    string de_ping_command = de_serial(ping_command);

    string echo_command = "*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n";
    string de_echo_command = de_serial(echo_command);

    string get_command = "*2\r\n$3\r\nget\r\n$3\r\nkey\r\n";
    string de_get_command = de_serial(get_command);

    string mixed_command = "*4\r\n$3\r\nget\r\n:67\r\n$3\r\nkey\r\n:54\r\n";
    string de_mixed_command = de_serial(mixed_command);

    string ok_response = "+OK\r\n";
    string de_ok_response = de_serial(ok_response);

    string error_response = "-Error message\r\n";
    string de_error_response = de_serial(error_response);


    string empty_bulk_string = "$0\r\n\r\n";
    string de_empty_bulk_string = de_serial(empty_bulk_string);

    string simple_string = "+hello world\r\n";
    string de_simple_string = de_serial(simple_string);

    
    string int_string = ":123\r\n";
    string de_int_string = de_serial(int_string);

    string bulk_string = "$11\r\nhello world\r\n";
    string de_bulk_string = de_serial(bulk_string);

    cout << de_ok_response;
    cout << '\n' << de_error_response;
    cout << '\n' << de_simple_string;
    cout << '\n' << de_int_string;
    cout << '\n' << de_null_bulk_string;
    cout << '\n' << de_null_array_string;
    cout << '\n' << de_bulk_string;
    cout << '\n' << de_empty_bulk_string;
    cout << '\n' << de_ping_command;
    cout << '\n' << de_echo_command;
    cout << '\n' << de_get_command << '\n';

raw_string_print(de_ping_command);
raw_string_print(de_echo_command);
raw_string_print(de_mixed_command);
  /*   raw_string_print(serialize_simple_string(de_ok_response));
    raw_string_print(serialize_error_string(de_error_response));
    raw_string_print(serialize_int(stoi(de_int_string)));
    raw_string_print(serialize_bulk_string(de_bulk_string));
    
 */

 string raw_string = "get 67 key 54";
 raw_string_print(serialize_array(raw_string));

    // Using the <valarray> header to slice array
    //std::slice s(0, 6, 1);


}