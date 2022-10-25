#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstring>
#include <unordered_map>

#include "bfd_wrapper.hpp"

class resolved_function {
public:
    uint64_t vaddr_start;
    uint64_t vaddr_end;
    std::vector<std::string> function_names;
    asection* section;

    friend bool operator<(const uint64_t& lhs, const resolved_function& rhs) {
        return lhs < rhs.vaddr_start;
    }

    resolved_function(uint64_t vaddr_start, uint64_t vaddr_end, std::vector<std::string> function_names, asection* section);
};

class resolved_line {
public:
    const char* file_name;
    const char* function_name;
    unsigned int line;

    bool operator==(const resolved_line& other) const {
        if (line != other.line) {
            return false;
        }

        if (function_name != other.function_name && strcmp(function_name, other.function_name) != 0) {
            return false;
        }

        if (file_name != other.file_name && strcmp(file_name, other.file_name) != 0) {
            return false;
        }

        return true;
    }

    resolved_line() noexcept : file_name(nullptr), function_name(nullptr), line(0) {}

    resolved_line(const char* file_name, const char* function_name, unsigned int line);
};

class address_resolver {
    std::optional<bfd_wrapper::bfd_handle> handle;
    std::vector<resolved_function> resolved_functions;
    std::vector<resolved_function>::iterator last_resolved_function;

    std::vector<asymbol*> symtab;
    std::vector<asymbol*> dyn_symtab;

public:
    std::unordered_map<uint64_t, resolved_line> find_line_at_address_cache;

    void load_file(const std::string& filename);

    const resolved_function& find_function_at_address(uint64_t vaddr);
    resolved_line find_line_at_address(uint64_t vaddr);

    [[nodiscard]] const std::vector<resolved_function>& get_resolved_functions() const {
        return resolved_functions;
    }
};
