#ifndef OFS_SERVER_HPP
#define OFS_SERVER_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include "../include/odf_types.hpp"
#include "../include/ofs_core.hpp"

// ============================================================================
// SIMPLE JSON PARSER (No external dependencies)
// ============================================================================
class SimpleJSON {
public:
    static std::string getString(const std::string& json, const std::string& key) {
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        
        pos = json.find(":", pos);
        if (pos == std::string::npos) return "";
        
        pos = json.find("\"", pos);
        if (pos == std::string::npos) return "";
        
        size_t end = json.find("\"", pos + 1);
        if (end == std::string::npos) return "";
        
        return json.substr(pos + 1, end - pos - 1);
    }
    
    static int getInt(const std::string& json, const std::string& key) {
        std::string val = getString(json, key);
        if (!val.empty()) return std::atoi(val.c_str());
        
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        
        pos = json.find(":", pos);
        if (pos == std::string::npos) return 0;
        
        pos = json.find_first_of("-0123456789", pos);
        if (pos == std::string::npos) return 0;
        
        return std::atoi(json.c_str() + pos);
    }
    
    static std::string buildSuccess(const std::string& operation, const std::string& request_id, 
                                     const std::string& data = "") {
        std::ostringstream oss;
        oss << "{\"status\":\"success\",\"operation\":\"" << operation << "\"";
        oss << ",\"request_id\":\"" << request_id << "\"";
        if (!data.empty()) {
            oss << ",\"data\":{" << data << "}";
        }
        oss << "}";
        return oss.str();
    }
    
    static std::string buildError(const std::string& operation, const std::string& request_id, 
                                   int error_code, const std::string& error_msg) {
        std::ostringstream oss;
        oss << "{\"status\":\"error\",\"operation\":\"" << operation << "\"";
        oss << ",\"request_id\":\"" << request_id << "\"";
        oss << ",\"error_code\":" << error_code;
        oss << ",\"error_message\":\"" << error_msg << "\"}";
        return oss.str();
    }
    
    static std::string escapeJSON(const std::string& str) {
        std::ostringstream oss;
        for (char c : str) {
            switch(c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default: 
                    if (c >= 32 && c <= 126) {
                        oss << c;
                    }
                    break;
            }
        }
        return oss.str();
    }
};

// ============================================================================
// CUSTOM QUEUE (FIFO)
// ============================================================================
template<typename T>
class Queue {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& val) : data(val), next(nullptr) {}
    };
    
    Node* front;
    Node* rear;
    int count;
    
public:
    Queue() : front(nullptr), rear(nullptr), count(0) {}
    
    ~Queue() {
        while (!isEmpty()) {
            dequeue();
        }
    }
    
    void enqueue(const T& val) {
        Node* newNode = new Node(val);
        if (rear == nullptr) {
            front = rear = newNode;
        } else {
            rear->next = newNode;
            rear = newNode;
        }
        count++;
    }
    
    T dequeue() {
        if (isEmpty()) {
            throw std::runtime_error("Queue is empty");
        }
        Node* temp = front;
        T data = front->data;
        front = front->next;
        if (front == nullptr) {
            rear = nullptr;
        }
        delete temp;
        count--;
        return data;
    }
    
    bool isEmpty() const { return front == nullptr; }
    int size() const { return count; }
};

// ============================================================================
// SESSION MAP (Custom HashMap Implementation)
// ============================================================================
class SessionMap {
private:
    struct Node {
        std::string key;
        void* value;
        Node* next;
        Node(const std::string& k, void* v) : key(k), value(v), next(nullptr) {}
    };
    
    static const int TABLE_SIZE = 101;
    Node* table[TABLE_SIZE];
    
    int hash(const std::string& key) const {
        int h = 0;
        for (char c : key) {
            h = (h * 31 + c) % TABLE_SIZE;
        }
        return h;
    }
    
public:
    SessionMap() {
        for (int i = 0; i < TABLE_SIZE; i++) {
            table[i] = nullptr;
        }
    }
    
    ~SessionMap() {
        clear();
    }
    
