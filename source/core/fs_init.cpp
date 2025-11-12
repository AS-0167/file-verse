#include "fs.h"
#include "file_io.h"
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <iostream>

OFSErrorCodes fs_init(std::unique_ptr<OFS>& out,
                      const std::string& omni_path,
                      const std::string& config_path,
                      std::string* err) {
    (void)config_path; // you can parse later in config.cpp

    int fd = ::open(omni_path.c_str(), O_RDWR);
    if (fd < 0) { if (err) *err = "open omni failed"; return OFSErrorCodes::ERR_IO; }

    OMNIHeader hdr{};
    if (!fs_read_header(fd, hdr)) { if (err) *err = "read header failed"; ::close(fd); return OFSErrorCodes::ERR_IO; }

    auto map = create_bitmap(hdr.total_size, hdr.block_size);
    if (!fs_read_bitmap(fd, hdr, map)) { if (err) *err = "read bitmap failed"; ::close(fd); return OFSErrorCodes::ERR_IO; }

    // allocate OFS (minimal attach)
    auto ofs = std::make_unique<OFS>();
    ofs->fd      = fd;
    ofs->header  = hdr;
    ofs->free_space.reset((int)(hdr.total_size / hdr.block_size)); // init local Bitmap too

    out = std::move(ofs);
    std::cout << "fs_init OK: blocks=" << (hdr.total_size / hdr.block_size) << "\n";
    return OFSErrorCodes::SUCCESS;
}
