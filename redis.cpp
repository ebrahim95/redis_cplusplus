#include <cstddef>
#include <cstdio>
#include <string>
#include <valarray>
#include <vector>

#include "resp_deserializer.h"
#include <iostream>


void raw_string_print(string value) {
    for (char c : value) {
        if (c == '\r') cout << "\\r";
        else if (c == '\n') cout << "\\n";
        else cout << c;
    }
    cout << endl;
}

string serialize_simple_string(string value) {
        return "+" + value + "\r\n";
}


string serialize_error_string(string value) {
        return "-" + value + "\r\n";
}

string serialize_int(int value) {
        return ":" + to_string(value) + "\r\n";
}

string serialize_bulk_string(string value) {
        return "$" + to_string(value.length()) + "\r\n" + value + "\r\n";
}


bool isConvertibleToInt (const string &value) {
    try {
        stoi(value);
        return true;
    } catch (const invalid_argument& e) {
        return false;
    }
}

string serialize_array(string &value) {
    // create for loop to count elements 
    // use space as seperator 
    // use that as a size counter
    // we need to track the size of every string 
    // while I am going through the string I could add the string in a variable.
    // pseduo code
    // total_lenght_of_array = 0;
    // while (!end_of_string)
    //      for (let i; i < length_string; i++) {
    //          if (!space) {
    //              counter++
    //          } elsif (space) {
    //              counter = 0; 
    //          }

    // new solution 
    // use serialize int and bulk string functions
    // break up the deserialized string array functions into different string in an array
    // cycle through them and them to a completed array

    vector<string> break_up_string = {};
    int length_of_string = value.length();
    string completed_string = ""; 

    int total_count = 0;
    int count = 0;
    bool string_or_not = false;
    for (size_t i = 0; i < length_of_string; i++) {
        if (value[i] != ' ') {
            // or use value.substr(i, 1);
            count += 1;
            completed_string += string(1, value[i]);
        } else if (value[i] == ' ') {
            if (isConvertibleToInt(completed_string)) {
                completed_string = ":" + completed_string + "\r\n";
            } else {
                completed_string = "$" + to_string(count) + "\r\n" + completed_string + "\r\n";
            }
            break_up_string.push_back( completed_string);
            total_count += 1; 
            count = 0;
            completed_string = "";
        }

        if (i + 1 == length_of_string) {
            if (isConvertibleToInt(completed_string)) {
                completed_string = ":" + completed_string + "\r\n";
            } else {
                completed_string = "$" + to_string(count) + "\r\n" + completed_string + "\r\n";
            }
            total_count += 1;
            break_up_string.push_back(completed_string);
        }
    }

    break_up_string.insert(break_up_string.begin(), "*" + to_string(total_count) + "\r\n");

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