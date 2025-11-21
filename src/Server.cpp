#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include "../include/RequestQueue.h"

// This is the global queue defined in Main.cpp
extern RequestQueue g_request_queue;

/**
 * @brief Handles an individual client connection in its own thread.
 *
 * This function's job is to read a single message from a client,
 * package it into a ClientRequest object, and push it onto the global
 * work queue. It then detaches, letting the worker thread handle the rest.
 *
 * @param client_socket The file descriptor for the connected client socket.
 */
void client_handler(int client_socket) {
    std::cout << "Handling new client on socket fd: " << client_socket << std::endl;

    // Create a buffer to hold incoming data.
    std::vector<char> buffer(2048);

    // Read data from the client socket.
    ssize_t bytes_read = recv(client_socket, buffer.data(), buffer.size() - 1, 0);

    if (bytes_read > 0) {
        // Null-terminate the received data to treat it as a C-string.
        buffer[bytes_read] = '\0';

        // Create a request struct.
        ClientRequest req;
        req.client_socket = client_socket;
        req.request_data = buffer.data();

        // Push the request onto the thread-safe queue.
        g_request_queue.push(req);

        std::cout << "Request from socket " << client_socket << " has been queued for processing." << std::endl;
    } else {
        // If recv returns 0, the client has closed the connection.
        // If it returns -1, an error occurred.
        std::cerr << "Failed to read from socket or client disconnected. Closing socket " << client_socket << std::endl;
        close(client_socket);
    }
    // This thread's job is done. The worker thread will handle the response and closing the socket on success.
}

/**
 * @brief Starts the main server loop.
 *
 * This function sets up the main listening socket and enters an infinite loop
 * to accept new client connections. For each new connection, it spawns a
 * detached thread to handle the client's request.
 */
void start_server() {
    // 1. Create the server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "FATAL: Could not create server socket." << std::endl;
        return;
    }

    // 2. Define the server address
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    server_addr.sin_port = htons(8080);      // Listen on port 8080

    // 3. Bind the socket to the address and port
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "FATAL: Bind failed. Port may be in use." << std::endl;
        close(server_fd);
        return;
    }
    
    // 4. Start listening for incoming connections
    if (listen(server_fd, 10) < 0) { // 10 is the max length of the pending connections queue
        std::cerr << "FATAL: Listen failed." << std::endl;
        close(server_fd);
        return;
    }
    
    std::cout << "Server is active and listening on port 8080..." << std::endl;

    // 5. Main accept loop
    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) {
            std::cerr << "Error: Accept call failed." << std::endl;
            continue; // Continue listening for other connections
        }
        
        std::cout << "Connection accepted. Creating a handler thread for the new client." << std::endl;

        // Create a new thread to handle the client and detach it.
        // Detaching allows the thread to run independently, and the main
        // loop can immediately go back to waiting for another connection.
        std::thread(client_handler, client_socket).detach();
    }

    // This part is unreachable in this simple server, but good practice
    close(server_fd);
}