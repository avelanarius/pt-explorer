#include <map>
#include <utility>
#include "address_resolver.hpp"

std::string remove_at_sign(const char* str) {
    std::string result = str;
    auto at_pos = result.find_last_of('@');
    if (at_pos == std::string::npos) {
        return result;
    }
    result.erase(at_pos, result.size() - at_pos);
    return result;
}

void address_resolver::load_file(const std::string &filename) {
    handle = bfd_wrapper::bfd_handle{bfd_openr(filename.c_str(),nullptr)};

    if (!handle->check_format_matches(bfd_object)) {
        throw std::runtime_error("Loaded file does not match bfd_object");
    }

    symtab = handle->get_symtab();
    dyn_symtab = handle->get_dynamic_symtab();
    auto synth_symtab = handle->get_synthetic_symtab(symtab, dyn_symtab);

    std::map<uint64_t, std::vector<std::string>> vaddr_to_funcs;
    std::vector<asymbol*> sections;

    for (auto& i : symtab) {
        if (i->flags & BSF_FUNCTION) {
            auto vaddr = i->value + i->section->vma;
            if (vaddr) {
                vaddr_to_funcs[vaddr].push_back(remove_at_sign(i->name));
            }
        } else if (i->flags & BSF_SECTION_SYM) {
            sections.push_back(i);
        }
    }

    for (auto& i : dyn_symtab) {
        if (i->flags & BSF_FUNCTION) {
            auto vaddr = i->value + i->section->vma;
            if (vaddr) {
                vaddr_to_funcs[vaddr].push_back(remove_at_sign(i->name));
            }
        }
    }

    for (auto& i : synth_symtab.first) {
        if (i.flags & BSF_FUNCTION) {
            auto vaddr = i.value + i.section->vma;
            if (vaddr) {
                vaddr_to_funcs[vaddr].push_back(remove_at_sign(i.name));
            }
        }
    }

    auto current_section = sections.begin();

    resolved_functions.clear();
    for (auto mapping = vaddr_to_funcs.begin(); mapping != vaddr_to_funcs.end(); ++mapping) {
        std::sort(mapping->second.begin(), mapping->second.end());
        mapping->second.erase(std::unique(mapping->second.begin(), mapping->second.end()), mapping->second.end());

        auto vaddr_start = mapping->first;
        while (vaddr_start > ((*current_section)->section->vma + (*current_section)->section->size)) {
            ++current_section;
            if (current_section == sections.end()) {
                throw std::runtime_error(std::string("Address: ") + std::to_string(vaddr_start) + "(decimal) out of bounds (section info)");
            }
        }

        if (vaddr_start < (*current_section)->section->vma) {
            throw std::runtime_error(std::string("Internal error while finding section for address: ") + std::to_string(vaddr_start) + " (decimal)");
        }

        auto vaddr_end = (*current_section)->section->vma + (*current_section)->section->size;

        ++mapping;
        if (mapping != vaddr_to_funcs.end()) {
            vaddr_end = std::min(vaddr_end, mapping->first);
        }
        --mapping;

        resolved_functions.emplace_back(vaddr_start, vaddr_end, mapping->second, (*current_section)->section);
    }
    last_resolved_function = resolved_functions.begin();

    find_line_at_address_cache.clear();
}

const resolved_function& address_resolver::find_function_at_address(uint64_t vaddr) {
    if (vaddr >= last_resolved_function->vaddr_start && vaddr <= last_resolved_function->vaddr_end) {
        return *last_resolved_function;
    }

    last_resolved_function = std::upper_bound(resolved_functions.begin(), resolved_functions.end(), vaddr);
    if (last_resolved_function == resolved_functions.begin()) {
        throw std::runtime_error(std::string("Could not find function at address: ") + std::to_string(vaddr));
    }
    --last_resolved_function;

    if (vaddr < last_resolved_function->vaddr_start || vaddr > last_resolved_function->vaddr_end) {
        throw std::runtime_error(std::string("Could not find function at address: ") + std::to_string(vaddr));
    }
    return *last_resolved_function;
}

resolved_line address_resolver::find_line_at_address(uint64_t vaddr) {
    auto it = find_line_at_address_cache.find(vaddr);
    if (it != find_line_at_address_cache.end()) {
        return it->second;
    }

    auto& resolved = find_function_at_address(vaddr);

    const char* file_name;
    const char* function_name;
    unsigned int line;

    auto found = bfd_find_nearest_line(handle->get(), resolved.section, symtab.data(), vaddr - resolved.section->vma,
                                       &file_name, &function_name, &line);

    if (!found) {
        throw std::runtime_error("Could not find line at address: " + std::to_string(vaddr) + " for function " + resolved.function_names.front() + " (section " + resolved.section->name + ")");
    }

    // TODO: found = bfd_find_inliner_info(handle->get(), &filename, &functionname, &line);

    bfd_find_inliner_info(handle->get(), &file_name, &function_name, &line);

    auto result = resolved_line{file_name, function_name, line};
    find_line_at_address_cache[vaddr] = result;
    return result;
}

resolved_function::resolved_function(uint64_t vaddr_start, uint64_t vaddr_end,
                                     std::vector<std::string> function_names, asection* section)
     : vaddr_start(vaddr_start), vaddr_end(vaddr_end), function_names(std::move(function_names)), section(section) {}

resolved_line::resolved_line(const char* file_name, const char* function_name, unsigned int line)
    : file_name(file_name), function_name(function_name), line(line) {}
