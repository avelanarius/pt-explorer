#include "bfd_wrapper.hpp"
#include <cstdlib>

namespace bfd_wrapper {
    bool bfd_handle::check_format_matches(bfd_format format) noexcept {
        char** matching;

        if (bfd_check_format_matches(handle, format, &matching)) {
            return true;
        } else {
            free(matching);
            return false;
        }
    }

    std::vector<asymbol*> bfd_handle::get_symtab() {
        auto space_upper_bound = bfd_get_symtab_upper_bound(handle);
        if (space_upper_bound < 0) {
            throw std::runtime_error("While reading symtab, could not calculate space_upper_bound");
        }

        std::vector<asymbol*> symtab;
        symtab.resize(space_upper_bound / sizeof(asymbol*) + 1);

        auto symtab_actual_size = bfd_canonicalize_symtab(handle, symtab.data());
        if (symtab_actual_size < 0) {
            throw std::runtime_error("While reading symtab, could not load any entries");
        }

        symtab.resize(symtab_actual_size);
        return symtab;
    }

    std::vector<asymbol*> bfd_handle::get_dynamic_symtab() {
        auto space_upper_bound = bfd_get_dynamic_symtab_upper_bound(handle);
        if (space_upper_bound < 0) {
            throw std::runtime_error("While reading dynamic_symtab, could not calculate space_upper_bound");
        }

        std::vector<asymbol*> dynamic_symtab;
        dynamic_symtab.resize(space_upper_bound / sizeof(asymbol*) + 1);

        auto dynamic_symtab_actual_size = bfd_canonicalize_dynamic_symtab(handle, dynamic_symtab.data());
        if (dynamic_symtab_actual_size < 0) {
            throw std::runtime_error("While reading dynamic_symtab, could not load any entries");
        }

        dynamic_symtab.resize(dynamic_symtab_actual_size);
        return dynamic_symtab;
    }

    std::pair<std::vector<asymbol>, std::unique_ptr<asymbol>> bfd_handle::get_synthetic_symtab(std::vector<asymbol*>& symtab, std::vector<asymbol*>& dynamic_symtab) {
        asymbol* synthetic_symtab_temp;

        auto synthetic_symtab_count = bfd_get_synthetic_symtab(handle, symtab.size(), symtab.data(), dynamic_symtab.size(), dynamic_symtab.data(), &synthetic_symtab_temp);

        std::vector<asymbol> result(synthetic_symtab_temp, synthetic_symtab_temp + synthetic_symtab_count);
        return std::make_pair(std::move(result), std::unique_ptr<asymbol>(synthetic_symtab_temp));
    }
}