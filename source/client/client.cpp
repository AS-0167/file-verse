// In file: source/client/client.cpp

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <vector>

#include "include/json.hpp"

using json = nlohmann::json;

int main() {
    std::cout << "OFS Client. Type 'help' for commands or 'exit' to quit." << std::endl;
    std::string line;

    while (true) {
        std::cout << "ofs> ";
        std::getline(std::cin, line);
        if (line == "exit") break;

        std::stringstream ss(line);
        std::string command;
        ss >> command;
        json request_json;
        request_json["parameters"] = json::object();
        
        // --- CLIENT-SIDE COMMAND PARSING ---
        if (command == "help") {
            std::cout << "Commands:\n"
            << " user_login <user> <pass>\n"
            << " user_create <user> <pass> <role(0/1)>\n"
            << " user_delete <user>\n"
            << " user_list\n"
            << " dir_create <path>\n"
            << " dir_list <path>\n"
            << " dir_delete <path>\n"
            << " dir_exists <path>\n"
            << " file_create <path> <content...>\n"
            << " file_read <path>\n"
            << " file_delete <path>\n"
            << " file_exists <path>\n"
            << " file_rename <old_path> <new_path>\n"
            << " exit\n";
            continue;
        } else if (command == "user_login") {
            request_json["operation"] = "user_login";
            ss >> request_json["parameters"]["username"] >> request_json["parameters"]["password"];
        } else if (command == "user_create") {
            request_json["operation"] = "user_create";
            ss >> request_json["parameters"]["username"] >> request_json["parameters"]["password"] >> request_json["parameters"]["role"];
        } else if (command == "user_delete") {
            request_json["operation"] = "user_delete";
            ss >> request_json["parameters"]["username"];
        } else if (command == "user_list") {
            request_json["operation"] = "user_list";
        } else if (command == "dir_create") {
            request_json["operation"] = "dir_create";
            ss >> request_json["parameters"]["path"];
        } else if (command == "dir_list") {
            request_json["operation"] = "dir_list";
            ss >> request_json["parameters"]["path"];
        } else if (command == "dir_delete") {
            request_json["operation"] = "dir_delete";
            ss >> request_json["parameters"]["path"];
        } else if (command == "dir_exists") {
            request_json["operation"] = "dir_exists";
            ss >> request_json["parameters"]["path"];
        } else if (command == "file_create") {
            request_json["operation"] = "file_create";
            ss >> request_json["parameters"]["path"];
            std::string content; std::getline(ss, content);
            // *** THIS IS THE CORRECTED LINE ***
            if (!content.empty() && content.rfind(" ", 0) == 0) content.erase(0, 1);
            request_json["parameters"]["data"] = content;
        } else if (command == "file_read") {
            request_json["operation"] = "file_read";
            ss >> request_json["parameters"]["path"];
        } else if (command == "file_delete") {
            request_json["operation"] = "file_delete";
            ss >> request_json["parameters"]["path"];
        } else if (command == "file_exists") {
            request_json["operation"] = "file_exists";
            ss >> request_json["parameters"]["path"];
        } else if (command == "file_rename") {
            request_json["operation"] = "file_rename";
            ss >> request_json["parameters"]["old_path"] >> request_json["parameters"]["new_path"];
        } else {
            std::cerr << "Unknown command." << std::endl;
            continue;
        }
        
        // --- SEND REQUEST AND RECEIVE RESPONSE ---
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { std::cerr << "Socket creation error." << std::endl; continue; }
        
        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection Failed." << std::endl; close(sock); continue;
        }
        
        std::string request_str = request_json.dump();
        send(sock, request_str.c_str(), request_str.length(), 0);

        std::vector<char> buffer(4096, 0);
        int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
        if (bytes_received > 0) {
            try {
                json response_json = json::parse(std::string(buffer.data(), bytes_received));
                std::cout << response_json.dump(4) << std::endl;
            } catch (json::parse_error& e) {
                std::cerr << "Error parsing server response: " << e.what() << std::endl;
                std::cerr << "Raw response: " << std::string(buffer.data(), bytes_received) << std::endl;
            }
        }
        close(sock);
    }
    return 0;
}