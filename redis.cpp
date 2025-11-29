#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <valarray>
#include <vector>

using namespace std;

enum class RESP_TYPES {
    STRINGS,
    ERRORS,
    INTEGERS,
    BULK_STRINGS,
    ARRAYS,
    NULL_BULK_STRING,
    NULL_ARRAY
};

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

    if (value.empty()) return "";

    char type_resp = value[0];
    string extract_value = value.substr(1, value.find("\r\n"));

    // check for simple string, error, int
    if (type_resp == '+' || type_resp == '-' || type_resp == ':') {
        return extract_value;
    }

    // check for null bulk and array string
    if (extract_value.substr(0,2) == "-1") {
        return "(null)";
    }

    // check for bulk string
    int find_n = value.find('\n');
    int position_end = stoi(value.substr(1, value.find('\r') - 1));

    if (type_resp == '$') { 
        return value.substr(find_n + 1, position_end);
    }
    
    if (type_resp == '*') {
        
    }

    return "none";



}

int main() {
    string null_bulk_string = "$-1\r\n";
    string de_null_bulk_string = de_serial(null_bulk_string);

    string null_array_string = "*-1\r\n";
    string de_null_array_string = de_serial(null_array_string);

    // string ping_command = "*1\r\n$4\r\nping\r\n";
    // string de_ping_command = de_serial(ping_command);

    // string echo_command = "*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n";
    // string de_echo_command = de_serial(echo_command);

    // string get_command = "*2\r\n$3\r\nget\r\n$3\r\nkey\r\n";
    // string de_get_command = de_serial(get_command);

    string ok_response = "+OK\r\n";
    string de_ok_response = de_serial(ok_response);

    string error_response = "-Error message\r\n";
    string de_error_response = de_serial(error_response);


    // string empty_bulk_string = "$0\r\n\r\n";
    // string de_empty_bulk_string = de_serial(empty_bulk_string);

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



    // Using the <valarray> header to slice array
    //std::slice s(0, 6, 1);


    // extract all the resp strings
        // if (de_simple_string[0] == '+') {
        //     cout << "simple string" << '\n';
        //     cout << de_simple_string << "\n";
        // }

        // // error string  
        // if (de_error_response[0] == '-') {
        //     cout << "error" << '\n';
        //     cout << de_error_response << "\n";        
        // }
        
        // // null bulk string  
        // if (de_null_bulk_string.substr(0, 3) == "$-1") {
        //     cout << "null bulk string" << '\n';
        //     cout << de_null_bulk_string << "\n";
        // }

        // // null array string  
        // if (de_null_array_string.substr(0, 3) == "*-1") {
        //     cout << "null array string" << '\n';
        //     cout << de_null_array_string << "\n";
        // }
        
        // // int string 
        // if (de_int_string[0] == ':') {
        //     cout << "int string" << '\n';
        //     cout << de_int_string << "\n";
        // }
        
        // // empty bulk string
        // if (empty_bulk_string[0] == '$') {
        //     cout << "empty bulk string" << '\n';
        //     cout << de_empty_bulk_string << "\n";
        // }
        
        // // array string 
        // if (de_ping_command[0] == '*') {
        //     cout << "array string" << '\n';
        //     cout << de_ping_command << "\n";
        // }


    cout << endl;
}