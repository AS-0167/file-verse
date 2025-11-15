#include <iostream>
#include <filesystem>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <netinet/in.h>
#include <unistd.h>
#include "fs_core.h"
#include "user_manager.h"
#include "dir_manager.h"
#include "file_manager.h"
#include "metadata.h"
#include "odf_types.hpp"

#define PORT 8080
#define MAX_CONN 10
#define BUFFER_SIZE 8192

namespace fs = std::filesystem;
using namespace std;

// Global OFS Components
FSInstance fs_inst;
user_manager* um;
dir_manager* dm;
file_manager* fm;
metadata* meta;

// --- Helper Functions ---

void send_msg(int sock, const string& msg) {
    send(sock, msg.c_str(), msg.size(), 0);
}

string recv_msg(int sock) {
    char buffer[BUFFER_SIZE];
    int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0) return "";
    buffer[n] = 0;
    return string(buffer);
}

string build_response(
    const std::string &operation,
    const std::string &session_id,
    const std::string &param1,
    const std::string &param2,
    const std::string &request_id
) {
    std::string msg = "{\n";
    msg += "  \"operation\": \"" + operation + "\",\n";
    msg += "  \"session_id\": \"" + session_id + "\",\n";
    msg += "  \"parameters\": {\n";
    msg += "    \"param1\": \"" + param1 + "\",\n";
    msg += "    \"param2\": \"" + param2 + "\"\n";
    msg += "  },\n";
    msg += "  \"request_id\": \"" + request_id + "\"\n";
    msg += "}\n";
    return msg;
}


// Tokenize command with multi-word support for last argument (data)
vector<string> tokenize_command(const string &line) {
    vector<string> out;
    size_t i = 0, len = line.size();

    while (i < len && line[i] == ' ') ++i;
    size_t start = i;
    while (i < len && line[i] != ' ') ++i;
    if (i > start) out.push_back(line.substr(start, i - start));
    while (i < len && line[i] == ' ') ++i;

    start = i;
    while (i < len && line[i] != ' ') ++i;
    if (i > start) out.push_back(line.substr(start, i - start));
    while (i < len && line[i] == ' ') ++i;

    if (i < len) out.push_back(line.substr(i)); // remaining as data
    return out;
}
void send_raw(int sock, const char* data, size_t len) {
    send(sock, data, len, 0);
}

