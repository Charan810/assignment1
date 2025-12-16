// udpclient.cpp
// Build: g++ -std=c++17 udpclient.cpp -o udp_client -lcrypto
// Usage: ./udp_client charlie.nplab.bth.se 5000 your@email

#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>

#include <openssl/sha.h>

static bool handle_response(int sockfd)
{
    constexpr std::size_t BUF_SIZE = 4096;
    char buffer[BUF_SIZE];

    // Receive response
    ssize_t n = ::recv(sockfd, buffer, BUF_SIZE, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false; // timeout, caller decides whether to retry
        } else {
            std::perror("recv");
            return false;
        }
    }

    if (n < 32) {
        std::cerr << "Received packet too short (" << n
                  << " bytes, need >= 32)\n";
        return false;
    }

    // First 32 bytes = SHA-256 hash, rest = quote+padding
    const unsigned char* recv_hash =
        reinterpret_cast<unsigned char*>(buffer);
    const char* message_part = buffer + 32;
    std::size_t message_len = static_cast<std::size_t>(n) - 32;

    unsigned char local_hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(message_part),
           message_len,
           local_hash);

    bool hash_ok =
        (std::memcmp(recv_hash, local_hash, SHA256_DIGEST_LENGTH) == 0);

    if (hash_ok) {
        std::cout << "Hash OK (read " << n
                  << " bytes). Full message (including padding):\n";
        std::cout.write(message_part, message_len);
        std::cout << "\n";
    } else {
        std::cout << "Hash mismatch (read " << n
                  << " bytes). Not printing quote.\n";
    }

    std::cout << "Hash:";
    std::cout << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        std::cout << std::setw(2)
                  << static_cast<unsigned int>(recv_hash[i]);
    }
    std::cout << std::dec << "\n";

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <host> <port> <email>\n";
        return 1;
    }

    const char* host  = argv[1];
    const char* port  = argv[2];
    const char* email = argv[3];

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;   // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;  // UDP

    struct addrinfo* res = nullptr;
    int ret = getaddrinfo(host, port, &hints, &res);
    if (ret != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(ret) << "\n";
        return 1;
    }

    const int max_retries = 3;
    bool overall_success = false;

    // Try each resolved address until one gives a valid reply
    for (struct addrinfo* rp = res; rp != nullptr && !overall_success; rp = rp->ai_next) {
        int sockfd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }

        // Set 1-second receive timeout on this socket
        struct timeval tv{};
        tv.tv_sec  = 1;
        tv.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            std::perror("setsockopt(SO_RCVTIMEO)");
            ::close(sockfd);
            continue;
        }

        if (::connect(sockfd, rp->ai_addr, rp->ai_addrlen) < 0) {
            ::close(sockfd);
            continue;
        }

        // Try up to max_retries on this particular address
        for (int attempt = 1; attempt <= max_retries && !overall_success; ++attempt) {
            ssize_t sent = ::send(sockfd, email, std::strlen(email), 0);
            if (sent < 0) {
                std::perror("send");
                break; // try next address
            }

            if (!handle_response(sockfd)) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::cerr << "Timeout (attempt " << attempt
                              << " of " << max_retries << ") on one address\n";
                    continue; // retry on same address
                } else {
                    // Some other recv error already printed in handle_response
                    break; // try next address
                }
            } else {
                overall_success = true;
            }
        }

        ::close(sockfd);
    }

    freeaddrinfo(res);

    if (!overall_success) {
        std::cerr << "No valid response after trying all addresses and retries.\n";
        return 1;
    }

    return 0;
}
