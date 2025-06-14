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
  else if (c == 'l') {
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
  else if (c == 'd') {
    pos++;
    json dict = json::object();
    // make the functioon to decode the dictionary
    while (pos < encoded_value.size() && encoded_value[pos] != 'e') {
      // store each key for the whole dictionary in the end
      json key = decode_bencoded_value(encoded_value, pos);
      if (!key.is_string()) {
        throw std::runtime_error("Dictionary key must be string");
      }
      json value = decode_bencoded_value(encoded_value, pos);
      dict[key.get<std::string>()] = value;
    }
    // check for errors and return the values
    if (pos >= encoded_value.size() || encoded_value[pos] != 'e') {
      throw std::runtime_error("Invalid dictionary: missing 'e'");
    }
    pos++;
    return dict;
  }
  throw std::runtime_error("Invalid bencoded value at position " +
                           std::to_string(pos));
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
  if (value.is_string()) {
    std::string str = value.get<std::string>();
    return std::to_string(str.size()) + ":" + str;
  }
  // decode to an integer
  else if (value.is_number_integer()) {
    return "i" + std::to_string(value.get < int64_t()) + "e";
  }
  // decode to a list
  else if (value.is_array()) {
    std::string result = "l";
    for (const auto &elem : value) {
      result += bencode(elem);
    }
    return result + "e";
  }
  // encode to a dictionary
  else if (value.is_object()) {
    std::string result = "d";
    std::vector<std::string> keys;
    for (auto it = value.begin(); it != value.end(); ++it) {
      keys.push_back(it.key());
    }
    std::sort(keys.begin(), keys.end());
    for (const auto &key : keys) {
      result += bencode(key);
      result += bencode(value[key]);
    }
    return result + "e";
  }
  throw std::runtime_error("Unsupported JSON type for bencoding");
}

// Tracker Utils

// encode info hash for requests
std::string url_encode_info_hash(const unsigned char *hash, size_t length) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (size_t i = 0; i < length; ++i) {
    ss << "%" << std::setw(2) << static_cast<int>(hash[i]);
  }
  return ss.str();
}

// capture response using CURl lib
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total_size = size * nmemb;
  std::string *response = static_cast<std::string *>(userp);
  response->append(static_cast<char *>(contents), total_size);
  return total_size;
}

// fliter the HTTP  & HTTPS from the annouce list
std::string select_tracker_url(const json &torrent) {
  if (torrent.contains("announce-list") &&
      torrent["announce-list"].is_array()) {
    for (const auto &list : torrent["announce-list"]) {
      if (list.is_array()) {
        for (const auto &tracker : list) {
          if (tracker.is_string()) {
            std::string url = tracker.get<std::string>();
            if (url.substr(0, 7) == "http://" ||
                url.substr(0, 8) == "https://") {
              return url;
            }
          }
        }
      }
    }
  }
  if (torrent.contains("announce") && torrent["announce"].is_string()) {
    std::string url = torrent["announce"].get<std::string>();
    if (url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://") {
      return url;
    }
  }
  throw std::runtime_error("No valid HTTP/HTTPS tracker found");
}

// Peer Comunication Handle

