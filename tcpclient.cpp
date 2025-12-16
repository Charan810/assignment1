// tcpclient.cpp
// Build: g++ -std=c++17 tcpclient.cpp -o anim
// Usage: ./anim charlie.nplab.bth.se 2323

#include <iostream>
#include <iomanip>
#include <cstring>
#include <chrono>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // We expect two arguments: host and port
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }

    const char* host = argv[1];
    const char* port = argv[2];

    // -------------------------
    // Resolve host/port (IPv4 or IPv6) using getaddrinfo
    // -------------------------
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;   // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    struct addrinfo* res = nullptr;
    int ret = getaddrinfo(host, port, &hints, &res);
    if (ret != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(ret) << "\n";
        return 1;
    }

    // -------------------------
    // Try to connect to one of the results
    // -------------------------
    int sockfd = -1;
    struct addrinfo* rp;
    for (rp = res; rp != nullptr; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            // Could not create this socket, try next address
            continue;
        }

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // Connected successfully
            break;
        }

        // connect() failed, close socket and try next
        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd == -1) {
        std::cerr << "Failed to connect to " << host << ":" << port << "\n";
        return 1;
    }

    // -------------------------
    // Terminal setup:
    // - switch to alternate screen buffer
    // - hide cursor
    // -------------------------
    std::cout << "\x1b[?1049h"; // alternate screen
    // std::cout << "\x1b[?25l";   // hide cursor
    std::cout.flush();

    // -------------------------
    // Reading loop
    // -------------------------
    const std::size_t BUF_SIZE = 4096;
    char buffer[BUF_SIZE];

    std::size_t total_bytes = 0;
    auto start_time = std::chrono::steady_clock::now();

    int exit_status = 0;

    while (true) {
        // Take time before and after each read to measure duration
        auto read_start = std::chrono::steady_clock::now();
        ssize_t n = read(sockfd, buffer, BUF_SIZE);
        auto read_end = std::chrono::steady_clock::now();

        if (n < 0) {
            perror("read");
            exit_status = 1;
            break;
        }
        if (n == 0) {
            // Server closed connection
            break;
        }

        total_bytes += static_cast<std::size_t>(n);

        std::chrono::duration<double> read_duration = read_end - read_start;
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;

        // Move cursor to top-left and clear screen
        std::cout << "\x1b[H\x1b[2J";

        // Print header, similar to assignment example:
        // Total Bytes: 44767 | Last Read: 0311 | Duration: 0.536304 | Elapsed: 29.00 sec
        std::cout << "Total Bytes: " << total_bytes
                  << " | Last Read: "
                  << std::setw(4) << std::setfill('0') << n
                  << " | Duration: "
                  << std::fixed << std::setprecision(6) << read_duration.count()
                  << " | Elapsed: "
                  << std::fixed << std::setprecision(2) << elapsed.count()
                  << " sec\n\n";

        std::cout.flush();

        // Write the animation bytes exactly as received (binary-safe).
        // We use write() so that the escape sequences are not modified by iostreams.
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(STDOUT_FILENO, buffer + written, n - written);
            if (w < 0) {
                perror("write");
                exit_status = 1;
                goto cleanup; // simple way to jump to cleanup code
            }
            written += w;
        }
    }

cleanup:
    // -------------------------
    // Cleanup: close socket, restore terminal
    // -------------------------
    close(sockfd);

    // Reset attributes, show cursor, and return to normal screen buffer
    std::cout << "\x1b[0m";       // reset colors / attributes
    std::cout << "\x1b[?25h";     // show cursor
    std::cout << "\x1b[?1049l";   // back to normal screen
    std::cout.flush();

    return exit_status;
}
