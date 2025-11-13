#include <iostream>
#include <thread>
#include <unistd.h>     // For sleep and close
#include <cstring>      // For strlen
#include <sys/socket.h> // For send
#include <fstream>      // To check if the file exists

// Core project includes
#include "../include/queue.h"
#include "../include/ofs_types.h"
#include "../include/filesystem.h"

// Third-party JSON library
#include "../include/json.hpp"
// for convenience
using json = nlohmann::json;


// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void start_server();
void worker_thread_function();

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
// The single, thread-safe queue for all incoming client requests
ThreadSafeQueue request_queue;

// The single, global instance of our file system.
// This holds the live, in-memory state of the .omni file.
FileSystemInstance g_fs_instance;

// ============================================================================
// MAIN FUNCTION
// ============================================================================
int main() {
    const std::string OMNI_PATH = "my_fs.omni";

    // --- 1. Initialize the File System ---
    // Check if the filesystem file exists. If not, create it.
    std::ifstream f(OMNI_PATH);
    if (!f.good()) {
        std::cout << "File system '" << OMNI_PATH << "' not found. Formatting a new one..." << std::endl;
        fs_format(OMNI_PATH);
    } else {
        std::cout << "Found existing file system '" << OMNI_PATH << "'." << std::endl;
    }

    // Load the file system (either the new one or the existing one) into our global instance.
    fs_init(g_fs_instance, OMNI_PATH);

    // --- 2. Start Server and Worker Threads ---
    // Create and start the worker thread. It will immediately start waiting for requests.
    std::thread worker(worker_thread_function);
    
    // Start the server to accept client connections. This function will run forever.
    start_server();

    // This part will never be reached in normal operation, but is good practice
    worker.join();
    return 0;
}

// ============================================================================
// WORKER THREAD LOGIC
// ============================================================================
/**
 * This function runs in a dedicated background thread. Its only job is to
 * pull requests from the queue one-by-one and process them, ensuring
 * that no two file system operations happen at the same time.
 */
// THIS IS THE ONLY FUNCTION YOU NEED TO REPLACE IN main.cpp

// --- REPLACE your entire worker_thread_function in main.cpp ---

// --- REPLACE your entire worker_thread_function in main.cpp ---

void worker_thread_function() {
    std::cout << "Worker thread started and is ready for real work." << std::endl;
    while (true) {
        ClientRequest req = request_queue.pop();
        
        json response_json;

        try {
            json request_json = json::parse(req.request_data);
            std::string operation = request_json["operation"];
            response_json["operation"] = operation;

            if (operation == "file_create") {
                std::string path = request_json["parameters"]["path"];
                std::string data = request_json["parameters"]["data"];
                file_create(g_fs_instance, path, data);
                response_json["status"] = "success";
                
            } else if (operation == "file_read") {
                std::string path = request_json["parameters"]["path"];
                std::string content = file_read(g_fs_instance, path);
                response_json["status"] = "success";
                response_json["data"]["content"] = content;

            } else if (operation == "dir_create") {
                std::string path = request_json["parameters"]["path"];
                dir_create(g_fs_instance, path);
                response_json["status"] = "success";

            } else if (operation == "dir_list") {
                std::string path = request_json["parameters"]["path"];
                std::vector<DirEntryInfo> entries = dir_list(g_fs_instance, path);
                response_json["status"] = "success";
                json data_array = json::array();
                for (const auto& entry : entries) {
                    data_array.push_back({
                        {"name", entry.name},
                        {"is_directory", entry.is_directory}
                    });
                }
                response_json["data"] = data_array;
            
            } 
            // --- NEW CODE BLOCK STARTS HERE ---
            else if (operation == "user_login") {
                std::string username = request_json["parameters"]["username"];
                std::string password = request_json["parameters"]["password"];

                // Call the login function you already wrote!
                bool login_success = user_login(g_fs_instance, username, password);

                if (login_success) {
                    response_json["status"] = "success";
                    // In a real system, you would generate and return a session_id here
                    response_json["data"]["message"] = "Login successful!";
                } else {
                    response_json["status"] = "error";
                    response_json["error_message"] = "Invalid username or password.";
                }
            } 
              // Add this block in main.cpp's worker thread
            else if (operation == "file_delete") {
                std::string path = request_json["parameters"]["path"];
                file_delete(g_fs_instance, path);
                response_json["status"] = "success";
            }
            // Add this new else if block inside the worker_thread_function in main.cpp

            else if (operation == "dir_delete") {
                std::string path = request_json["parameters"]["path"];
                dir_delete(g_fs_instance, path); // Call the new function
                response_json["status"] = "success";
            }
                        // Add this new else if block inside the worker_thread_function in main.cpp

            else if (operation == "file_exists") {
                 std::string path = request_json["parameters"]["path"];
                 bool exists = file_exists(g_fs_instance, path);
                 response_json["status"] = "success";
                 response_json["data"]["exists"] = exists; // Add the true/false result to the response
            }

            // // Add this new else if block inside the worker_thread_function in main.cpp

            else if (operation == "dir_exists") {
                std::string path = request_json["parameters"]["path"];
                bool exists = dir_exists(g_fs_instance, path);
                response_json["status"] = "success";
                response_json["data"]["exists"] = exists;
            }
            // Add this new else if block inside the worker_thread_function in main.cpp

            else if (operation == "file_rename") {
                std::string old_path = request_json["parameters"]["old_path"];
                std::string new_path = request_json["parameters"]["new_path"];
                file_rename(g_fs_instance, old_path, new_path);
                response_json["status"] = "success";
            }
            // Add this new else if block inside the worker_thread_function in main.cpp

            else if (operation == "user_create") {
    // NOTE: In a real system, you'd check if the logged-in user is an admin first.
                std::string username = request_json["parameters"]["username"];
                std::string password = request_json["parameters"]["password"];
                uint32_t role = request_json["parameters"]["role"];
                user_create(g_fs_instance, username, password, role);
                response_json["status"] = "success";
            }
            // Add these new else if blocks inside the worker_thread_function in main.cpp

            else if (operation == "user_delete") {
                std::string username = request_json["parameters"]["username"];
                user_delete(g_fs_instance, username);
                response_json["status"] = "success";
            }
else if (operation == "user_list") {
    std::vector<std::string> users = user_list(g_fs_instance);
    response_json["status"] = "success";
    // We can directly assign the vector to the JSON object!
    response_json["data"]["users"] = users;
}
            else {
                response_json["status"] = "error";
                response_json["error_message"] = "Unknown operation.";
            }

        } catch (json::parse_error& e) {
            response_json["status"] = "error";
            response_json["error_message"] = "Invalid JSON format.";
        }

        std::string response_str = response_json.dump();
        send(req.client_socket, response_str.c_str(), response_str.length(), 0);
        close(req.client_socket);
    }
}