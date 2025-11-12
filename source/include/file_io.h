#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "ofs_types.h"

bool write_all(int fd, const void* buf, size_t n);

bool read_all(int fd, void* buf, size_t n);

bool fs_write_header(int fd, const OMNIHeader& hdr);


bool fs_read_header(int fd, OMNIHeader& hdr);


bool fs_zero_fill(int fd, uint64_t total_size);


std::vector<uint8_t> create_bitmap(uint64_t total_bytes, uint32_t block_size);
bool fs_write_bitmap(int fd, const OMNIHeader& hdr, const std::vector<uint8_t>& map);
bool fs_read_bitmap(int fd, const OMNIHeader& hdr, std::vector<uint8_t>& map);
