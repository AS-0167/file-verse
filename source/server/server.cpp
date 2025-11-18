
#include <iostream>
#include <filesystem>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <cctype>
#include <mutex>
#include <random>
#include <signal.h>

#include "fs_core.h"
#include "user_manager.h"
#include "dir_manager.h"
#include "file_manager.h"
#include "metadata.h"
#include "odf_types.hpp"
#include <fstream>

#include "RequestQueue.h"

#include "session_manager.h"


#define BUFFER_SIZE 8192
int PORT= 8080;
int MAX_CONN =10;

bool load_server_config(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line, section;

    while (std::getline(file, line)) {
        // trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') continue;

        // detect section
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        // only parse [server] section
        if (section != "server") continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // trim
        auto trim = [](std::string &s){
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
        };
        trim(key); trim(value);

        if (key == "port") PORT = std::stoi(value);
        else if (key == "max_connections") MAX_CONN = std::stoi(value);
    }

    return true;
}
namespace fs = std::filesystem;
using namespace std;

// Global OFS Components (pointers)
FSInstance* fs_inst = nullptr;
user_manager* um = nullptr;
dir_manager* dm = nullptr;
file_manager* fm = nullptr;
metadata* meta = nullptr;

// Global FIFO queue
static RequestQueue req_queue;

// ----------------------- Helper functions -----------------------
std::string read_until_eof(int client_sock, const std::string& eof_marker = "<<<EOF>>>") {
    std::string buffer;
    char recv_buf[1024];

    while (true) {
        ssize_t n = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        if (n <= 0) break;

        buffer.append(recv_buf, n);  // <-- append EXACT n bytes

        size_t pos = buffer.find(eof_marker);
        if (pos != std::string::npos) {
            return buffer.substr(0, pos);
        }
    }

    return buffer;
}

void graceful_shutdown(int signum) {
    cout << "\nSaving filesystem..." << endl;
    fs_shutdown(fs_inst);
    cout << "Saved. Exiting." << endl;
    exit(0);
}


void send_msg(int sock, const string& msg) {
    ssize_t off = 0;
    ssize_t total = (ssize_t)msg.size();
    const char* data = msg.c_str();
    while (off < total) {
        ssize_t n = send(sock, data + off, total - off, 0);
        if (n <= 0) break;
        off += n;
    }
}

string recv_msg(int sock) {
    // Read once (this is same behaviour as your earlier recv_msg).
    char buffer[BUFFER_SIZE];
    int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0) return "";
    buffer[n] = '\0';
    return string(buffer, n);
}