    void insert(const std::string& key, void* value) {
        int idx = hash(key);
        Node* current = table[idx];
        while (current) {
            if (current->key == key) {
                current->value = value;
                return;
            }
            current = current->next;
        }
        Node* newNode = new Node(key, value);
        newNode->next = table[idx];
        table[idx] = newNode;
    }
    
    void* find(const std::string& key) const {
        int idx = hash(key);
        Node* current = table[idx];
        while (current) {
            if (current->key == key) {
                return current->value;
            }
            current = current->next;
        }
        return nullptr;
    }
    
    void remove(const std::string& key) {
        int idx = hash(key);
        Node* current = table[idx];
        Node* prev = nullptr;
        while (current) {
            if (current->key == key) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    table[idx] = current->next;
                }
                delete current;
                return;
            }
            prev = current;
            current = current->next;
        }
    }
    
    void clear() {
        for (int i = 0; i < TABLE_SIZE; i++) {
            Node* current = table[i];
            while (current) {
                Node* next = current->next;
                delete current;
                current = next;
            }
            table[i] = nullptr;
        }
    }
};

// ============================================================================
// REQUEST STRUCTURE
// ============================================================================
struct Request {
    int client_fd;
    std::string operation;
    std::string session_id;
    std::string request_id;
    std::string json_data;
    
    Request() : client_fd(-1) {}
    Request(int fd, const std::string& op, const std::string& sid, 
            const std::string& rid, const std::string& data)
        : client_fd(fd), operation(op), session_id(sid), 
          request_id(rid), json_data(data) {}
};

// ============================================================================
// OFS SERVER
// ============================================================================
class OFSServer {
private:
    int server_fd;
    int port;
    bool running;
    OFSCore* ofs_core;
    Queue<Request> request_queue;
    SessionMap session_map;
    
    int max_connections;
    int queue_timeout;
    int session_counter;
    
    std::string generateSessionID(const std::string& username) {
        std::ostringstream oss;
        oss << "SESSION_" << username << "_" << time(nullptr) << "_" << (++session_counter);
        return oss.str();
    }
    
    // ========================================================================
    // HANDLER FUNCTIONS - Connected to OFSCore (camelCase)
    // ========================================================================
    
