#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include "include/queue.h"

extern ThreadSafeQueue request_queue;

void handle_client(int client_socket) {
    // This function must be completely silent. No std::cout!
    std::vector<char> buffer(2048, 0);
    int bytes_read = recv(client_socket, buffer.data(), buffer.size() - 1, 0);
    if (bytes_read > 0) {
        ClientRequest req;
        req.client_socket = client_socket;
        req.request_data = std::string(buffer.data(), bytes_read);
        request_queue.push(req);
    } else {
        close(client_socket);
    }
}

void start_server() {
    // This function must also be completely silent.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) { return; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { close(server_fd); return; }
    if (listen(server_fd, 10) < 0) { close(server_fd); return; }
    
    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) continue;
        // Do NOT print anything here.
        std::thread(handle_client, client_socket).detach();
    }
    close(server_fd);
}