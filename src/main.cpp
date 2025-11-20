#include <iostream>
#include <fstream>
#include <thread>

// IMPORTANT: Include the new library
// #define CPPHTTPLIB_OPENSSL_SUPPORT // We have disabled this for now
#include "../include/httplib.h"

#include "../include/ofs_types.h"
#include "../include/filesystem.h"
#include "../include/json.hpp"

using json = nlohmann::json;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
FileSystemInstance g_fs_instance;

// ============================================================================
// MAIN FUNCTION
// ============================================================================
int main() {
    // --- 1. Initialize the File System ---
    const std::string OMNI_PATH = "my_fs.omni";
    std::ifstream f(OMNI_PATH);
    if (!f.good()) {
        fs_format(OMNI_PATH);
    }
    fs_init(g_fs_instance, OMNI_PATH);

    // --- 2. Create and Configure the HTTP Server ---
    httplib::Server svr;
// --- ADD THIS BLOCK TO SERVE THE HTML AND JS FILES ---
    // This tells the server to look for files in the current directory
    // where you run the executable from (i.e., your project's root)
    svr.set_mount_point("/", "."); 

    svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
        std::ifstream file("index.html");
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else {
            res.status = 404;
            res.set_content("Not Found", "text/plain");
        }
    });

    svr.Get("/script.js", [](const httplib::Request &, httplib::Response &res) {
        std::ifstream file("script.js");
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "application/javascript");
        } else {
            res.status = 404;
            res.set_content("Not Found", "text/plain");
        }
    });
    // --- END OF NEW BLOCK ---
    // --- 3. Define All API Routes ---

    // ======================================================
    //  PUBLIC ROUTES (No login required)
    // ======================================================

    svr.Post("/user_login", [](const httplib::Request& req, httplib::Response& res) {
        json response_json;
        try {
            json params = json::parse(req.body)["parameters"];
            std::string session_id = user_login(g_fs_instance, params["username"], params["password"]);
            if (!session_id.empty()) {
                response_json = {{"status", "success"}, {"data", {{"session_id", session_id}}}};
            } else {
                response_json = {{"status", "error"}, {"error_message", "Invalid credentials."}};
            }
        } catch (const std::exception& e) {
            response_json = {{"status", "error"}, {"error_message", "Invalid JSON: " + std::string(e.what())}};
            res.status = 400;
        }
        res.set_content(response_json.dump(), "application/json");
    });

    // NEWLY ADDED
    svr.Post("/get_error_message", [&](const httplib::Request& req, httplib::Response& res) {
        json response_json;
        try {
            json p = json::parse(req.body)["parameters"];
            std::string msg = get_error_message(p["error_code"]);
            response_json = {{"status", "success"}, {"data", {{"error_message", msg}}}};
        } catch (const std::exception& e) {
            response_json = {{"status", "error"}, {"error_message", "Invalid JSON: " + std::string(e.what())}};
            res.status = 400;
        }
        res.set_content(response_json.dump(), "application/json");
    });

    // ======================================================
    //  AUTHENTICATED ROUTES (Login required)
    // ======================================================

    auto is_logged_in = [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_header("X-Session-ID") || g_fs_instance.active_sessions.count(req.get_header_value("X-Session-ID")) == 0) {
            res.status = 401;
            res.set_content(json{{"status", "error"}, {"error_message", "Authentication required."}}.dump(), "application/json");
            return false;
        }
        return true;
    };

    svr.Post("/user_logout", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return;
        user_logout(g_fs_instance, req.get_header_value("X-Session-ID"));
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });
    
    svr.Get("/get_session_info", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return;
        SessionInfo info = get_session_info(g_fs_instance, req.get_header_value("X-Session-ID"));
        res.set_content(json{{"status", "success"}, {"data", {{"username", info.username}, {"role", info.role}}}}.dump(), "application/json");
    });

    svr.Post("/dir_list", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        std::vector<DirEntryInfo> entries = dir_list(g_fs_instance, p["path"]);
        json data_array = json::array();
        for (const auto& entry : entries) {
           data_array.push_back({{"name", entry.name}, {"is_directory", entry.is_directory}});
        }
        res.set_content(json{{"status", "success"}, {"data", data_array}}.dump(), "application/json");
    });

    svr.Post("/file_read", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        std::string content = file_read(g_fs_instance, p["path"]);
        res.set_content(json{{"status", "success"}, {"data", {{"content", content}}}}.dump(), "application/json");
    });

    // NEWLY ADDED
    svr.Post("/file_exists", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        bool exists = file_exists(g_fs_instance, p["path"]);
        res.set_content(json{{"status", "success"}, {"data", {{"exists", exists}}}}.dump(), "application/json");
    });

    // NEWLY ADDED
    svr.Post("/dir_exists", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        bool exists = dir_exists(g_fs_instance, p["path"]);
        res.set_content(json{{"status", "success"}, {"data", {{"exists", exists}}}}.dump(), "application/json");
    });

    // NEWLY ADDED
    svr.Post("/get_metadata", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        FileMetadata meta = get_metadata(g_fs_instance, p["path"]);
        res.set_content(json{{"status", "success"}, {"data", {
            {"name", meta.name}, {"is_directory", meta.is_directory}, {"size", meta.size},
            {"owner_id", meta.owner_id}, {"permissions", meta.permissions},
            {"created_time", meta.created_time}, {"modified_time", meta.modified_time}
        }}}.dump(), "application/json");
    });

    svr.Get("/get_stats", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return;
        FSStats stats = get_stats(g_fs_instance);
        res.set_content(json{{"status", "success"}, {"data", {
            {"total_size", stats.total_size}, {"used_space", stats.used_space}, {"free_space", stats.free_space},
            {"file_count", stats.file_count}, {"directory_count", stats.directory_count}
        }}}.dump(), "application/json");
    });

    // ======================================================
    //  ADMIN-ONLY ROUTES (Admin login required)
    // ======================================================
    
    auto is_admin = [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_logged_in(req, res)) return false;
        auto it = g_fs_instance.active_sessions.find(req.get_header_value("X-Session-ID"));
        if (it->second->role != 1) { // 1 is the admin role
            res.status = 403; // Forbidden
            res.set_content(json{{"status", "error"}, {"error_message", "Permission denied: Admin access required."}}.dump(), "application/json");
            return false;
        }
        return true;
    };

    svr.Get("/user_list", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        std::vector<std::string> users = user_list(g_fs_instance);
        res.set_content(json{{"status", "success"}, {"data", {{"users", users}}}}.dump(), "application/json");
    });

    svr.Post("/user_create", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        user_create(g_fs_instance, p["username"], p["password"], p["role"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    svr.Post("/user_delete", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        user_delete(g_fs_instance, p["username"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });
    
    svr.Post("/dir_create", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        dir_create(g_fs_instance, p["path"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    svr.Post("/dir_delete", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        dir_delete(g_fs_instance, p["path"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    svr.Post("/file_create", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        file_create(g_fs_instance, p["path"], p["data"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    svr.Post("/file_delete", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        file_delete(g_fs_instance, p["path"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });
    
    svr.Post("/file_rename", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        file_rename(g_fs_instance, p["old_path"], p["new_path"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    svr.Post("/file_edit", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        file_edit(g_fs_instance, p["path"], p["data"], p["index"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    svr.Post("/file_truncate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        file_truncate(g_fs_instance, p["path"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    // NEWLY ADDED
    svr.Post("/set_permissions", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        json p = json::parse(req.body)["parameters"];
        set_permissions(g_fs_instance, p["path"], p["permissions"]);
        res.set_content(json{{"status", "success"}}.dump(), "application/json");
    });

    svr.Post("/shutdown", [&](const httplib::Request& req, httplib::Response& res) {
        if (!is_admin(req, res)) return;
        res.set_content(json{{"status", "success"}, {"data", {{"message", "Server is shutting down."}}}}.dump(), "application/json");
        svr.stop();
    });


    // --- 4. Start the Server ---
    std::cout << "HTTP server is starting on http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    // After svr.listen() returns (on svr.stop()), we can call shutdown
    fs_shutdown();

    return 0;
}