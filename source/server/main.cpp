#include "fs.h"
#include "config.h"
#include <iostream>
#include <memory>
#include <sys/stat.h>

static bool file_exists(const std::string& p) {
    struct stat st{}; return ::stat(p.c_str(), &st) == 0;
}

int main(int argc, char** argv) {
    std::string uconf   = (argc >= 2 ? argv[1] : "../compiled/default.uconf");
    std::string omni    = (argc >= 3 ? argv[2] : "../compiled/sample.omni");

    Config cfg;
    if (!file_exists(omni)) {
        std::string err;
        auto rc = fs_format(omni, cfg, &err);
        if (rc != OFSErrorCodes::SUCCESS) {
            std::cerr << "fs_format failed: " << err << "\n";
            return 1;
        }
        std::cout << "formatted " << omni << "\n";
    }

    std::unique_ptr<OFS> ofs;
    std::string err;
    auto rc = fs_init(ofs, omni, uconf, &err);
    if (rc != OFSErrorCodes::SUCCESS) {
        std::cerr << "fs_init failed: " << err << "\n";
        return 1;
    }

    std::cout << "Server stub running. header_size=" << ofs->header.header_size
              << " block_size=" << ofs->header.block_size << "\n";
    return 0;
}
