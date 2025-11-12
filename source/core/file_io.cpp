#include "file_io.h"
#include "ofs_types.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <vector>


bool write_all(int fd, const void* buf, size_t n) {
    const char* p = static_cast<const char*>(buf);
    size_t left = n;
    while (left) {
        ssize_t w = ::write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += w; left -= size_t(w);
    }
    return true;
}

bool read_all(int fd, void* buf, size_t n) {
    char* p = static_cast<char*>(buf);
    size_t left = n;
    while (left) {
        ssize_t r = ::read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (r == 0) return false;
        p += r; left -= size_t(r);
    }
    return true;
}

bool fs_write_header(int fd, const OMNIHeader& hdr) {
    if (::lseek(fd, 0, SEEK_SET) < 0) return false;
    return write_all(fd, &hdr, sizeof(hdr));
}

bool fs_read_header(int fd, OMNIHeader& hdr) {
    if (::lseek(fd, 0, SEEK_SET) < 0) return false;
    if (!read_all(fd, &hdr, sizeof(hdr))) return false;
    return hdr.magic == 0x4F4D4E49;
}


bool fs_zero_fill(int fd, uint64_t total_size) {
    if (total_size == 0) return false;
    if (::lseek(fd, (off_t)total_size - 1, SEEK_SET) < 0) return false;
    char z = 0;
    return write_all(fd, &z, 1);
}

std::vector<uint8_t> create_bitmap(uint64_t total_bytes, uint32_t block_size) {

    uint64_t blocks = (block_size ? total_bytes / block_size : 0);
    uint64_t bits   = blocks;
    uint64_t bytes  = (bits + 7) / 8;
    return std::vector<uint8_t>(bytes, 0); 
}

static off_t bitmap_offset(const OMNIHeader& hdr) {
return (off_t)hdr.header_size;
}

bool fs_write_bitmap(int fd, const OMNIHeader& hdr, const std::vector<uint8_t>& map) {
    if (::lseek(fd, bitmap_offset(hdr), SEEK_SET) < 0) return false;
    return write_all(fd, map.data(), map.size());
}

bool fs_read_bitmap(int fd, const OMNIHeader& hdr, std::vector<uint8_t>& map) {
    if (map.empty()) return true;
    if (::lseek(fd, bitmap_offset(hdr), SEEK_SET) < 0) return false;
    return read_all(fd, map.data(), map.size());
}
