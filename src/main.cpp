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

