#pragma once
#include <cstdint>

static constexpr uint32_t OMNI_MAGIC = 0x4F4D4E49; 


enum class OFSErrorCodes : int32_t {
    SUCCESS = 0, ERR_GENERIC = -1, ERR_IO = -2, ERR_CORRUPT = -3, ERR_CONFIG = -4,
    ERR_EXISTS = -5, ERR_NOT_FOUND = -6, ERR_PERM = -7, ERR_NO_SPACE = -8, ERR_INVALID = -9
};

enum class UserRole : uint8_t { Admin = 1, Normal = 2 };

#pragma pack(push, 1)
struct OMNIHeader {
    uint32_t magic = OMNI_MAGIC;
    uint32_t header_size = 512;
    uint64_t total_size = 0;
    uint32_t block_size = 4096;
    uint32_t max_files = 1000;
    uint32_t max_filename_length = 64;
    uint32_t max_users = 50;
    uint8_t  reserved[512 - 32]{};
};
#pragma pack(pop)

struct UserInfo {
    char username[32]{};
    char password_hash[64]{};
    UserRole role{UserRole::Normal};
    uint8_t _pad[1]{};
};
