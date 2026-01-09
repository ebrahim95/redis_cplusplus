#include <cstddef>
#include <cstdio>
#include <string>
#include <valarray>

#include "resp_deserializer.h"
#include "resp_serializer.h"
#include <iostream>


// Function to print a string with escaped carriage return and newline characters


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