void send_raw(int sock, const char* data, size_t len) {
    ssize_t off = 0;
    while (off < (ssize_t)len) {
        ssize_t n = send(sock, data + off, (size_t)(len - off), 0);
        if (n <= 0) break;
        off += n;
    }
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

// trims both ends (space, tab, CR, LF)
static inline string trim_all(const string& s) {
    size_t b = 0;
    while (b < s.size() && isspace((unsigned char)s[b])) ++b;
    if (b == s.size()) return "";
    size_t e = s.size() - 1;
    while (e > b && isspace((unsigned char)s[e])) --e;
    return s.substr(b, e - b + 1);
}

// simple tokenizer: splits on whitespace, DOES NOT collapse quoted segments
static inline vector<string> tokenize_command(const string &line) {
    vector<string> tokens;
    string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (isspace((unsigned char)c)) {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// ----------------------- Core request handler -----------------------
// Processes ONE enqueued request. It uses session_manager to track session per client.
void handle_client_request(int client_sock, const string& raw_request) {
    // get (or create) session for this client
    void* session = get_session(client_sock);

    // For interactive commands that ask user to send extra data (CREATE/EDIT), we may need
    // to read further from the socket — so keep the socket open until we finish processing.
    // Send welcome only if client hasn't been seen before
    if (!session) {
        send_msg(client_sock, build_response("WELCOME", "", "message", "Welcome to OFS server!", to_string(time(nullptr))));
    }

    string request = trim_all(raw_request);
    if (request.empty()) {
        // nothing to do
        return;
    }

    vector<string> tokens = tokenize_command(request);
    if (tokens.empty()) {
        send_msg(client_sock, build_response("INVALID_COMMAND", session ? to_string((uintptr_t)session) : "", "error", "ERROR_INVALID_COMMAND", to_string(time(nullptr))));
        return;
    }

    string cmd = tokens[0];
    transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    string session_id = session ? to_string((uintptr_t)session) : "";
    string request_id = to_string(time(nullptr));

    // --------- USER COMMANDS ---------
    if (cmd == "LOGIN") {
        if (tokens.size() < 3) {
            send_msg(client_sock, build_response("LOGIN", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
        }
        string username = tokens[1];
        string password = tokens[2];
        int res = um->user_login(&session, username.c_str(), password.c_str());
        if (res == 0) {
            set_session(client_sock, session);
            session_id = to_string((uintptr_t)session);
        }
        send_msg(client_sock, build_response("LOGIN", session_id, "result", res == 0 ? "SUCCESS_LOGIN" : "ERROR_INVALID_CREDENTIALS", request_id));
        return;
    }

    if (cmd == "LOGOUT") {
        if (session) { um->user_logout(session); remove_session(client_sock); session = nullptr; }
        send_msg(client_sock, build_response("LOGOUT", "", "result", "SUCCESS_LOGOUT", request_id));
        return;
    }

    if (cmd == "CREATE_USER") {
        if (!session) { send_msg(client_sock, build_response("CREATE_USER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 4) { send_msg(client_sock, build_response("CREATE_USER", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        string username = tokens[1], password = tokens[2];
        int role = stoi(tokens[3]);
        int res = um->user_create(session, username.c_str(), password.c_str(), static_cast<UserRole>(role));
        send_msg(client_sock, build_response("CREATE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "DELETE_USER") {
        if (!session) { send_msg(client_sock, build_response("DELETE_USER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DELETE_USER", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = um->user_delete(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DELETE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "LIST_USERS") {
        if (!session) { send_msg(client_sock, build_response("LIST_USERS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        UserInfo* users = nullptr; int count = 0;
        int res = um->user_list(session, &users, &count);
        if (res == 0) {
            for (int i = 0; i < count; ++i) {
                send_msg(client_sock, build_response("LIST_USERS", session_id, "user", users[i].username, request_id));
            }
        } else {
            send_msg(client_sock, build_response("LIST_USERS", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }

    if (cmd == "GET_SESSION_INFO") {
        if (!session) { send_msg(client_sock, build_response("GET_SESSION_INFO", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        SessionInfo info; int res = um->get_session_info(session, &info);
        send_msg(client_sock, build_response("GET_SESSION_INFO", session_id, "user", res == 0 ? info.user.username : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    // ------- FILE COMMANDS -------
    if (cmd == "CREATE") {
    if (!session) { 
        send_msg(client_sock, build_response("CREATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); 
        return; 
    }
    if (tokens.size() < 2) { 
        send_msg(client_sock, build_response("CREATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); 
        return; 
    }

    std::string path = tokens[1];

    // ask for content
    send_msg(client_sock, build_response("CREATE", session_id, "message", "Enter content. End with <<<EOF>>>", request_id));

    // read content until <<<EOF>>>
    std::string data = read_until_eof(client_sock);

    int res = fm->file_create(session, path.c_str(), data.c_str(), data.size());
    send_msg(client_sock, build_response("CREATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
    return;
}

if (cmd == "EDIT") {
    if (!session) { 
        send_msg(client_sock, build_response("EDIT", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); 
        return; 
    }
    if (tokens.size() < 3) { 
        send_msg(client_sock, build_response("EDIT", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); 
        return; 
    }

    std::string path = tokens[1];
    size_t index = (size_t)std::stoul(tokens[2]);

    send_msg(client_sock, build_response("EDIT", session_id, "message", "Enter new content. End with <<<EOF>>>", request_id));

    std::string data = read_until_eof(client_sock);

    int res = fm->file_edit(session, path.c_str(), data.c_str(), data.size(), index);
    send_msg(client_sock, build_response("EDIT", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
    return;
}

    if (cmd == "READ") {
        if (!session) { send_msg(client_sock, build_response("READ", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("READ", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        char* buffer = nullptr; size_t size = 0;
        int res = fm->file_read(session, tokens[1].c_str(), &buffer, &size);
        if (res == 0) {
            const size_t CHUNK = 4096;
            for (size_t i = 0; i < size; i += CHUNK) {
                size_t len = std::min(CHUNK, size - i);
                send_raw(client_sock, buffer + i, len);
            }
            send_msg(client_sock, build_response("READ", session_id, "status", "EOF_REACHED", request_id));
            meta->free_buffer(buffer);
        } else {
            send_msg(client_sock, build_response("READ", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }

    if (cmd == "DELETE_FILE") {
        if (!session) { send_msg(client_sock, build_response("DELETE_FILE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DELETE_FILE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = fm->file_delete(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DELETE_FILE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "TRUNCATE") {
        if (!session) { send_msg(client_sock, build_response("TRUNCATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("TRUNCATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = fm->file_truncate(session, tokens[1].c_str());
        send_msg(client_sock, build_response("TRUNCATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "FILE_EXISTS") {
        if (!session) { send_msg(client_sock, build_response("FILE_EXISTS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("FILE_EXISTS", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = fm->file_exists(session, tokens[1].c_str());
        send_msg(client_sock, build_response("FILE_EXISTS", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "RENAME_FILE") {
        if (!session) { send_msg(client_sock, build_response("RENAME_FILE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 3) { send_msg(client_sock, build_response("RENAME_FILE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = fm->file_rename(session, tokens[1].c_str(), tokens[2].c_str());
        send_msg(client_sock, build_response("RENAME_FILE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    // ------- DIRECTORY -------
    if (cmd == "DIR_CREATE") {
        if (!session) { send_msg(client_sock, build_response("DIR_CREATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DIR_CREATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = dm->dir_create(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DIR_CREATE", session_id, "DIRECTORY", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "DIR_LIST") {
        if (!session) { send_msg(client_sock, build_response("DIR_LIST", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DIR_LIST", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        FileEntry* entries = nullptr; int count = 0;
        int res = dm->dir_list(session, tokens[1].c_str(), &entries, &count);
        if (res == 0) {
        for (int i = 0; i < count; ++i) {
            if (string(entries[i].name).empty()) {
            continue;
            }
        string prefix = (entries[i].getType() == EntryType::DIRECTORY) ? "D:" : "F:";
        string combined = prefix + entries[i].name;

        send_msg(client_sock, build_response("DIR_LIST", session_id, "entry", combined, request_id));
        }
        } else {
            send_msg(client_sock, build_response("DIR_LIST", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }

    if (cmd == "DELETE_DIR") {
        if (!session) { send_msg(client_sock, build_response("DELETE_DIR", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DELETE_DIR", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = dm->dir_delete(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DELETE_DIR", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "DIR_EXISTS") {
        if (!session) { send_msg(client_sock, build_response("DIR_EXISTS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DIR_EXISTS", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = dm->dir_exists(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DIR_EXISTS", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    // ------- METADATA -------
    if (cmd == "GET_METADATA") {
        if (!session) { send_msg(client_sock, build_response("GET_METADATA", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("GET_METADATA", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        FileMetadata meta_out;
        int res = meta->get_metadata(session, tokens[1].c_str(), &meta_out);
        if (res == 0) {
            std::string meta_json = "{";
            meta_json += "\"name\":\"" + std::string(meta_out.entry.name) + "\",";
            meta_json += "\"owner\":\"" + std::string(meta_out.entry.owner) + "\",";
            meta_json += "\"size\":" + std::to_string(meta_out.entry.size) + ",";
            meta_json += "\"type\":\"" + std::string(
                meta_out.entry.getType() == EntryType::FILE ? "File" : "Dir"
            ) + "\",";
            meta_json += "\"blocks_used\":" + std::to_string(meta_out.blocks_used) + ",";
            meta_json += "\"permissions\":" + std::to_string(meta_out.entry.permissions);
            meta_json += "}";


    send_msg(client_sock, build_response("GET_METADATA", session_id, "result", meta_json, request_id));
        return; 
        }
        send_msg(client_sock, build_response("GET_METADATA", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        
    }

    if (cmd == "SET_PERMISSIONS") {
        if (!session) { send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 3) { send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        uint32_t perms = (uint32_t)stoul(tokens[2]);
        int res = meta->set_permissions(session, tokens[1].c_str(), perms);
        send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "GET_STATS") {
        if (!session) { send_msg(client_sock, build_response("GET_STATS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        FSStats stats;
        int res = meta->get_stats(session, &stats);
        send_msg(client_sock, build_response("GET_STATS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "SET_OWNER") {
        if (!session) { send_msg(client_sock, build_response("SET_OWNER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 3) { send_msg(client_sock, build_response("SET_OWNER", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = meta->set_owner(session, tokens[1].c_str(), tokens[2].c_str());
        send_msg(client_sock, build_response("SET_OWNER", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "EXIT") {

    remove_session(client_sock);

    // Send goodbye message
    send(client_sock,
         "{ \"operation\": \"EXIT\", \"result\": \"BYE\" }\n",
         40, 0);

    close(client_sock);
    return;
}


    // Unknown command
    send_msg(client_sock, build_response("UNKNOWN_COMMAND", session_id, "error", "ERROR_UNKNOWN_COMMAND", request_id));
}

// ----------------------- worker that pops and processes requests -----------------------
void process_requests() {
    while (true) {
        Request r = req_queue.pop(); // blocks until a request exists
        // Process the single request in FIFO order
        handle_client_request(r.client_sock, r.request);
        // do NOT close client here — client may send more commands; client connection closed in accept loop when client disconnects
    }
}

// ----------------------- accept handling: read requests and enqueue -----------------------
void accept_loop(int server_fd) {
    while (true) {
        int client_sock = accept(server_fd, nullptr, nullptr);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        // We will read from this socket in the accept loop non-blocking fashion: repeatedly receive commands and enqueue them.
        // To keep it simple and safe: spawn a thread per client that reads messages and enqueues them.
        thread([client_sock]() {
            // send initial welcome (worker will also send welcome if session missing)
            send_msg(client_sock, build_response("WELCOME", "", "message", "Welcome to OFS server!", to_string(time(nullptr))));

            while (true) {
                string req = recv_msg(client_sock);
                if (req.empty()) break; // client disconnected or error
                // push the request into FIFO queue (process_requests will handle and reply)
                req_queue.push(client_sock, req);
            }

            // client disconnected: cleanup session record
            remove_session(client_sock);
            close(client_sock);
        }).detach();
    }
}

// ----------------------- main -----------------------
int main() {
     if (!load_server_config("default.uconf")) {
        std::cerr << "Failed to load server config, using defaults.\n";
        return 0;
    }
    // initialize FS
    if (fs::exists("file.omni")) {
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
            cerr << "FS Init failed!" << endl;
            return 1;
        }
    } else {
        fs_format("file.omni", "default_uconf.txt");
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
            cerr << "FS Init failed!" << endl;
            return 1;
        }
    }
    signal(SIGINT, graceful_shutdown);
    signal(SIGTERM, graceful_shutdown);

   // sessions_by_token = new HashTable<Session>(50);
    cout << "fs_inst address: " << fs_inst << endl;
cout << "Block size: " << fs_inst->header.block_size << endl;
    // initialize managers
    um = new user_manager(fs_inst->users);
    dm = new dir_manager(fs_inst->root, um);
    fm = new file_manager(fs_inst, um);
    meta = new metadata(fs_inst);

    // server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket failed"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) { perror("bind failed"); return 1; }
    if (listen(server_fd, MAX_CONN) < 0) { perror("listen failed"); return 1; }

    cout << "OFS Server listening on port " << PORT << endl;

    thread(process_requests).detach();


    accept_loop(server_fd);

    // shutdown
    fs_shutdown(fs_inst);
    return 0;
}







/*
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
#include "RequestQueue.h"
#include "session_manager.h"


RequestQueue req_queue;


#define PORT 8080
#define MAX_CONN 10
#define BUFFER_SIZE 8192

namespace fs = std::filesystem;
using namespace std;

// Global OFS Components
FSInstance* fs_inst;
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

//Handle Client for Request Process
void handle_client_request(int client_sock, const std::string &request) {
    // Get or create session for this client
    void* session = nullptr;
    if (client_sessions.count(client_sock)) {
        session = client_sessions[client_sock];
    }

    // --- Helpers ---
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

    std::string trimmed_request = trim(request);
    if (trimmed_request.empty()) return;

    std::vector<std::string> tokens = tokenize_command(trimmed_request);
    if (tokens.empty()) {
        send_msg(client_sock, build_response(
            "INVALID_COMMAND",
            session ? std::to_string((uintptr_t)session) : "",
            "error",
            "ERROR_INVALID_COMMAND",
            std::to_string(time(nullptr))
        ));
        return;
    }

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    std::string session_id = session ? std::to_string((uintptr_t)session) : "";
    std::string request_id = std::to_string(time(nullptr));

    // ----------------- User Commands -----------------
    if (cmd == "LOGIN") {
        if (tokens.size() < 3) {
            send_msg(client_sock, build_response("LOGIN", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
        }

        std::string username = tokens[1];
        std::string password = tokens[2];

        int res = um->user_login(&session, username.c_str(), password.c_str());
        if (res == 0) client_sessions[client_sock] = session;

        send_msg(client_sock, build_response("LOGIN", session_id,
                                             "result",
                                             res == 0 ? "SUCCESS_LOGIN" : "ERROR_INVALID_CREDENTIALS",
                                             request_id));
        return;
    }

    if (cmd == "LOGOUT") {
        if (session) {
            um->user_logout(session);
            session = nullptr;
            client_sessions.erase(client_sock);
        }
        send_msg(client_sock, build_response("LOGOUT", session_id, "result", "SUCCESS_LOGOUT", request_id));
        return;
    }

    // All other commands require a valid session
    if (!session) {
        send_msg(client_sock, build_response(cmd, session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
        return;
    }

    // ----- User Management -----
    if (cmd == "CREATE_USER") {
        if (tokens.size() < 4) {
            send_msg(client_sock, build_response("CREATE_USER", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
        }
        std::string username = tokens[1];
        std::string password = tokens[2];
        int role = std::stoi(tokens[3]);
        int res = um->user_create(session, username.c_str(), password.c_str(), static_cast<UserRole>(role));
        send_msg(client_sock, build_response("CREATE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "DELETE_USER") {
        int res = um->user_delete(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DELETE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "LIST_USERS") {
        UserInfo* users = nullptr;
        int count = 0;
        int res = um->user_list(session, &users, &count);
        if (res == 0) {
            for (int i = 0; i < count; ++i)
                send_msg(client_sock, build_response("LIST_USERS", session_id, "user", users[i].username, request_id));
        } else {
            send_msg(client_sock, build_response("LIST_USERS", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }

    if (cmd == "GET_SESSION_INFO") {
        SessionInfo info;
        int res = um->get_session_info(session, &info);
        send_msg(client_sock, build_response("GET_SESSION_INFO", session_id,
                                             "user",
                                             res == 0 ? info.user.username : error_to_string(static_cast<OFSErrorCodes>(res)),
                                             request_id));
        return;
    }

    // ----------------- File Commands -----------------
    if (cmd == "CREATE") {
        if (tokens.size() < 2) {
            send_msg(client_sock, build_response("CREATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
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
        return;
    }

    if (cmd == "EDIT") {
        if (tokens.size() < 3) {
            send_msg(client_sock, build_response("EDIT", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
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
        return;
    }

    if (cmd == "READ") {
        if (tokens.size() < 2) {
            send_msg(client_sock, build_response("READ", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
        }
        char* buffer = nullptr;
        size_t size = 0;
        int res = fm->file_read(session, tokens[1].c_str(), &buffer, &size);
        if (res == 0) {
            const size_t CHUNK = 4096;
            for (size_t i = 0; i < size; i += CHUNK) {
                size_t len = std::min(CHUNK, size - i);
                send_raw(client_sock, buffer + i, len);
            }
            send_msg(client_sock, build_response("READ", session_id, "status", "EOF_REACHED", request_id));
            meta->free_buffer(buffer);
        } else {
            send_msg(client_sock, build_response("READ", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }
        // ----- Directory Commands -----
        else if (cmd == "DIR_CREATE") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_CREATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            int res = dm->dir_create(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DIR_CREATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "DIR_LIST") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_LIST", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
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
            return;
                
            }
            int res = dm->dir_delete(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DELETE_DIR", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "DIR_EXISTS") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_EXISTS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            int res = dm->dir_exists(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DIR_EXISTS", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        // ----- Metadata Commands -----
        else if (cmd == "GET_METADATA") {
            if (!session) {
                send_msg(client_sock, build_response("GET_METADATA", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
            
            }
            FileMetadata meta_out;
            int res = meta->get_metadata(session, tokens[1].c_str(), &meta_out);
            send_msg(client_sock, build_response("GET_METADATA", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "SET_PERMISSIONS") {
            if (!session) {
                send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            uint32_t perms = std::stoul(tokens[2]);
            int res = meta->set_permissions(session, tokens[1].c_str(), perms);
            send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "GET_STATS") {
            if (!session) {
                send_msg(client_sock, build_response("GET_STATS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            FSStats stats;
            int res = meta->get_stats(session, &stats);
            send_msg(client_sock, build_response("GET_STATS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "SET_OWNER") {
            if (!session) {
                send_msg(client_sock, build_response("SET_OWNER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            int res = meta->set_owner(session, tokens[1], tokens[2]);
            send_msg(client_sock, build_response("SET_OWNER", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else {
            send_msg(client_sock, build_response("UNKNOWN_COMMAND", session_id, "error", "ERROR_UNKNOWN_COMMAND", request_id));
        }
    

    if (session) { um->user_logout(session); session = nullptr; }
    //close(client_sock);   
}

void handle_client(int client_sock) {
    send_msg(client_sock, build_response("WELCOME", "", "message", "Welcome to OFS server!", std::to_string(time(nullptr))));
    
    while (true) {
        std::string request = recv_msg(client_sock);
        if (request.empty()) break; // client disconnected
        req_queue.push({client_sock, request}); // enqueue command
    }

    close(client_sock); // when client disconnects
}


void process_requests() {
    while (true) {
        Request req = req_queue.pop();  // FIFO
        handle_client_request(req.client_sock, req.command); // pass the command
    }
}

// --- Main server ---
int main() {
    fs_inst = nullptr;

    if (fs::exists("file.omni")) {
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
            cerr << "FS Init failed!" << endl;
            return 1;
        }
    } else {
        fs_format("file.omni", "default_config.txt");
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
            cerr << "FS Init failed!" << endl;
            return 1;
        }
    }

    // Initialize managers
    um = new user_manager(fs_inst->users);
    dm = new dir_manager(fs_inst->root, um);
    fm = new file_manager(fs_inst, um);
    meta = new metadata(fs_inst);

    // Setup server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket failed"); exit(EXIT_FAILURE); }

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CONN) < 0) {
        perror("listen failed"); exit(EXIT_FAILURE);
    }

    cout << "OFS Server listening on port " << PORT << endl;

    // Start worker thread to process requests FIFO
    std::thread worker(process_requests);
    worker.detach();


    // Accept loop: enqueue client sockets
    while (true) {
        int client_sock = accept(server_fd, nullptr, nullptr);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }
        req_queue.push({client_sock}); // enqueue request
    }

    fs_shutdown(fs_inst);
    return 0;
}*/

/*g++ -std=c++17 \
-Isource/include \
-Isource/include/core \
source/server/*.cpp \
source/core/*.cpp \
source/data_structures/*.cpp \
-o ofs_server \
-pthread -lssl -lcrypto


*/

/*
#include <iostream>
#include <filesystem>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <cctype>
#include <mutex>
#include <random>
#include <signal.h>
#include <fstream>

#include "fs_core.h"
#include "user_manager.h"
#include "dir_manager.h"
#include "file_manager.h"
#include "metadata.h"
#include "odf_types.hpp"

#include "RequestQueue.h"

#include "session_manager.h"
/*
#define PORT 8080
#define MAX_CONN 10*/

/*#define BUFFER_SIZE 8192

int PORT;
int MAX_CONN;

#include <fstream>
#include <string>
#include <sstream>

bool load_server_config(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line, section;

    while (std::getline(file, line)) {
        // trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') continue;

        // detect section
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        // only parse [server] section
        if (section != "server") continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // trim
        auto trim = [](std::string &s){
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
        };
        trim(key); trim(value);

        if (key == "port") PORT = std::stoi(value);
        else if (key == "max_connections") MAX_CONN = std::stoi(value);
    }

    return true;
}


namespace fs = std::filesystem;
using namespace std;

// Global OFS Components (pointers)
FSInstance* fs_inst = nullptr;
user_manager* um = nullptr;
dir_manager* dm = nullptr;
file_manager* fm = nullptr;
metadata* meta = nullptr;

// Global FIFO queue
static RequestQueue req_queue;

// ----------------------- Helper functions -----------------------
std::string read_until_eof(int client_sock, const std::string& eof_marker = "<<<EOF>>>") {
    std::string buffer; // accumulates all received data
    char recv_buf[1024];
    
    while (true) {
        ssize_t n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0) break;
        
        recv_buf[n] = '\0';
        buffer += recv_buf;
        
        // Check if EOF marker exists in buffer
        size_t pos = buffer.find(eof_marker);
        if (pos != std::string::npos) {
            // Return only the content before EOF marker
            return buffer.substr(0, pos);
        }
        // Continue reading if EOF not found yet
    }
    
    return buffer;  // Return accumulated data if connection closed
}

void graceful_shutdown(int signum) {
    cout << "\nSaving filesystem..." << endl;
    fs_shutdown(fs_inst);
    cout << "Saved. Exiting." << endl;
    exit(0);
}


void send_msg(int sock, const string& msg) {
    ssize_t off = 0;
    ssize_t total = (ssize_t)msg.size();
    const char* data = msg.c_str();
    while (off < total) {
        ssize_t n = send(sock, data + off, total - off, 0);
        if (n <= 0) break;
        off += n;
    }
}

string recv_msg(int sock) {
    // Read once (this is same behaviour as your earlier recv_msg).
    char buffer[BUFFER_SIZE];
    int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0) return "";
    buffer[n] = '\0';
    return string(buffer, n);
}

void send_raw(int sock, const char* data, size_t len) {
    ssize_t off = 0;
    while (off < (ssize_t)len) {
        ssize_t n = send(sock, data + off, (size_t)(len - off), 0);
        if (n <= 0) break;
        off += n;
    }
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

// trims both ends (space, tab, CR, LF)
static inline string trim_all(const string& s) {
    size_t b = 0;
    while (b < s.size() && isspace((unsigned char)s[b])) ++b;
    if (b == s.size()) return "";
    size_t e = s.size() - 1;
    while (e > b && isspace((unsigned char)s[e])) --e;
    return s.substr(b, e - b + 1);
}

// simple tokenizer: splits on whitespace, DOES NOT collapse quoted segments
static inline vector<string> tokenize_command(const string &line) {
    vector<string> tokens;
    string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (isspace((unsigned char)c)) {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// ----------------------- Core request handler -----------------------
// Processes ONE enqueued request. It uses session_manager to track session per client.
void handle_client_request(int client_sock, const string& raw_request) {
    // get (or create) session for this client
    void* session = get_session(client_sock);

    // For interactive commands that ask user to send extra data (CREATE/EDIT), we may need
    // to read further from the socket — so keep the socket open until we finish processing.
    // Send welcome only if client hasn't been seen before
    if (!session) {
        send_msg(client_sock, build_response("WELCOME", "", "message", "Welcome to OFS server!", to_string(time(nullptr))));
    }

    string request = trim_all(raw_request);
    if (request.empty()) {
        // nothing to do
        return;
    }

    vector<string> tokens = tokenize_command(request);
    if (tokens.empty()) {
        send_msg(client_sock, build_response("INVALID_COMMAND", session ? to_string((uintptr_t)session) : "", "error", "ERROR_INVALID_COMMAND", to_string(time(nullptr))));
        return;
    }

    string cmd = tokens[0];
    transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    string session_id = session ? to_string((uintptr_t)session) : "";
    string request_id = to_string(time(nullptr));

    // --------- USER COMMANDS ---------
    if (cmd == "LOGIN") {
        if (tokens.size() < 3) {
            send_msg(client_sock, build_response("LOGIN", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
        }
        string username = tokens[1];
        string password = tokens[2];
        int res = um->user_login(&session, username.c_str(), password.c_str());
        if (res == 0) {
            set_session(client_sock, session);
            session_id = to_string((uintptr_t)session);
        }
        send_msg(client_sock, build_response("LOGIN", session_id, "result", res == 0 ? "SUCCESS_LOGIN" : "ERROR_INVALID_CREDENTIALS", request_id));
        return;
    }

    if (cmd == "LOGOUT") {
        if (session) { um->user_logout(session); remove_session(client_sock); session = nullptr; }
        send_msg(client_sock, build_response("LOGOUT", "", "result", "SUCCESS_LOGOUT", request_id));
        return;
    }

    if (cmd == "CREATE_USER") {
        if (!session) { send_msg(client_sock, build_response("CREATE_USER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 4) { send_msg(client_sock, build_response("CREATE_USER", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        string username = tokens[1], password = tokens[2];
        int role = stoi(tokens[3]);
        int res = um->user_create(session, username.c_str(), password.c_str(), static_cast<UserRole>(role));
        send_msg(client_sock, build_response("CREATE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "DELETE_USER") {
        if (!session) { send_msg(client_sock, build_response("DELETE_USER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DELETE_USER", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = um->user_delete(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DELETE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "LIST_USERS") {
        if (!session) { send_msg(client_sock, build_response("LIST_USERS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        UserInfo* users = nullptr; int count = 0;
        int res = um->user_list(session, &users, &count);
        if (res == 0) {
            for (int i = 0; i < count; ++i) {
                send_msg(client_sock, build_response("LIST_USERS", session_id, "user", users[i].username, request_id));
            }
        } else {
            send_msg(client_sock, build_response("LIST_USERS", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }

    if (cmd == "GET_SESSION_INFO") {
        if (!session) { send_msg(client_sock, build_response("GET_SESSION_INFO", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        SessionInfo info; int res = um->get_session_info(session, &info);
        send_msg(client_sock, build_response("GET_SESSION_INFO", session_id, "user", res == 0 ? info.user.username : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    // ------- FILE COMMANDS -------
    if (cmd == "CREATE") {
    if (!session) { 
        send_msg(client_sock, build_response("CREATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); 
        return; 
    }
    if (tokens.size() < 2) { 
        send_msg(client_sock, build_response("CREATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); 
        return; 
    }

    std::string path = tokens[1];

    // ask for content
    send_msg(client_sock, build_response("CREATE", session_id, "message", "Enter content. End with <<<EOF>>>", request_id));

    // read content until <<<EOF>>>
    std::string data = read_until_eof(client_sock);

    int res = fm->file_create(session, path.c_str(), data.c_str(), data.size());
    send_msg(client_sock, build_response("CREATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
    return;
}

if (cmd == "EDIT") {
    if (!session) { 
        send_msg(client_sock, build_response("EDIT", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); 
        return; 
    }
    if (tokens.size() < 3) { 
        send_msg(client_sock, build_response("EDIT", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); 
        return; 
    }

    std::string path = tokens[1];
    size_t index = (size_t)std::stoul(tokens[2]);

    send_msg(client_sock, build_response("EDIT", session_id, "message", "Enter new content. End with <<<EOF>>>", request_id));

    std::string data = read_until_eof(client_sock);

    int res = fm->file_edit(session, path.c_str(), data.c_str(), data.size(), index);
    send_msg(client_sock, build_response("EDIT", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
    return;
}

    if (cmd == "READ") {
        if (!session) { send_msg(client_sock, build_response("READ", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("READ", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        char* buffer = nullptr; size_t size = 0;
        int res = fm->file_read(session, tokens[1].c_str(), &buffer, &size);
        if (res == 0) {
            const size_t CHUNK = 4096;
            for (size_t i = 0; i < size; i += CHUNK) {
                size_t len = std::min(CHUNK, size - i);
                send_raw(client_sock, buffer + i, len);
            }
            send_msg(client_sock, build_response("READ", session_id, "status", "EOF_REACHED", request_id));
            meta->free_buffer(buffer);
        } else {
            send_msg(client_sock, build_response("READ", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }

    if (cmd == "DELETE_FILE") {
        if (!session) { send_msg(client_sock, build_response("DELETE_FILE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DELETE_FILE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = fm->file_delete(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DELETE_FILE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "TRUNCATE") {
        if (!session) { send_msg(client_sock, build_response("TRUNCATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("TRUNCATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = fm->file_truncate(session, tokens[1].c_str());
        send_msg(client_sock, build_response("TRUNCATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "FILE_EXISTS") {
        if (!session) { send_msg(client_sock, build_response("FILE_EXISTS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("FILE_EXISTS", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = fm->file_exists(session, tokens[1].c_str());
        send_msg(client_sock, build_response("FILE_EXISTS", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "RENAME_FILE") {
        if (!session) { send_msg(client_sock, build_response("RENAME_FILE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 3) { send_msg(client_sock, build_response("RENAME_FILE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = fm->file_rename(session, tokens[1].c_str(), tokens[2].c_str());
        send_msg(client_sock, build_response("RENAME_FILE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    // ------- DIRECTORY -------
    if (cmd == "DIR_CREATE") {
        if (!session) { send_msg(client_sock, build_response("DIR_CREATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DIR_CREATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = dm->dir_create(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DIR_CREATE", session_id, "DIRECTORY", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "DIR_LIST") {
        if (!session) { send_msg(client_sock, build_response("DIR_LIST", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DIR_LIST", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        FileEntry* entries = nullptr; int count = 0;
        int res = dm->dir_list(session, tokens[1].c_str(), &entries, &count);
        if (res == 0) {
        for (int i = 0; i < count; ++i) {
            if (string(entries[i].name).empty()) {
            continue;
            }
        string prefix = (entries[i].getType() == EntryType::DIRECTORY) ? "D:" : "F:";
        string combined = prefix + entries[i].name;

        send_msg(client_sock, build_response("DIR_LIST", session_id, "entry", combined, request_id));
        }
        } else {
            send_msg(client_sock, build_response("DIR_LIST", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }

    if (cmd == "DELETE_DIR") {
        if (!session) { send_msg(client_sock, build_response("DELETE_DIR", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DELETE_DIR", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = dm->dir_delete(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DELETE_DIR", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "DIR_EXISTS") {
        if (!session) { send_msg(client_sock, build_response("DIR_EXISTS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("DIR_EXISTS", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = dm->dir_exists(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DIR_EXISTS", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    // ------- METADATA -------
    if (cmd == "GET_METADATA") {
        if (!session) { send_msg(client_sock, build_response("GET_METADATA", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 2) { send_msg(client_sock, build_response("GET_METADATA", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        FileMetadata meta_out;
        int res = meta->get_metadata(session, tokens[1].c_str(), &meta_out);
        if (res == 0) {
    // success → build a detailed metadata JSON string
        std::string meta_json = "{";
        meta_json += "\"name\":\"" + std::string(meta_out.entry.name) + "\",";
        meta_json += "\"owner\":\"" + string(meta_out.entry.owner) + "\",";
        meta_json += "\"size\":" + std::to_string(meta_out.entry.size) + ",";
        meta_json += "\"type\":\"" + std::string(
        meta_out.entry.getType() == EntryType::FILE ? "File" : "Dir"
    ) + "\"";
    meta_json += "}";

    send_msg(client_sock, build_response("GET_METADATA", session_id, "result", meta_json, request_id));
} 
else {
    send_msg(client_sock, build_response("GET_METADATA", session_id, "error", 
        error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
}

    }

    if (cmd == "SET_PERMISSIONS") {
        if (!session) { send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 3) { send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        uint32_t perms = (uint32_t)stoul(tokens[2]);
        int res = meta->set_permissions(session, tokens[1].c_str(), perms);
        send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "GET_STATS") {
        if (!session) { send_msg(client_sock, build_response("GET_STATS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        FSStats stats;
        int res = meta->get_stats(session, &stats);
        send_msg(client_sock, build_response("GET_STATS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "SET_OWNER") {
        if (!session) { send_msg(client_sock, build_response("SET_OWNER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id)); return; }
        if (tokens.size() < 3) { send_msg(client_sock, build_response("SET_OWNER", session_id, "error", "ERROR_INVALID_COMMAND", request_id)); return; }
        int res = meta->set_owner(session, tokens[1].c_str(), tokens[2].c_str());
        send_msg(client_sock, build_response("SET_OWNER", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "EXIT") {

    remove_session(client_sock);

    // Send goodbye message
    send(client_sock,
         "{ \"operation\": \"EXIT\", \"result\": \"BYE\" }\n",
         40, 0);

    close(client_sock);
    return;
}


    // Unknown command
    send_msg(client_sock, build_response("UNKNOWN_COMMAND", session_id, "error", "ERROR_UNKNOWN_COMMAND", request_id));
}

// ----------------------- worker that pops and processes requests -----------------------
void process_requests() {
    while (true) {
        Request r = req_queue.pop(); // blocks until a request exists
        // Process the single request in FIFO order
        handle_client_request(r.client_sock, r.request);
        // do NOT close client here — client may send more commands; client connection closed in accept loop when client disconnects
    }
}

// ----------------------- accept handling: read requests and enqueue -----------------------
void accept_loop(int server_fd) {
    while (true) {
        int client_sock = accept(server_fd, nullptr, nullptr);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        // We will read from this socket in the accept loop non-blocking fashion: repeatedly receive commands and enqueue them.
        // To keep it simple and safe: spawn a thread per client that reads messages and enqueues them.
        thread([client_sock]() {
            // send initial welcome (worker will also send welcome if session missing)
            send_msg(client_sock, build_response("WELCOME", "", "message", "Welcome to OFS server!", to_string(time(nullptr))));

            while (true) {
                string req = recv_msg(client_sock);
                if (req.empty()) break; // client disconnected or error
                // push the request into FIFO queue (process_requests will handle and reply)
                req_queue.push(client_sock, req);
            }

            // client disconnected: cleanup session record
            remove_session(client_sock);
            close(client_sock);
        }).detach();
    }
}

// ----------------------- main -----------------------
int main() {
    if (!load_server_config("default.uconf")) {
        std::cerr << "Failed to load server config, using defaults.\n";
        return 0;
    }


    // initialize FS
    if (fs::exists("file.omni")) {
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
            cerr << "FS Init failed!" << endl;
            return 1;
        }
    } else {
        cout << 1;
        fs_format("file.omni", "default_uconf.txt");
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
            cerr << "FS Init failed!" << endl;
            return 1;
        }
    }
    signal(SIGINT, graceful_shutdown);
    signal(SIGTERM, graceful_shutdown);

   // sessions_by_token = new HashTable<Session>(50);
    cout << "fs_inst address: " << fs_inst << endl;
    cout << "Block size: " << fs_inst->header.block_size << endl;
    // initialize managers
    um = new user_manager(fs_inst->users);
    dm = new dir_manager(fs_inst->root, um);
    fm = new file_manager(fs_inst, um);
    meta = new metadata(fs_inst);

    // server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket failed"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) { perror("bind failed"); return 1; }
    if (listen(server_fd, MAX_CONN) < 0) { perror("listen failed"); return 1; }

    cout << "OFS Server listening on port " << PORT << endl;

    thread(process_requests).detach();


    accept_loop(server_fd);

    // shutdown
    fs_shutdown(fs_inst);
    return 0;
}


*/





/*
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
#include "RequestQueue.h"
#include "session_manager.h"


RequestQueue req_queue;


#define PORT 8080
#define MAX_CONN 10
#define BUFFER_SIZE 8192

namespace fs = std::filesystem;
using namespace std;

// Global OFS Components
FSInstance* fs_inst;
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

//Handle Client for Request Process
void handle_client_request(int client_sock, const std::string &request) {
    // Get or create session for this client
    void* session = nullptr;
    if (client_sessions.count(client_sock)) {
        session = client_sessions[client_sock];
    }

    // --- Helpers ---
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

    std::string trimmed_request = trim(request);
    if (trimmed_request.empty()) return;

    std::vector<std::string> tokens = tokenize_command(trimmed_request);
    if (tokens.empty()) {
        send_msg(client_sock, build_response(
            "INVALID_COMMAND",
            session ? std::to_string((uintptr_t)session) : "",
            "error",
            "ERROR_INVALID_COMMAND",
            std::to_string(time(nullptr))
        ));
        return;
    }

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    std::string session_id = session ? std::to_string((uintptr_t)session) : "";
    std::string request_id = std::to_string(time(nullptr));

    // ----------------- User Commands -----------------
    if (cmd == "LOGIN") {
        if (tokens.size() < 3) {
            send_msg(client_sock, build_response("LOGIN", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
        }

        std::string username = tokens[1];
        std::string password = tokens[2];

        int res = um->user_login(&session, username.c_str(), password.c_str());
        if (res == 0) client_sessions[client_sock] = session;

        send_msg(client_sock, build_response("LOGIN", session_id,
                                             "result",
                                             res == 0 ? "SUCCESS_LOGIN" : "ERROR_INVALID_CREDENTIALS",
                                             request_id));
        return;
    }

    if (cmd == "LOGOUT") {
        if (session) {
            um->user_logout(session);
            session = nullptr;
            client_sessions.erase(client_sock);
        }
        send_msg(client_sock, build_response("LOGOUT", session_id, "result", "SUCCESS_LOGOUT", request_id));
        return;
    }

    // All other commands require a valid session
    if (!session) {
        send_msg(client_sock, build_response(cmd, session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
        return;
    }

    // ----- User Management -----
    if (cmd == "CREATE_USER") {
        if (tokens.size() < 4) {
            send_msg(client_sock, build_response("CREATE_USER", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
        }
        std::string username = tokens[1];
        std::string password = tokens[2];
        int role = std::stoi(tokens[3]);
        int res = um->user_create(session, username.c_str(), password.c_str(), static_cast<UserRole>(role));
        send_msg(client_sock, build_response("CREATE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "DELETE_USER") {
        int res = um->user_delete(session, tokens[1].c_str());
        send_msg(client_sock, build_response("DELETE_USER", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        return;
    }

    if (cmd == "LIST_USERS") {
        UserInfo* users = nullptr;
        int count = 0;
        int res = um->user_list(session, &users, &count);
        if (res == 0) {
            for (int i = 0; i < count; ++i)
                send_msg(client_sock, build_response("LIST_USERS", session_id, "user", users[i].username, request_id));
        } else {
            send_msg(client_sock, build_response("LIST_USERS", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }

    if (cmd == "GET_SESSION_INFO") {
        SessionInfo info;
        int res = um->get_session_info(session, &info);
        send_msg(client_sock, build_response("GET_SESSION_INFO", session_id,
                                             "user",
                                             res == 0 ? info.user.username : error_to_string(static_cast<OFSErrorCodes>(res)),
                                             request_id));
        return;
    }

    // ----------------- File Commands -----------------
    if (cmd == "CREATE") {
        if (tokens.size() < 2) {
            send_msg(client_sock, build_response("CREATE", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
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
        return;
    }

    if (cmd == "EDIT") {
        if (tokens.size() < 3) {
            send_msg(client_sock, build_response("EDIT", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
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
        return;
    }

    if (cmd == "READ") {
        if (tokens.size() < 2) {
            send_msg(client_sock, build_response("READ", session_id, "error", "ERROR_INVALID_COMMAND", request_id));
            return;
        }
        char* buffer = nullptr;
        size_t size = 0;
        int res = fm->file_read(session, tokens[1].c_str(), &buffer, &size);
        if (res == 0) {
            const size_t CHUNK = 4096;
            for (size_t i = 0; i < size; i += CHUNK) {
                size_t len = std::min(CHUNK, size - i);
                send_raw(client_sock, buffer + i, len);
            }
            send_msg(client_sock, build_response("READ", session_id, "status", "EOF_REACHED", request_id));
            meta->free_buffer(buffer);
        } else {
            send_msg(client_sock, build_response("READ", session_id, "error", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }
        return;
    }
        // ----- Directory Commands -----
        else if (cmd == "DIR_CREATE") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_CREATE", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            int res = dm->dir_create(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DIR_CREATE", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "DIR_LIST") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_LIST", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
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
            return;
                
            }
            int res = dm->dir_delete(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DELETE_DIR", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "DIR_EXISTS") {
            if (!session) {
                send_msg(client_sock, build_response("DIR_EXISTS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            int res = dm->dir_exists(session, tokens[1].c_str());
            send_msg(client_sock, build_response("DIR_EXISTS", session_id, "result", error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        // ----- Metadata Commands -----
        else if (cmd == "GET_METADATA") {
            if (!session) {
                send_msg(client_sock, build_response("GET_METADATA", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
            
            }
            FileMetadata meta_out;
            int res = meta->get_metadata(session, tokens[1].c_str(), &meta_out);
            send_msg(client_sock, build_response("GET_METADATA", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "SET_PERMISSIONS") {
            if (!session) {
                send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            uint32_t perms = std::stoul(tokens[2]);
            int res = meta->set_permissions(session, tokens[1].c_str(), perms);
            send_msg(client_sock, build_response("SET_PERMISSIONS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "GET_STATS") {
            if (!session) {
                send_msg(client_sock, build_response("GET_STATS", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            FSStats stats;
            int res = meta->get_stats(session, &stats);
            send_msg(client_sock, build_response("GET_STATS", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else if (cmd == "SET_OWNER") {
            if (!session) {
                send_msg(client_sock, build_response("SET_OWNER", session_id, "error", "ERROR_NOT_LOGGED_IN", request_id));
            return;
                
            }
            int res = meta->set_owner(session, tokens[1], tokens[2]);
            send_msg(client_sock, build_response("SET_OWNER", session_id, "result", res == 0 ? "SUCCESS" : error_to_string(static_cast<OFSErrorCodes>(res)), request_id));
        }

        else {
            send_msg(client_sock, build_response("UNKNOWN_COMMAND", session_id, "error", "ERROR_UNKNOWN_COMMAND", request_id));
        }
    

    if (session) { um->user_logout(session); session = nullptr; }
    //close(client_sock);   
}

void handle_client(int client_sock) {
    send_msg(client_sock, build_response("WELCOME", "", "message", "Welcome to OFS server!", std::to_string(time(nullptr))));
    
    while (true) {
        std::string request = recv_msg(client_sock);
        if (request.empty()) break; // client disconnected
        req_queue.push({client_sock, request}); // enqueue command
    }

    close(client_sock); // when client disconnects
}


void process_requests() {
    while (true) {
        Request req = req_queue.pop();  // FIFO
        handle_client_request(req.client_sock, req.command); // pass the command
    }
}

// --- Main server ---
int main() {
    fs_inst = nullptr;

    if (fs::exists("file.omni")) {
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
            cerr << "FS Init failed!" << endl;
            return 1;
        }
    } else {
        fs_format("file.omni", "default_config.txt");
        if (fs_init((void**)&fs_inst, "file.omni", "default.uconf.txt") != 0) {
            cerr << "FS Init failed!" << endl;
            return 1;
        }
    }

    // Initialize managers
    um = new user_manager(fs_inst->users);
    dm = new dir_manager(fs_inst->root, um);
    fm = new file_manager(fs_inst, um);
    meta = new metadata(fs_inst);

    // Setup server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket failed"); exit(EXIT_FAILURE); }

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CONN) < 0) {
        perror("listen failed"); exit(EXIT_FAILURE);
    }

    cout << "OFS Server listening on port " << PORT << endl;

    // Start worker thread to process requests FIFO
    std::thread worker(process_requests);
    worker.detach();


    // Accept loop: enqueue client sockets
    while (true) {
        int client_sock = accept(server_fd, nullptr, nullptr);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }
        req_queue.push({client_sock}); // enqueue request
    }

    fs_shutdown(fs_inst);
    return 0;
}*/

/*g++ -std=c++17 \
-Isource/include \
-Isource/include/core \
source/server/*.cpp \
source/core/*.cpp \
source/data_structures/*.cpp \
-o ofs_server \
-pthread -lssl -lcrypto


*/