// handshake to downalod a peice
void exchange_peer_messages(const std::string &saved_path,
                            const std::string &info_hash,
                            const std::pair<std::string, uint16_t> &peer,
                            int piece_index, int piece_length,
                            const std::string &pieces) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    throw std::runtime_error("Failed to create socket");

  struct timeval timeout = {10, 0};
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  sockaddr_in peer_addr = {AF_INET, htons(peer.second)};
  if (inet_pton(AF_INET, peer.first.c_str(), &peer_addr.sin_addr) <= 0) {
    close(sockfd);
    throw std::runtime_error("Invalid peer IP");
  }

  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

  int connect_result =
      connect(sockfd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
  if (connect_result < 0 && errno != EINPROGRESS) {
    close(sockfd);
    throw std::runtime_error("Failed to connect to peer");
  }

  if (connect_result < 0) {
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sockfd, &write_fds);
    struct timeval connect_timeout = {5, 0};
    if (select(sockfd + 1, nullptr, &write_fds, nullptr, &connect_timeout) <=
        0) {
      close(sockfd);
      throw std::runtime_error("Connection timeout");
    }
    int error;
    socklen_t len = sizeof(error);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 ||
        error != 0) {
      close(sockfd);
      throw std::runtime_error("Connection failed");
    }
  }
  fcntl(sockfd, F_SETFL, flags);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);
  std::vector<unsigned char> peer_id(20);
  for (auto &byte : peer_id)
    byte = static_cast<unsigned char>(dis(gen));

  std::vector<char> handshake(68, 0);
  handshake[0] = 19;
  std::copy_n("BitTorrent protocol", 19, handshake.begin() + 1);
  std::copy(info_hash.begin(), info_hash.end(), handshake.begin() + 28);
  std::copy(peer_id.begin(), peer_id.end(), handshake.begin() + 48);

  if (send(sockfd, handshake.data(), handshake.size(), 0) !=
      static_cast<ssize_t>(handshake.size())) {
    close(sockfd);
    throw std::runtime_error("Failed to send handshake");
  }

  std::vector<char> response(68);
  ssize_t received = 0;
  while (received < 68) {
    ssize_t bytes = recv(sockfd, response.data() + received, 68 - received, 0);
    if (bytes <= 0) {
      close(sockfd);
      throw std::runtime_error("Failed to receive handshake");
    }
    received += bytes;
  }

  if (response[0] != 19 ||
      std::strncmp(response.data() + 1, "BitTorrent protocol", 19) != 0) {
    close(sockfd);
    throw std::runtime_error("Invalid handshake response");
  }

  uint32_t msg_len;
  received = 0;
  while (received < 4) {
    ssize_t bytes = recv(sockfd, reinterpret_cast<char *>(&msg_len) + received,
                         4 - received, 0);
    if (bytes <= 0) {
      close(sockfd);
      throw std::runtime_error("Failed to receive message length");
    }
    received += bytes;
  }
  msg_len = ntohl(msg_len);
  std::vector<char> payload(msg_len);
  if (msg_len > 0) {
    received = 0;
    while (received < static_cast<ssize_t>(msg_len)) {
      ssize_t bytes =
          recv(sockfd, payload.data() + received, msg_len - received, 0);
      if (bytes <= 0) {
        close(sockfd);
        throw std::runtime_error("Failed to receive bitfield");
      }
      received += bytes;
    }
  }
  if (msg_len < 1 || payload[0] != 5) {
    close(sockfd);
    throw std::runtime_error("Expected bitfield message");
  }

  char interested_msg[5] = {0, 0, 0, 1, 2};
  if (send(sockfd, interested_msg, 5, 0) != 5) {
    close(sockfd);
    throw std::runtime_error("Failed to send interested message");
  }

  received = 0;
  while (received < 4) {
    ssize_t bytes = recv(sockfd, reinterpret_cast<char *>(&msg_len) + received,
                         4 - received, 0);
    if (bytes <= 0) {
      close(sockfd);
      throw std::runtime_error("Failed to receive unchoke length");
    }
    received += bytes;
  }
  msg_len = ntohl(msg_len);
  if (msg_len != 1) {
    close(sockfd);
    throw std::runtime_error("Expected unchoke message length 1");
  }
  char msg_id;
  if (recv(sockfd, &msg_id, 1, 0) != 1 || msg_id != 1) {
    close(sockfd);
    throw std::runtime_error("Expected unchoke message");
  }

  const uint32_t block_size = 16384;
  std::vector<char> piece(piece_length);
  uint32_t offset = 0;
  while (offset < static_cast<uint32_t>(piece_length)) {
    uint32_t request_len =
        std::min(static_cast<uint32_t>(piece_length) - offset, block_size);

    char request_msg[17] = {0, 0, 0, 13, 6};
    *reinterpret_cast<uint32_t *>(request_msg + 5) = htonl(piece_index);
    *reinterpret_cast<uint32_t *>(request_msg + 9) = htonl(offset);
    *reinterpret_cast<uint32_t *>(request_msg + 13) = htonl(request_len);
    if (send(sockfd, request_msg, 17, 0) != 17) {
      close(sockfd);
      throw std::runtime_error("Failed to send request");
    }

    received = 0;
    while (received < 4) {
      ssize_t bytes =
          recv(sockfd, reinterpret_cast<char *>(&msg_len) + received,
               4 - received, 0);
      if (bytes <= 0) {
        close(sockfd);
        throw std::runtime_error("Failed to receive piece length");
      }
      received += bytes;
    }
    msg_len = ntohl(msg_len);
    if (msg_len < 9) {
      close(sockfd);
      throw std::runtime_error("Invalid piece message length");
    }
    std::vector<char> piece_payload(msg_len);
    received = 0;
    while (received < static_cast<ssize_t>(msg_len)) {
      ssize_t bytes =
          recv(sockfd, piece_payload.data() + received, msg_len - received, 0);
      if (bytes <= 0) {
        close(sockfd);
        throw std::runtime_error("Failed to receive piece");
      }
      received += bytes;
    }
    if (piece_payload[0] != 7) {
      close(sockfd);
      throw std::runtime_error("Expected piece message");
    }
    uint32_t received_index =
        ntohl(*reinterpret_cast<uint32_t *>(piece_payload.data() + 1));
    uint32_t received_begin =
        ntohl(*reinterpret_cast<uint32_t *>(piece_payload.data() + 5));
    if (received_index != static_cast<uint32_t>(piece_index) ||
        received_begin != offset) {
      close(sockfd);
      throw std::runtime_error("Received incorrect block");
    }
    std::copy(piece_payload.begin() + 9,
              piece_payload.begin() + 9 + request_len, piece.begin() + offset);
    offset += request_len;
  }

  close(sockfd);

  unsigned char computed_hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char *>(piece.data()), piece.size(),
       computed_hash);
  std::string expected_hash = pieces.substr(piece_index * 20, 20);
  if (std::string(reinterpret_cast<char *>(computed_hash), SHA_DIGEST_LENGTH) !=
      expected_hash) {
    throw std::runtime_error("Piece hash mismatch");
  }

  std::ofstream outfile(saved_path, std::ios::binary);
  if (!outfile)
    throw std::runtime_error("Failed to open output file");
  outfile.write(piece.data(), piece.size());
  outfile.close();
}