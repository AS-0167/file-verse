#include "fs.h"
#include "file_io.h"
#include <fcntl.h>
#include <unistd.h>
#include <string>

OFSErrorCodes fs_format(const std::string& omni_path, const Config& cfg, std::string* err) {
    int fd = ::open(omni_path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0) { if (err) *err = "open failed"; return OFSErrorCodes::ERR_IO; }

    OMNIHeader hdr{};
    hdr.magic               = 0x4F4D4E49; // "OMNI"
    hdr.header_size         = cfg.header_size;
    hdr.total_size          = cfg.total_size;
    hdr.block_size          = cfg.block_size;
    hdr.max_files           = cfg.max_files;
    hdr.max_filename_length = cfg.max_filename_length;
    hdr.max_users           = cfg.max_users;

    if (!fs_zero_fill(fd, hdr.total_size)) { if (err) *err = "zero fill failed"; ::close(fd); return OFSErrorCodes::ERR_IO; }
    if (!fs_write_header(fd, hdr))          { if (err) *err = "write header failed"; ::close(fd); return OFSErrorCodes::ERR_IO; }

    auto bitmap = create_bitmap(hdr.total_size, hdr.block_size);
    if (!fs_write_bitmap(fd, hdr, bitmap))  { if (err) *err = "write bitmap failed"; ::close(fd); return OFSErrorCodes::ERR_IO; }

    ::close(fd);
    return OFSErrorCodes::SUCCESS;
}
