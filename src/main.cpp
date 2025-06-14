// Include Dependencies
#include "lib/nlohmann/json.hpp"
#include <arpa/inet.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using json = nlohmann::json;

// Decoding Functions

//function to decode bencoded value 
json decode_bencoded_value(const std::string &encoded_value, size_t &pos){
    // throw an error when the size doesn't match
    if (pos >= encoded_value.size()){
        throw std::runtime_error("Unexpected end of encoded value");
    }
    // store current char
    char c = encoded_value[pos];

    // decoded bencoded strings
    if (std::isdigit(c)){ 
        // locate the colon postion 
        size_t colon = encoded_value.find(':', pos);
        // check if there is a vaild colon or not 
        if (colon == std::string::npos){
            throw std::runtime_error("Invalid string: missing colon");
        }
        // store the length of the encoded string
        int64_t len = std::stoll(encoded_value.substr(pos, colon - pos));
        // check if the length matches the user input
        if (colon + 1 + len > encoded_value.size()){
            throw std::runtime_error("String length exceeds data");
        }
        // store the decoded value 
        std::string str = encoded_value.substr(colon + 1, len);
        pos = colon + 1 + len;
        return json(str);
  }
}
