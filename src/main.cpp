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

// function to decode bencoded value
json decode_bencoded_value(const std::string &encoded_value, size_t &pos) {
  // throw an error when the size doesn't match
  if (pos >= encoded_value.size()) {
    throw std::runtime_error("Unexpected end of encoded value");
  }
  // store current char
  char c = encoded_value[pos];

  // decoded bencoded strings
  if (std::isdigit(c)) {
    // locate the colon postion
    size_t colon = encoded_value.find(':', pos);
    // check if there is a vaild colon or not
    if (colon == std::string::npos) {
      throw std::runtime_error("Invalid string: missing colon");
    }
    // store the length of the encoded string
    int64_t len = std::stoll(encoded_value.substr(pos, colon - pos));
    // check if the length matches the user input
    if (colon + 1 + len > encoded_value.size()) {
      throw std::runtime_error("String length exceeds data");
    }
    // store the decoded value
    std::string str = encoded_value.substr(colon + 1, len);
    pos = colon + 1 + len;
    return json(str);
  }

  // decoded bencoded Integers
  else if (c == 'i') {
    // skip the postion of the i and find the end
    pos++;
    size_t end = encoded_value.find('e', pos);
    // check for errors
    if (end == std::string::npos) {
      throw std::runtime_error("Invalid integer: missing 'e'");
    }
    // store the decoded value and return it
    int64_t num = std::stoll(encoded_value.substr(pos, end - pos));
    pos = end + 1;
    return json(num);
  }

  // decoded bencoded Lists
  else if (c == 'l'){
    pos++;
    json list = json::array();
    // make a loop to decode the value inside the main function
    while (pos < encoded_value.size() && encoded_value[pos] != 'e') {
      list.push_back(decode_bencoded_value(encoded_value, pos));
    }
    // check for erros 
    if (pos >= encoded_value.size() || encoded_value[pos] != 'e') {
      throw std::runtime_error("Invalid list: missing 'e'");
    }
    pos++;
    return list;
  }

  // decoded bencoded dictionires
  else if (c == 'd'){
    pos++;
    json dict = json::object();
    // make the functioon to decode the dictionary
    while (pos < encoded_value.size() && encoded_value[pos] != 'e'){
        // store each key for the whole dictionary in the end
        json key = decode_bencoded_value(encoded_value, pos);
        if (!key.is_string()){
            throw std::runtime_error("Dictionary key must be string");
        }
        json value = decode_bencoded_value(encoded_value, pos);
        dict[key.get<std::string>()] = value;
    }
    // check for errors and return the values
    if (pos >= encoded_value.size() || encoded_value[pos] != 'e'){
        throw std::runtime_error("Invalid dictionary: missing 'e'");
    }
    pos++;
    return dict;
  }
    throw std::runtime_error("Invalid bencoded value at position " + std::to_string(pos));
}

// decode values using the function above
json decode_bencoded_value(const std::string &encoded_value) {
  size_t pos = 0;
  json result = decode_bencoded_value(encoded_value, pos);
  if (pos != encoded_value.size()) {
    throw std::runtime_error("Extra data after decoding");
  }
  return result;
}

// encode JSON value to bencode
std::string bencode(const json &value) {
    // decode to a stirng
    if (value.is_string()){
        std::string str = value.get<std::string>();
        return std::to_string(str.size()) + ":" + str;
    }
    // decode to an integer
    else if (value.is_number_integer()){
        return "i" + std::to_string(value.get<int64_t()) + "e";
    }
    // decode to a list
    else if (value.is_array()){
        std::string result = "l";
        for (const auto &elem : value){
            result += bencode(elem);
        }
        return result + "e";
    }
    //encode to a dictionary
    else if (value.is_object()){
        std::string result = "d";
        std::vector<std::string> keys;
        for (auto it = value.begin(); it != value.end(); ++it){
            keys.push_back(it.key());
        }
        std::sort(keys.begin(), keys.end());
        for (const auto &key : keys){
            result += bencode(key);
            result += bencode(value[key]);
        }
        return result + "e";
    }
    throw std::runtime_error("Unsupported JSON type for bencoding");
}