    std::string handleUserLogin(const Request& req) {
        std::string username = SimpleJSON::getString(req.json_data, "username");
        std::string password = SimpleJSON::getString(req.json_data, "password");
        
        std::cout << "[LOGIN] User: " << username << std::endl;
        
        void* session = nullptr;
        int result = ofs_core->userLogin(&session, username.c_str(), password.c_str());
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            std::string session_id = generateSessionID(username);
            session_map.insert(session_id, session);
            
            std::ostringstream data;
            data << "\"session_id\":\"" << session_id << "\"";
            data << ",\"username\":\"" << username << "\"";
            
            return SimpleJSON::buildSuccess(req.operation, req.request_id, data.str());
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result, 
                ofs_core->getErrorMessage(result));
        }
    }
    
    std::string handleUserLogout(const Request& req) {
        void* session = session_map.find(req.session_id);
        if (!session) {
            return SimpleJSON::buildError(req.operation, req.request_id,
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION), "Invalid session");
        }
        
        int result = ofs_core->userLogout(session);
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            session_map.remove(req.session_id);
            return SimpleJSON::buildSuccess(req.operation, req.request_id);
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result,
                ofs_core->getErrorMessage(result));
        }
    }
    
    std::string handleFileCreate(const Request& req) {
        void* session = session_map.find(req.session_id);
        if (!session) {
            return SimpleJSON::buildError(req.operation, req.request_id,
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION), "Invalid session");
        }
        
        std::string path = SimpleJSON::getString(req.json_data, "path");
        std::string content = SimpleJSON::getString(req.json_data, "data");
        
        int result = ofs_core->fileCreate(session, path.c_str(), content.c_str(), content.size());
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            return SimpleJSON::buildSuccess(req.operation, req.request_id);
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result,
                ofs_core->getErrorMessage(result));
        }
    }
    
    std::string handleFileRead(const Request& req) {
        void* session = session_map.find(req.session_id);
        if (!session) {
            return SimpleJSON::buildError(req.operation, req.request_id,
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION), "Invalid session");
        }
        
        std::string path = SimpleJSON::getString(req.json_data, "path");
        
        char* buffer = nullptr;
        size_t size = 0;
        int result = ofs_core->fileRead(session, path.c_str(), &buffer, &size);
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            std::string content(buffer, size);
            std::string escaped = SimpleJSON::escapeJSON(content);
            
            std::ostringstream data;
            data << "\"content\":\"" << escaped << "\"";
            data << ",\"size\":" << size;
            
            ofs_core->freeBuffer(buffer);
            return SimpleJSON::buildSuccess(req.operation, req.request_id, data.str());
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result,
                ofs_core->getErrorMessage(result));
        }
    }
    
    std::string handleFileDelete(const Request& req) {
        void* session = session_map.find(req.session_id);
        if (!session) {
            return SimpleJSON::buildError(req.operation, req.request_id,
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION), "Invalid session");
        }
        
        std::string path = SimpleJSON::getString(req.json_data, "path");
        int result = ofs_core->fileDelete(session, path.c_str());
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            return SimpleJSON::buildSuccess(req.operation, req.request_id);
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result,
                ofs_core->getErrorMessage(result));
        }
    }
    
    std::string handleFileEdit(const Request& req) {
        void* session = session_map.find(req.session_id);
        if (!session) {
            return SimpleJSON::buildError(req.operation, req.request_id,
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION), "Invalid session");
        }
        
        std::string path = SimpleJSON::getString(req.json_data, "path");
        std::string data = SimpleJSON::getString(req.json_data, "data");
        int index = SimpleJSON::getInt(req.json_data, "index");
        
        int result = ofs_core->fileEdit(session, path.c_str(), data.c_str(), data.size(), index);
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            return SimpleJSON::buildSuccess(req.operation, req.request_id);
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result,
                ofs_core->getErrorMessage(result));
        }
    }
    
    std::string handleDirCreate(const Request& req) {
        void* session = session_map.find(req.session_id);
        if (!session) {
            return SimpleJSON::buildError(req.operation, req.request_id,
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION), "Invalid session");
        }
        
        std::string path = SimpleJSON::getString(req.json_data, "path");
        int result = ofs_core->dirCreate(session, path.c_str());
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            return SimpleJSON::buildSuccess(req.operation, req.request_id);
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result,
                ofs_core->getErrorMessage(result));
        }
    }
    
    std::string handleDirList(const Request& req) {
        void* session = session_map.find(req.session_id);
        if (!session) {
            return SimpleJSON::buildError(req.operation, req.request_id,
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION), "Invalid session");
        }
        
        std::string path = SimpleJSON::getString(req.json_data, "path");
        
        FileEntry* entries = nullptr;
        int count = 0;
        int result = ofs_core->dirList(session, path.c_str(), &entries, &count);
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            std::ostringstream data;
            data << "\"files\":[";
            
            for (int i = 0; i < count; i++) {
                if (i > 0) data << ",";
                data << "{";
                
                // Safely extract name (null-terminated string)
                std::string name;
                for (int j = 0; j < 255 && entries[i].name[j] != '\0'; j++) {
                    if (entries[i].name[j] >= 32 && entries[i].name[j] <= 126) {
                        name += entries[i].name[j];
                    }
                }
                
                // Safely extract owner
                std::string owner;
                for (int j = 0; j < 31 && entries[i].owner[j] != '\0'; j++) {
                    if (entries[i].owner[j] >= 32 && entries[i].owner[j] <= 126) {
                        owner += entries[i].owner[j];
                    }
                }
                
                data << "\"name\":\"" << SimpleJSON::escapeJSON(name) << "\",";
                data << "\"type\":" << (int)entries[i].type << ",";
                data << "\"size\":" << entries[i].size << ",";
                data << "\"permissions\":" << entries[i].permissions << ",";
                data << "\"owner\":\"" << SimpleJSON::escapeJSON(owner) << "\"";
                data << "}";
            }
            
            data << "]";
            
            ofs_core->freeBuffer(entries);
            return SimpleJSON::buildSuccess(req.operation, req.request_id, data.str());
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result,
                ofs_core->getErrorMessage(result));
        }
    }
    
    std::string handleGetStats(const Request& req) {
        void* session = session_map.find(req.session_id);
        if (!session) {
            return SimpleJSON::buildError(req.operation, req.request_id,
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION), "Invalid session");
        }
        
        FSStats stats;
        int result = ofs_core->getStats(session, &stats);
        
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            std::ostringstream data;
            data << "\"total_size\":" << stats.total_size << ",";
            data << "\"used_space\":" << stats.used_space << ",";
            data << "\"free_space\":" << stats.free_space << ",";
            data << "\"total_files\":" << stats.total_files << ",";
            data << "\"total_directories\":" << stats.total_directories;
            
            return SimpleJSON::buildSuccess(req.operation, req.request_id, data.str());
        } else {
            return SimpleJSON::buildError(req.operation, req.request_id, result,
                ofs_core->getErrorMessage(result));
        }
    }
    
    void processRequest(const Request& req) {
        std::string response;
        
        try {
            if (req.operation == "user_login") {
                response = handleUserLogin(req);
            } else if (req.operation == "user_logout") {
                response = handleUserLogout(req);
            } else if (req.operation == "file_create") {
                response = handleFileCreate(req);
            } else if (req.operation == "file_read") {
                response = handleFileRead(req);
            } else if (req.operation == "file_delete") {
                response = handleFileDelete(req);
            } else if (req.operation == "file_edit") {
                response = handleFileEdit(req);
            } else if (req.operation == "dir_create") {
                response = handleDirCreate(req);
            } else if (req.operation == "dir_list") {
                response = handleDirList(req);
            } else if (req.operation == "get_stats") {
                response = handleGetStats(req);
            } else {
                response = SimpleJSON::buildError(req.operation, req.request_id, 
                    static_cast<int>(OFSErrorCodes::ERROR_NOT_IMPLEMENTED), 
                    "Operation not implemented");
            }
        } catch (const std::exception& e) {
            response = SimpleJSON::buildError(req.operation, req.request_id, 
                static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION), 
                std::string("Exception: ") + e.what());
        }
        
        sendResponse(req.client_fd, response);
    }
    
    void sendResponse(int client_fd, const std::string& response) {
        std::string msg = response + "\n";
        send(client_fd, msg.c_str(), msg.length(), 0);
    }
    
    std::string receiveMessage(int client_fd) {
        char buffer[8192];
        std::string message;
        
        int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            message = buffer;
        }
        
        return message;
    }
    
