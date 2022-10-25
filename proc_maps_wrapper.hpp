#pragma once

#include <stdint.h>
#include <string>
#include <vector>

struct maps_entry {
    uint64_t address_start;
    uint64_t address_end;

    uint64_t file_address_start;
    uint64_t file_address_end;

    bool read_permissions;
    bool write_permissions;
    bool execute_permissions;
    bool is_private;
    std::string pathname;

};

std::vector<maps_entry> read_maps_entry(pid_t pid);

std::string read_pid_executable_name(pid_t pid);
