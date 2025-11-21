#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <vector>

// Exact structures from odf_types.hpp
enum class UserRole : uint32_t {
    NORMAL = 0,
    ADMIN = 1
};

struct OMNIHeader {
    char magic[8];
    uint32_t format_version;
    uint64_t total_size;
    uint64_t header_size;
    uint64_t block_size;
    char student_id[32];
    char submission_date[16];
    char config_hash[64];
    uint64_t config_timestamp;
    uint32_t user_table_offset;
    uint32_t max_users;
    uint32_t file_state_storage_offset;
    uint32_t change_log_offset;
    uint8_t reserved[328];
};  // Total: 512 bytes

struct UserInfo {
    char username[32];
    char password_hash[64];
    UserRole role;
    uint64_t created_time;
    uint64_t last_login;
    uint8_t is_active;
    uint8_t reserved[23];
};  // Total: 128 bytes

void analyze_omni_file(const char* filepath) {
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        std::cerr << "Error: Cannot open file " << filepath << std::endl;
        return;
    }

    // Get actual file size
    uint64_t actual_file_size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    // Read header
    OMNIHeader header;
    ifs.read(reinterpret_cast<char*>(&header), sizeof(OMNIHeader));

    std::cout << "\n========================================\n";
    std::cout << "     OMNI FILE SIZE ANALYSIS\n";
    std::cout << "========================================\n\n";

    // Basic info
    std::cout << "File: " << filepath << "\n";
    std::cout << "Magic: " << std::string(header.magic, 8) << "\n";
    std::cout << "Version: 0x" << std::hex << header.format_version << std::dec << "\n";
    std::cout << "Header size: " << sizeof(OMNIHeader) << " bytes\n";
    std::cout << "UserInfo size: " << sizeof(UserInfo) << " bytes\n\n";

    // Size breakdown
    std::cout << "SIZE BREAKDOWN:\n";
    std::cout << "----------------------------------------\n";
    
    uint64_t header_size = sizeof(OMNIHeader);
    std::cout << std::left << std::setw(30) << "1. Header:" 
              << std::setw(12) << std::right << header_size << " bytes\n";

    uint64_t user_table_size = header.max_users * sizeof(UserInfo);
    std::cout << std::left << std::setw(30) << "2. User Table:" 
              << std::setw(12) << std::right << user_table_size << " bytes"
              << "  (" << header.max_users << " slots)\n";

    // Count active users
    ifs.seekg(header.user_table_offset, std::ios::beg);
    int active_users = 0;
    std::cout << "\n   Active Users:\n";
    for (uint32_t i = 0; i < header.max_users; ++i) {
        UserInfo u;
        ifs.read(reinterpret_cast<char*>(&u), sizeof(UserInfo));
        if (u.is_active) {
            active_users++;
            std::string role = (u.role == UserRole::ADMIN) ? "ADMIN" : "NORMAL";
            std::cout << "   - " << u.username << " (" << role << ")\n";
        }
    }
    if (active_users == 0) {
        std::cout << "   (No active users)\n";
    }
    std::cout << "\n";

    uint64_t fs_tree_start = header.user_table_offset + user_table_size;
    uint64_t bitmap_size = (header.total_size / header.block_size + 7) / 8;
    uint64_t fs_tree_size = actual_file_size - fs_tree_start - bitmap_size;

    std::cout << std::left << std::setw(30) << "3. File System Tree:" 
              << std::setw(12) << std::right << fs_tree_size << " bytes\n";

    std::cout << std::left << std::setw(30) << "4. Free Space Bitmap:" 
              << std::setw(12) << std::right << bitmap_size << " bytes\n";

    std::cout << "----------------------------------------\n";
    std::cout << std::left << std::setw(30) << "TOTAL ACTUAL FILE SIZE:" 
              << std::setw(12) << std::right << actual_file_size << " bytes\n";
    std::cout << std::left << std::setw(30) << "   In KB:" 
              << std::setw(12) << std::right << std::fixed << std::setprecision(2) 
              << (actual_file_size / 1024.0) << " KB\n";
    std::cout << std::left << std::setw(30) << "   In MB:" 
              << std::setw(12) << std::right << (actual_file_size / (1024.0 * 1024.0)) << " MB\n\n";

    // Additional info
    std::cout << "CAPACITY INFO:\n";
    std::cout << "----------------------------------------\n";
    std::cout << std::left << std::setw(30) << "Configured Total Size:" 
              << std::setw(12) << std::right << std::fixed << std::setprecision(2)
              << (header.total_size / (1024.0 * 1024.0)) << " MB\n";
    std::cout << std::left << std::setw(30) << "Block Size:" 
              << std::setw(12) << std::right << header.block_size << " bytes\n";
    std::cout << std::left << std::setw(30) << "Total Blocks:" 
              << std::setw(12) << std::right << (header.total_size / header.block_size) << "\n";
    std::cout << std::left << std::setw(30) << "Active Users:" 
              << std::setw(12) << std::right << active_users << "\n";

    // Calculate used blocks from bitmap
    ifs.seekg(-static_cast<long long>(bitmap_size), std::ios::end);
    std::vector<uint8_t> bitmap(bitmap_size);
    ifs.read(reinterpret_cast<char*>(bitmap.data()), bitmap_size);

    uint64_t used_blocks = 0;
    uint64_t total_blocks = header.total_size / header.block_size;
    for (uint64_t block = 0; block < total_blocks; ++block) {
        size_t byte_index = block / 8;
        size_t bit_index = block % 8;
        if (byte_index < bitmap.size() && (bitmap[byte_index] & (1 << bit_index))) {
            used_blocks++;
        }
    }

    uint64_t used_space = used_blocks * header.block_size;
    uint64_t free_space = header.total_size - used_space;

    std::cout << std::left << std::setw(30) << "Used Blocks:" 
              << std::setw(12) << std::right << used_blocks << "\n";
    std::cout << std::left << std::setw(30) << "Free Blocks:" 
              << std::setw(12) << std::right << (total_blocks - used_blocks) << "\n";
    std::cout << std::left << std::setw(30) << "Used Space:" 
              << std::setw(12) << std::right << std::fixed << std::setprecision(2)
              << (used_space / (1024.0 * 1024.0)) << " MB\n";
    std::cout << std::left << std::setw(30) << "Free Space:" 
              << std::setw(12) << std::right << (free_space / (1024.0 * 1024.0)) << " MB\n";
    std::cout << std::left << std::setw(30) << "Space Utilization:" 
              << std::setw(11) << std::right << std::setprecision(2) 
              << (100.0 * used_space / header.total_size) << "%\n";

    std::cout << "\n========================================\n";

    ifs.close();
}

int main(int argc, char* argv[]) {
    const char* filepath = "student_test.omni";
    
    if (argc > 1) {
        filepath = argv[1];
    }

    analyze_omni_file(filepath);
    return 0;
}

//g++ -std=c++17 -Isource/include -Isource/include/core source/core/*.cpp source/*.cpp analyze_omni.cpp -o analyze_omni -lssl -lcrypto