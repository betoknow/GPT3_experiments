#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>

uint16_t checksum(void *data, int len) {
    uint16_t *words = (uint16_t *)data;
    uint32_t sum = 0;
    for (int i = 0; i < len / 2; i++) {
        sum += words[i];
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

unsigned int get_iface_index(const std::string &iface_name) {
    unsigned int iface_index = if_nametoindex(iface_name.c_str());
    if (iface_index == 0) {
        std::cerr << "Failed to find interface: " << iface_name << std::endl;
        exit(1);
    }
    return iface_index;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <interface> <wait_time_in_seconds>" << std::endl;
        return 1;
    }

    std::string iface_name = argv[1];
    int wait_time = std::stoi(argv[2]);

    int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    unsigned int iface_index = get_iface_index(iface_name);
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &iface_index, sizeof(iface_index)) < 0) {
        perror("setsockopt");
        return 1;
    }

    int loop = 1;
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
        perror("setsockopt");
        return 1;
    }

    struct icmp6_hdr icmp6_req;
    memset(&icmp6_req, 0, sizeof(icmp6_req));
    icmp6_req.icmp6_type = ICMP6_ECHO_REQUEST;
    icmp6_req.icmp6_code = 0;
    icmp6_req.icmp6_id = htons(getpid() & 0xffff);
    icmp6_req.icmp6_seq = htons(1);
    icmp6_req.icmp6_cksum = checksum(&icmp6_req, sizeof(icmp6_req));

    struct sockaddr_in6 dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "ff02::1", &dest_addr.sin6_addr);

    if (sendto(sockfd, &icmp6_req, sizeof(icmp6_req), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("sendto");
        return 1;
    }

    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(wait_time)) {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100 ms
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            perror("setsockopt");
            return 1;
        }

        char buf[2048];
        struct sockaddr_in6 src_addr;
        socklen_t addr_len = sizeof(src_addr);
        int len = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&src_addr, &addr_len);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout reached, continue to next iteration
                continue;
            }
            perror("recvfrom");
            return 1;
        }

        struct icmp6_hdr *icmp6_resp = (struct icmp6_hdr *)buf;
        if (icmp6_resp->icmp6_type == ICMP6_ECHO_REPLY && icmp6_resp->icmp6_id == icmp6_req.icmp6_id) {
            char ip_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &src_addr.sin6_addr, ip_str, sizeof(ip_str));
            std::cout << "Received response from " << ip_str << std::endl;
        }
    }

    close(sockfd);
    return 0;
}