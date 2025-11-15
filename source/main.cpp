#include <iostream>
#include <thread>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>

#include "include/queue.h"
#include "include/ofs_types.h"
#include "include/filesystem.h"
#include "include/json.hpp"

using json = nlohmann::json;

void start_server();
void worker_thread_function();

ThreadSafeQueue request_queue;
FileSystemInstance g_fs_instance;

int main() {
    const std::string OMNI_PATH = "my_fs.omni";
    
    // --- ALL SERVER STARTUP MESSAGES HAPPEN HERE, AND ONLY HERE ---
    std::cout << "--- OFS Server Starting ---" << std::endl;

    std::ifstream f(OMNI_PATH);
    if (!f.good()) {
        std::cout << "File system '" << OMNI_PATH << "' not found. Formatting..." << std::endl;
        fs_format(OMNI_PATH);
    } else {
        std::cout << "Found existing file system '" << OMNI_PATH << "'." << std::endl;
    }
    f.close();

    std::cout << "Initializing file system into memory..." << std::endl;
    fs_init(g_fs_instance, OMNI_PATH);
    std::cout << "Initialization complete." << std::endl;

    // Start the background thread that will process requests
    std::thread worker(worker_thread_function);
    
    // This is the LAST message before the server enters its infinite loop.
    std::cout << "Starting network listener on port 8080..." << std::endl;
    start_server(); // This function will never return.

    worker.join();
    return 0;
}

void worker_thread_function() {
    // This message is safe because it only prints once when the server starts.
    std::cout << "Worker thread started and is ready for requests." << std::endl;
    while (true) {
        ClientRequest req = request_queue.pop();
        json response_json;
        try {
            json request_json = json::parse(req.request_data);
            std::string operation = request_json["operation"];
            response_json["operation"] = operation;
            response_json["status"] = "success";

            // --- USER MANAGEMENT ---
            if (operation == "user_login") {
                if (!user_login(g_fs_instance, request_json["parameters"]["username"], request_json["parameters"]["password"])) {
                    response_json["status"] = "error"; response_json["error_message"] = "Invalid credentials.";
                }
            } else if (operation == "user_create") {
                user_create(g_fs_instance, request_json["parameters"]["username"], request_json["parameters"]["password"], request_json["parameters"]["role"]);
            } else if (operation == "user_delete") {
                user_delete(g_fs_instance, request_json["parameters"]["username"]);
            } else if (operation == "user_list") {
                response_json["data"]["users"] = user_list(g_fs_instance);
            }
            // --- FILE OPERATIONS ---
            else if (operation == "file_create") {
                file_create(g_fs_instance, request_json["parameters"]["path"], request_json["parameters"]["data"]);
            } else if (operation == "file_read") {
                response_json["data"]["content"] = file_read(g_fs_instance, request_json["parameters"]["path"]);
            } else if (operation == "file_delete") {
                file_delete(g_fs_instance, request_json["parameters"]["path"]);
            } else if (operation == "file_exists") {
                response_json["data"]["exists"] = file_exists(g_fs_instance, request_json["parameters"]["path"]);
            } else if (operation == "file_rename") {
                file_rename(g_fs_instance, request_json["parameters"]["old_path"], request_json["parameters"]["new_path"]);
            }
            // --- DIRECTORY OPERATIONS ---
            else if (operation == "dir_create") {
                dir_create(g_fs_instance, request_json["parameters"]["path"]);
            } else if (operation == "dir_list") {
                std::vector<DirEntryInfo> entries = dir_list(g_fs_instance, request_json["parameters"]["path"]);
                json data_array = json::array();
                for (const auto& entry : entries) {
                    data_array.push_back({{"name", entry.name}, {"is_directory", entry.is_directory}});
                }
                response_json["data"] = data_array;
            } else if (operation == "dir_delete") {
                dir_delete(g_fs_instance, request_json["parameters"]["path"]);
            } else if (operation == "dir_exists") {
                response_json["data"]["exists"] = dir_exists(g_fs_instance, request_json["parameters"]["path"]);
            }
            // --- UNKNOWN ---
            else {
                response_json["status"] = "error";
                response_json["error_message"] = "Unknown operation.";
            }
        } catch (json::parse_error& e) {
            response_json["status"] = "error";
            response_json["error_message"] = "Invalid JSON received: " + std::string(e.what());
        } catch (std::exception& e) {
            response_json["status"] = "error";
            response_json["error_message"] = "An internal server error occurred: " + std::string(e.what());
        }
        
        std::string response_str = response_json.dump();
        send(req.client_socket, response_str.c_str(), response_str.length(), 0);
        close(req.client_socket);
    }
}