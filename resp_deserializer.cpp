#include "resp_deserializer.h"
#include <cstring>
#include <stdlib.h>
#include <string>

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
    
    // Handle array type responses (starting with '*')
    if (type_resp == '*') {
        int last_string_index = 0;
        
        // Loop through each element in the array
        for (int i = 0; i < loop_count; i++) {
               // Find the position of the newline character for the current bulk string length declaration
              int find_n = value.find('\n', last_string_index);
              
              // Extract the length of the current bulk string from the length declaration
              int position_end = stoi(value.substr(find_n + 2, value.find('\r', find_n) - 1)); 
              
              // Find the start position of the actual string content (after the length declaration)
              int position_end_index = value.find('\n', find_n + 1);
              
              // Extract the string content and append it to the result with a space separator
              completed_string += value.substr(position_end_index + 1, position_end) + " ";   
              
              // Update the search position for the next iteration (skip past current string + CRLF)
              last_string_index = find_n + position_end + 4;     
        }
    
        return completed_string; 
    }
   

    return "none";



}