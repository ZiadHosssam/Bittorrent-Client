# BitTorrent Client for YTS.mx Torrents

![C++](https://img.shields.io/badge/C++-23-blue)  
![License](https://img.shields.io/badge/license-MIT-green)  
![Build](https://img.shields.io/badge/build-passing-brightgreen)

A lightweight C++23 BitTorrent client designed to download single-file torrents from. This project implements core BitTorrent protocol features, including torrent parsing, tracker communication, and peer-to-peer file downloading. Built as a learning exercise, it showcases network programming, protocol implementation, and modern C++ skills.

## Features

- **Torrent Parsing**: Decodes `.torrent` files using bencode format.
- **Tracker Communication**: Fetches peer lists via HTTP/HTTPS trackers.
- **Peer-to-Peer Downloading**: Downloads and verifies file pieces from peers with SHA-1 hashing.
- **Commands**:
    - `info`: Displays torrent metadata (tracker URL, file length, info hash, piece hashes).
    - `peers`: Lists available peers.
    - `download_piece`: Downloads a single piece.
    - `download`: Downloads the entire file.
- **Robust Error Handling**: Handles invalid torrents, network failures, and protocol errors.
- **Single-File Focus**: Tailored for YTS.mx’s single-file torrents.

## Getting Started

### Prerequisites

- C++23 compiler (e.g., GCC, Clang)
- [vcpkg](https://vcpkg.io/) for dependencies
- CMake 3.13+
- Dependencies: `libcurl`, `openssl`, `nlohmann/json` (included in `lib/nlohmann/json.hpp`)

### Installation

1. Clone the repository:
    
    ```bash
    git clone https://github.com/your-username/bittorrent-client.git
    cd bittorrent-client
    ```
    
2. Install dependencies:
    
    ```bash
    export VCPKG_ROOT=/path/to/vcpkg
    $VCPKG_ROOT/vcpkg install curl openssl
    ```
    
3. Build the project:
    
    ```bash
    ./your_program.sh
    ```
    

### Usage

Run the client with:

```bash
./your_program.sh <command> [args]
```

Examples:

- View torrent info:
    
    ```bash
    ./your_program.sh info sample.torrent
    ```
    
- List peers:
    
    ```bash
    ./your_program.sh peers sample.torrent
    ```
    
- Download a file:
    
    ```bash
    ./your_program.sh download -o movie.mp4 sample.torrent
    ```
    

## What I Learned

Building this BitTorrent client was a deep dive into networking and C++ programming. Key takeaways:

- **BitTorrent Protocol**: Mastered bencode parsing, tracker requests, peer handshakes, and piece verification.
- **Network Programming**: Implemented TCP sockets with non-blocking I/O, timeouts, and CURL for HTTP.
- **C++23**: Leveraged modern features (e.g., `std::string`, `std::vector`, exceptions) for clean, efficient code.
- **Cryptography**: Used SHA-1 for hashing and URL-encoding binary data.
- **Build Systems**: Configured CMake and `vcpkg` for dependency management.
- **Problem-Solving**: Tackled challenges like JSON type errors, YTS.mx tracker quirks, and reliable peer connections.

This project honed my ability to implement complex protocols and debug distributed systems, preparing me for real-world networking challenges.

## Project Structure

- `src/main.cpp`: Core implementation (bencoding, tracker, peer logic, CLI).
- `CMakeLists.txt`: Build configuration.
- `your_program.sh`: Script for local compilation and execution.
- `lib/nlohmann/json.hpp`: JSON library for bencode parsing.

## Challenges Overcome

- **Peer Reliability**: Used non-blocking sockets and retries for robust downloads.
- **Error Handling**: Prevented JSON type errors with strict validation.
- **Optimization**: Streamlined code for single-file torrents, removing multi-file support.

## License

MIT License - feel free to use and modify!

## Acknowledgments

- [CodeCrafters](https://codecrafters.io/).
- [nlohmann/json](https://github.com/nlohmann/json) for JSON parsing.

---

_Built with passion for learning and networking!_
