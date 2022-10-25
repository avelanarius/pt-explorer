#pragma once

#include <vector>
#include <stdexcept>
#include <memory>
#include "bfd.h"

namespace bfd_wrapper {

    class bfd_handle {
        bfd* handle = nullptr;
    public:
        explicit bfd_handle(bfd* handle) : handle(handle) {
            if (handle == nullptr) {
                throw std::runtime_error("NULL bfd_handle");
            }
        }

        bfd_handle(const bfd_handle&) = delete;
        bfd_handle& operator=(const bfd_handle&) = delete;

        bfd_handle(bfd_handle&& other) noexcept {
            std::swap(this->handle, other.handle);
        }

        bfd_handle& operator=(bfd_handle&& other) noexcept {
            std::swap(this->handle, other.handle);
            return *this;
        }

        ~bfd_handle() {
            if (handle) {
                bfd_close(handle);
                handle = nullptr;
            }
        }

        [[nodiscard]] bfd* get() noexcept {
            return handle;
        }

        [[nodiscard]] bool check_format_matches(bfd_format format) noexcept;

        [[nodiscard]] std::vector<asymbol*> get_symtab();
        [[nodiscard]] std::vector<asymbol*> get_dynamic_symtab();
        [[nodiscard]] std::pair<std::vector<asymbol>, std::unique_ptr<asymbol>> get_synthetic_symtab(std::vector<asymbol*>& symtab,
                                                                std::vector<asymbol*>& dynamic_symtab);
    };

}