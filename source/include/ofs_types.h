#ifndef OFS_TYPES_H
#define OFS_TYPES_H

#include <string>

enum OFSErrorCodes {
    OFS_SUCCESS = 0,
    OFS_ERROR_GENERIC = -1
};

struct UserInfo {
    std::string username;
    std::string password;
};

#endif
