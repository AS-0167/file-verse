#pragma once
#include <string>
#include <cstdint>   
struct Config {
    uint64_t total_size = 104857600;
    uint32_t header_size = 512;
    uint32_t block_size = 4096;
    uint32_t max_files = 1000;
    uint32_t max_filename_length = 64;

    uint32_t max_users = 50;
    std::string admin_username = "admin";
    std::string admin_password = "admin123";
    bool require_auth = true;

    uint16_t port = 8080;
    uint32_t max_connections = 20;
    uint32_t queue_timeout = 30;
};

bool load_config(const std::string& path, Config& out, std::string* err = nullptr);

