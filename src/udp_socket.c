#include "udp_socket.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int init_udp_socket(const uint16_t bind_port) {
    int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_socket < 0) return -1;

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));

    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(bind_port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_socket, (struct sockaddr*) &bind_addr, sizeof(bind_addr)) < 0) {
        close(udp_socket);
        perror("Failed to bind socket.");
        return -1;
    }

    printf("UDP server listening on port %d.\n", bind_port);

    return udp_socket;
}

void destroy_udp_socket(int* udp_socket) {
    if (*udp_socket >= 0) {
        close(*udp_socket);
        *udp_socket = -1;
    }
}