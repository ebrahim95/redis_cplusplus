#include "resp_deserializer.h"
#include <cstring>
#include <stdlib.h>
#include <string>
#include <iostream>
using namespace std;

//Test cases 
/*
"$-1\r\n"                                    // Null bulk string
"*1\r\n$4\r\nping\r\n"                      // PING command
"*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n" // ECHO command
"*2\r\n$3\r\nget\r\n$3\r\nkey\r\n"          // GET command
"+OK\r\n"                                    // Simple string response
"-Error message\r\n"                         // Error response
"$0\r\n\r\n"                                // Empty bulk string
"+hello world\r\n"                          // Simple string response
*/

string de_serial(string value) {

    // Return empty string if input is empty
    if (value.empty()) return "";

    // Get the first character which indicates the RESP data type
    char type_resp = value[0];
    // Extract the value portion between the type indicator and the first CRLF
    string extract_value = value.substr(1, value.find("\r\n") - 1);

    // Handle simple strings (+), errors (-), and integers (:)
    // These types have their value directly after the type indicator
    if (type_resp == '+' || type_resp == '-' || type_resp == ':') {
        return extract_value;
    }

    // Handle null bulk strings ($-1) and null arrays (*-1)
    // Both return "(null)" when the length/count is -1
    if (extract_value.substr(0,2) == "-1") {
        return "(null)";
    }

    // Handle bulk strings (starting with '$')
    // Find the newline character after the length declaration
    int find_n = value.find('\n');
    // Parse the length of the bulk string from the length declaration
    int position_end = stoi(value.substr(1, value.find('\r') - 1));

    if (type_resp == '$') { 
        // Handle empty bulk string (length = 0)
        if ( position_end == 0) {
            return "(empty)";
        } else {
            // Extract and return the actual string content after the length declaration
            return value.substr(find_n + 1, position_end);
        }
    }
    
    // bulk array string     
    // Extract the number of elements in the array from the first character after '*'
    int loop_count = value[1] - '0';
    string completed_string = "";

    // string get_command = "*2\r\n$3\r\nget\r\n$3\r\nkey\r\n";
    // string mixed_command = "*4\r\n$3\r\nget\r\n:67\r\n$3\r\nkey\r\n:54\r\n";

    // Handle array type responses (starting with '*')
    if (type_resp == '*') {
        // Initialize tracking variables for parsing array elements
        int last_string_pos = 0;      // Track position for finding next '$' character
        int last_number_pos = 0;      // Track position for finding next number (unused in current implementation)
        int element_found = 0;        // Counter for elements processed so far

        // Loop through all elements in the array
        while (element_found != loop_count) {
            // Look for the next bulk string marker '$' starting from last_string_pos
            if (value.find('$', last_string_pos) != string::npos) {
                // Find the position of the '$' character
                size_t position_string_index = value.find('$', last_string_pos);
                int i = static_cast<int>(position_string_index);
                // Update last_string_pos to search after this '$' in next iteration
                last_string_pos += i + 1;
                // Increment the count of elements found
                element_found += 1;

                // Find the position of '\r' after the '$' to get the length declaration
                size_t position_r = value.find('\r', position_string_index);
                int int_position_r = static_cast<int>(position_r);

                // Extract and convert the length of the bulk string
                // stoi("23GEEKS") will convert
                // stoi("GEEKS23GEEKs") will not convert
                int string_length = stoi(value.substr(position_string_index + 1, int_position_r - 1 - position_string_index));

                // Find the position of '\n' after the length declaration
                size_t position_n = value.find('\n', i);
                
                int int_position_n = static_cast<int>(position_n);
                // Extract the actual string content and append it to the result with a space
                completed_string += value.substr(int_position_n + 1, string_length) + " ";

            }

            if (value.find(':', last_number_pos) != string::npos) {
                return "";
            }

        }
      
        // Return the concatenated string of all array elements
        return completed_string; 
    }
   

    return "none";



}