public:
    OFSServer(int p = 8080) 
        : server_fd(-1), port(p), running(false), ofs_core(nullptr),
          max_connections(20), queue_timeout(30), session_counter(0) {}
    
    ~OFSServer() {
        stop();
    }
    
    bool initialize(OFSCore* core) {
        ofs_core = core;
        
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "Failed to create socket\n";
            return false;
        }
        
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set socket options\n";
            return false;
        }
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Failed to bind socket to port " << port << "\n";
            return false;
        }
        
        if (listen(server_fd, max_connections) < 0) {
            std::cerr << "Failed to listen on socket\n";
            return false;
        }
        
        std::cout << "Server initialized on port " << port << "\n";
        return true;
    }
    
    void run() {
        running = true;
        std::cout << "Server running. Waiting for connections...\n";
        
        while (running) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                if (running) {
                    std::cerr << "Failed to accept connection\n";
                }
                continue;
            }
            
            std::cout << "Client connected: " << inet_ntoa(client_addr.sin_addr) << "\n";
            
            std::string message = receiveMessage(client_fd);
            if (!message.empty()) {
                std::string operation = SimpleJSON::getString(message, "operation");
                std::string session_id = SimpleJSON::getString(message, "session_id");
                std::string request_id = SimpleJSON::getString(message, "request_id");
                
                Request req(client_fd, operation, session_id, request_id, message);
                
                request_queue.enqueue(req);
                std::cout << "Request queued: " << operation << " (queue size: " 
                          << request_queue.size() << ")\n";
                
                while (!request_queue.isEmpty()) {
                    Request current = request_queue.dequeue();
                    std::cout << "Processing: " << current.operation << "\n";
                    processRequest(current);
                }
            }
            
            close(client_fd);
        }
    }
    
    void stop() {
        running = false;
        if (server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
        std::cout << "Server stopped\n";
    }
};

#endif // OFS_SERVER_HPP