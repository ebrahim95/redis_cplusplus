#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>

using namespace std; 


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