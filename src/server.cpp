#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <vector> // Needed for the buffer
#include "../include/queue.h"

// This is our global, shared queue for all client requests
extern ThreadSafeQueue request_queue;

/**
 * This function runs in a temporary, detached thread for each new client.
 * Its only job is to read the client's message from the socket and put it
 * into the main queue for the worker thread to process.
 */
void handle_client(int client_socket) {
    std::cout << "Handling client on socket " << client_socket << std::endl;

    // Create a buffer to read the client's data into
    std::vector<char> buffer(1024);
    std::string received_data;

    // Read data from the socket
    int bytes_read = recv(client_socket, buffer.data(), buffer.size() - 1, 0);

    if (bytes_read > 0) {
        // Successful read
        buffer[bytes_read] = '\0'; // Null-terminate the string
        received_data = buffer.data();
    } else {
        // Error or client disconnected
        std::cerr << "Error reading from socket or client disconnected." << std::endl;
        close(client_socket);
        return;
    }

    // Now, we create a request with the REAL data from the client
    ClientRequest req;
    req.client_socket = client_socket;
    req.request_data = received_data; // Use the data we just read

    // Add the real request to the global queue
    request_queue.push(req);
    std::cout << "Request from socket " << client_socket << " has been added to the queue." << std::endl;
}

/**
 * The main server function. It creates the listening socket and enters an
 * infinite loop to accept new client connections.
 */
void start_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Error: Could not create socket." << std::endl; return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error: Bind failed." << std::endl; close(server_fd); return;
    }
    
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Error: Listen failed." << std::endl; close(server_fd); return;
    }
    std::cout << "Server is listening for connections..." << std::endl;

    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) {
            std::cerr << "Error: Accept failed." << std::endl; continue;
        }
        std::cout << "Connection accepted. Starting new thread to handle client." << std::endl;

        // Create a new thread for each client to handle reading their request.
        // Detach it so the server can immediately go back to listening.
        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    close(server_fd);
}