// Map OFSErrorCodes to string
string error_to_string(OFSErrorCodes code) {
    switch (code) {
        case OFSErrorCodes::SUCCESS: return "SUCCESS";
        case OFSErrorCodes::ERROR_NOT_FOUND: return "ERROR_NOT_FOUND";
        case OFSErrorCodes::ERROR_PERMISSION_DENIED: return "ERROR_PERMISSION_DENIED";
        case OFSErrorCodes::ERROR_IO_ERROR: return "ERROR_IO_ERROR";
        case OFSErrorCodes::ERROR_INVALID_PATH: return "ERROR_INVALID_PATH";
        case OFSErrorCodes::ERROR_FILE_EXISTS: return "ERROR_EXISTS";
        case OFSErrorCodes::ERROR_NO_SPACE: return "ERROR_NO_SPACE";
        case OFSErrorCodes::ERROR_INVALID_CONFIG: return "ERROR_INVALID_CONFIG";
        case OFSErrorCodes::ERROR_NOT_IMPLEMENTED: return "ERROR_NOT_IMPLEMENTED";
        case OFSErrorCodes::ERROR_INVALID_SESSION: return "ERROR_INVALID_SESSION";
        case OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY: return "ERROR_DIRECTORY_NOT_EMPTY";
        case OFSErrorCodes::ERROR_INVALID_OPERATION: return "ERROR_INVALID_OPERATION";
        default: return "ERROR_UNKNOWN";
    }
}

    void handle_client(int client_sock) {
    void* session = nullptr;
    send_msg(client_sock, build_response("WELCOME", "", "message", "Welcome to OFS server!", std::to_string(time(nullptr))));

    auto trim = [](const std::string &s) -> std::string {
        size_t start = 0;
        while (start < s.size() && std::isspace(s[start])) start++;
        size_t end = s.size();
        while (end > start && std::isspace(s[end - 1])) end--;
        return s.substr(start, end - start);
    };

    auto tokenize_command = [&](const std::string &line) -> std::vector<std::string> {
        std::vector<std::string> tokens;
        size_t i = 0, len = line.size();

        while (i < len) {
            while (i < len && std::isspace(line[i])) i++;
            if (i >= len) break;
            size_t start = i;
            while (i < len && !std::isspace(line[i])) i++;
            tokens.push_back(trim(line.substr(start, i - start)));
        }

        return tokens;
    };

    while (true) {
        std::string request = recv_msg(client_sock);
        if (request.empty()) break;

        request = trim(request);
        std::vector<std::string> tokens = tokenize_command(request);
        if (tokens.empty()) {
            send_msg(client_sock, build_response("INVALID_COMMAND", session ? std::to_string((uintptr_t)session) : "", "error", "ERROR_INVALID_COMMAND", std::to_string(time(nullptr))));
            continue;
        }

        std::string cmd = tokens[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

        std::string session_id = session ? std::to_string((uintptr_t)session) : "";
        std::string request_id = std::to_string(time(nullptr));

        // ----- User Commands -----
        if (cmd == "LOGIN") {
            if (tokens.size() < 3) {
                send_msg(client_sock, build_response("LOGIN", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
                continue;
            }
            std::string username = tokens[1];
            std::string password = tokens[2];

            cout << "[DEBUG] LOGIN attempt: user='" << username << "' pass='" << password << "'" << endl;
            int res = um->user_login(&session, username.c_str(), password.c_str());

            send_msg(client_sock, build_response("LOGIN", session_id,
                                                 "result", res == 0 ? "SUCCESS_LOGIN" : "ERROR_INVALID_CREDENTIALS",
                                                 request_id));
        }
        else if (cmd == "LOGOUT") {
            if (session) { um->user_logout(session); session = nullptr; }
            send_msg(client_sock, build_response("LOGOUT", session_id, "result", "SUCCESS_LOGOUT", request_id));
            break;
        }
        else if (cmd == "CREATE_USER") {
            if (!session) {
                send_msg(client_sock, build_response("CREATE_USER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            if (tokens.size() < 4) {
                send_msg(client_sock, build_response("CREATE_USER", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
                continue;
            }

            std::string username = tokens[1];
            std::string password = tokens[2];
            int role = std::stoi(tokens[3]);

            int res = um->user_create(session, username.c_str(), password.c_str(), static_cast<UserRole>(role));
            send_msg(client_sock, build_response("CREATE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        else if (cmd == "DELETE_USER") {
            if (!session) {
                send_msg(client_sock, build_response("DELETE_USER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = um->user_delete(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DELETE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        else if (cmd == "LIST_USERS") {
            if (!session) {
                send_msg(client_sock, build_response("LIST_USERS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            UserInfo* users = nullptr;
            int count = 0;
            int res = um->user_list(session, &users, &count);
            if (res == 0) {
                for (int i = 0; i < count; ++i)
                    send_msg(client_sock, build_response("LIST_USERS", session_id, "user", users[i].username, request_id));
            } else send_msg(client_sock, build_response("LIST_USERS", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        else if (cmd == "GET_SESSION_INFO") {
            if (!session) {
                send_msg(client_sock, build_response("GET_SESSION_INFO", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            SessionInfo info;
            int res = um->get_session_info(session, &info);
            send_msg(client_sock, build_response("GET_SESSION_INFO", session_id, "user", res == 0 ? info.user.username : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        // ----- File Commands -----
        else if (cmd == "CREATE") {
            if (!session) {
                send_msg(client_sock, build_response("CREATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            if (tokens.size() < 2) {
                send_msg(client_sock, build_response("CREATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
                continue;
            }

            std::string path = tokens[1];
            send_msg(client_sock, build_response("CREATE", session_id, "message", "Enter content. End with <<<EOF>>>", request_id));

            std::string data, buffer;
            while (true) {
                std::string chunk = recv_msg(client_sock);
                if (chunk.empty()) break;

                buffer += chunk;
                size_t pos = buffer.find("<<<EOF>>>");
                if (pos != std::string::npos) {
                    data += buffer.substr(0, pos);
                    break;
                } else {
                    data += buffer;
                    buffer.clear();
                }
            }

            int res = fm->file_create(session, path.c_str(), data.c_str(), data.size());
            send_msg(client_sock, build_response("CREATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "EDIT") {
            if (!session) {
                send_msg(client_sock, build_response("EDIT", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            if (tokens.size() < 3) {
                send_msg(client_sock, build_response("EDIT", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
                continue;
            }

            std::string path = tokens[1];
            size_t index = std::stoul(tokens[2]);
            send_msg(client_sock, build_response("EDIT", session_id, "message", "Enter new content. End with <<<EOF>>>", request_id));

            std::string data, buffer;
            while (true) {
                std::string chunk = recv_msg(client_sock);
                if (chunk.empty()) break;

                buffer += chunk;
                size_t pos = buffer.find("<<<EOF>>>");
                if (pos != std::string::npos) {
                    data += buffer.substr(0, pos);
                    break;
                } else {
                    data += buffer;
                    buffer.clear();
                }
            }

            int res = fm->file_edit(session, path.c_str(), data.c_str(), data.size(), index);
            send_msg(client_sock, build_response("EDIT", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "READ") {
            if (!session) {
                send_msg(client_sock, build_response("READ", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            if (tokens.size() < 2) {
                send_msg(client_sock, build_response("READ", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
                continue;
            }

            char* buffer = nullptr;
            size_t size = 0;
            int res = fm->file_read(session, tokens[1].c_str(), &buffer, &size);

            if (res == 0) {
                const size_t CHUNK = 4096;
                for (size_t i = 0; i < size; i += CHUNK) {
                    size_t len = std::min(CHUNK, size - i);
                    send_raw(client_sock, buffer + i, len); // keep raw
                }

                send_msg(client_sock, build_response("READ", session_id, "status", "EOF_REACHED", request_id));
                meta->free_buffer(buffer);
            } else {
                send_msg(client_sock, build_response("READ", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
            }
        }



        else if (cmd == "DELETE_FILE") {
            if (!session) {
                send_msg(client_sock, build_response("DELETE_FILE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = fm->file_delete(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DELETE_FILE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "TRUNCATE") {
            if (!session) {
                send_msg(client_sock, build_response("TRUNCATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = fm->file_truncate(session, tokens[1].c_str());
            send_msg(client_sock, build_response("TRUNCATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "FILE_EXISTS") {
            if (!session) {
                send_msg(client_sock, build_response("FILE_EXISTS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = fm->file_exists(session, tokens[1].c_str());
            send_msg(client_sock, build_response("FILE_EXISTS", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "RENAME_FILE") {
            if (!session) {
                send_msg(client_sock, build_response("RENAME_FILE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = fm->file_rename(session, tokens[1].c_str(), tokens[2].c_str());
            send_msg(client_sock, build_response("RENAME_FILE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        // ----- Directory Commands -----
        else if (cmd == "DIR_CREATE") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_CREATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = dm->dir_create(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DIR_CREATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "DIR_LIST") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_LIST", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            FileEntry* entries = nullptr;
            int count = 0;
            int res = dm->dir_list(session, tokens[1].c_str(), &entries, &count);
            if (res == 0) {
                for (int i = 0; i < count; ++i)
                    send_msg(client_sock, build_response("DIR_LIST", session_id, "entry", entries[i].name, request_id));
            } else {
                send_msg(client_sock, build_response("DIR_LIST", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
            }
        }

        else if (cmd == "DELETE_DIR") {
            if (!session) {
                send_msg(client_sock, build_response("DELETE_DIR", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = dm->dir_delete(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DELETE_DIR", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "DIR_EXISTS") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_EXISTS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = dm->dir_exists(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DIR_EXISTS", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        // ----- Metadata Commands -----
        else if (cmd == "GET_METADATA") {
            if (!session) {
                send_msg(client_sock, build_response("GET_METADATA", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            FileMetadata meta_out;
            int res = meta->get_metadata(session, tokens[1].c_str(), &meta_out);
            send_msg(client_sock, build_response("GET_METADATA", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "SET_PERMISSIONS") {
            if (!session) {
                send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            uint32_t perms = std::stoul(tokens[2]);
            int res = meta->set_permissions(session, tokens[1].c_str(), perms);
            send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "GET_STATS") {
            if (!session) {
                send_msg(client_sock, build_response("GET_STATS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            FSStats stats;
            int res = meta->get_stats(session, &stats);
            send_msg(client_sock, build_response("GET_STATS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "SET_OWNER") {
            if (!session) {
                send_msg(client_sock, build_response("SET_OWNER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
                continue;
            }
            int res = meta->set_owner(session, tokens[1], tokens[2]);
            send_msg(client_sock, build_response("SET_OWNER", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else {
            send_msg(client_sock, build_response("UNKNOWN_COMMAND", session_id, "error", "ERROR_UNKNOWN_COMMAND", request_id));
        }
    }

    if (session) { um->user_logout(session); session = nullptr; }
    close(client_sock);
}


// --- Main server ---
int main() {
    FSInstance* fs_inst = nullptr;

    if (fs::exists("file.omni")) {
        cout <<"1";
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
        cerr << "FS Init failed!" << endl;
        return 1;
    }
}
else{
    cout <<"0 here\n";
    fs_format("file.omni", "default_config.txt");

    if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
        cerr << "FS Init failed!" << endl;
        return 1;
    }
}


    um = new user_manager(fs_inst->users);
    dm = new dir_manager(fs_inst->root, um);
    fm = new file_manager(fs_inst, um);
    meta = new metadata(fs_inst);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) { perror("socket failed"); exit(EXIT_FAILURE); }

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) { perror("bind failed"); exit(EXIT_FAILURE); }
    if (listen(server_fd, MAX_CONN) < 0) { perror("listen"); exit(EXIT_FAILURE); }

    cout << "OFS Server listening on port " << PORT << endl;

    while (true) {
        int client_sock = accept(server_fd, nullptr, nullptr);
        if (client_sock < 0) { perror("accept"); continue; }
        thread(handle_client, client_sock).detach();
    }

    fs_shutdown(fs_inst);
    return 0;
}

/*g++ -std=c++17 \
-Isource/include \
-Isource/include/core \
source/core/*.cpp \
source/server/server.cpp \
source/*.cpp \
-o ofs_server \
-pthread -lssl -lcrypto

*/