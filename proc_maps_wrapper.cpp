#include "proc_maps_wrapper.hpp"
#include <fstream>
#include <climits>
#include <unistd.h>

std::vector<maps_entry> read_maps_entry(pid_t pid) {
    std::vector<maps_entry> result;

    std::ifstream maps_stream("/proc/" + std::to_string(pid) + "/maps");
    if (!maps_stream.is_open()) {
        throw std::runtime_error("Could not open /proc/PID/maps");
    }
    while (!maps_stream.eof()) {
        uint64_t address_start, address_end;

        maps_stream >> std::hex >> address_start;
        maps_stream.get();
        maps_stream >> std::hex >> address_end;

        std::string perms;
        maps_stream >> perms;

        uint64_t offset;
        maps_stream >> std::hex >> offset;

        uint64_t dev_major, dev_minor;
        maps_stream >> std::hex >> dev_major;
        maps_stream.get();
        maps_stream >> std::hex >> dev_minor;

        uint64_t inode;
        maps_stream >> inode;

        std::string pathname;
        std::getline(maps_stream, pathname);

        pathname.erase(0, pathname.find_first_not_of(' '));

        maps_entry entry;
        entry.address_start = address_start;
        entry.address_end = address_end;
        entry.file_address_start = offset;
        entry.pathname = pathname;
        entry.read_permissions = perms[0] == 'r';
        entry.write_permissions = perms[1] == 'w';
        entry.execute_permissions = perms[2] == 'x';
        entry.is_private = perms[3] == 'p';
        entry.file_address_end = entry.file_address_start + entry.address_end - entry.address_start - 1;

        result.push_back(entry);
    }
    maps_stream.close();

    return result;
}

std::string read_pid_executable_name(pid_t pid) {
    char buf[PATH_MAX + 1];
    auto path = "/proc/" + std::to_string(pid) + "/exe";
    auto len = ::readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return std::string(buf);
    } else {
        throw std::runtime_error("Could not read executable name for pid");
    }
}
