#include <fstream>
#include "intel_pt_perf_collector.hpp"
#include <sys/syscall.h>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <iostream>

#include "lz4.h"

namespace intel_pt_wrapper {
    int read_intel_pt_type() {
        std::ifstream intel_pt_type_stream("/sys/bus/event_source/devices/intel_pt/type");
        std::string intel_pt_type_str((std::istreambuf_iterator<char>(intel_pt_type_stream)),
                                      std::istreambuf_iterator<char>());
        return std::stoi(intel_pt_type_str);
    }

    const static int INTEL_PT_TYPE = read_intel_pt_type();

    intel_pt_perf_collector::intel_pt_perf_collector(pid_t pid) {
        perf_event_attr attr{};
        attr.size = sizeof(attr);
        attr.type = INTEL_PT_TYPE;

        // Parse from /sys/devices/intel_pt/format/noretcomp etc
        // /sys/bus/event_source/devices/intel_pt/caps/mtc etc
        //
        // Check /proc/cpuinfo for constant_tsc
        //
        // https://stackoverflow.com/questions/35123379/getting-tsc-rate-from-x86-kernel
        // Measure rdtsc
        attr.config = (1U << 10) | 0/* (1U << 11)*/; // Enable TSC packets and Disable RET compression
        attr.disabled   = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;

        fd = syscall(SYS_perf_event_open, &attr, pid, -1, -1, 0);
        if (fd < 0) {
            throw std::runtime_error(std::string("Error in perf_event_open syscall: ") + strerror(errno));
        }
    }

    intel_pt_perf_collector::~intel_pt_perf_collector() {
        if (aux_buffer) {
            munmap(aux_buffer, aux_buffer_size);
        }
        if (header) {
            munmap((void *)header, header_size);
        }
        if (fd) {
            disable();
            close(fd);
        }
    }

    void intel_pt_perf_collector::set_filter(std::string &filer_string) {
        auto ret_value = ioctl(fd, PERF_EVENT_IOC_SET_FILTER, filer_string.c_str());
        if (ret_value != 0) {
            throw std::runtime_error(std::string("Error in PERF_EVENT_IOC_SET_FILTER ioctl: ") + strerror(errno));
        }
    }

    void intel_pt_perf_collector::enable() {
        auto ret_value = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        if (ret_value != 0) {
            throw std::runtime_error(std::string("Error in PERF_EVENT_IOC_ENABLE ioctl: ") + strerror(errno));
        }
    }

    void intel_pt_perf_collector::disable() {
        auto ret_value = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        if (ret_value != 0) {
            throw std::runtime_error(std::string("Error in PERF_EVENT_IOC_DISABLE ioctl: ") + strerror(errno));
        }
    }

    void intel_pt_perf_collector::setup_collection(size_t aux_size, size_t header_size_pages) {
        aux_buffer_size = aux_size;
        header_size = header_size_pages * getpagesize();

        header = (struct perf_event_mmap_page*) mmap(nullptr, header_size,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (header == MAP_FAILED) {
            throw std::runtime_error(std::string("Header mmap failed: ") + strerror(errno));
        }

        header->aux_offset = header->data_offset + header->data_size;
        header->aux_size = aux_buffer_size;

        // PROT_READ => circular buffer
        // PROT_READ | PROT_WRITE => linear buffer
        aux_buffer = mmap(nullptr, aux_buffer_size, PROT_READ, MAP_SHARED, fd, header->aux_offset);
        if (aux_buffer == MAP_FAILED) {
            throw std::runtime_error(std::string("AUX mmap failed: ") + strerror(errno));
        }
    }

    std::vector<char> intel_pt_perf_collector::get_aux_buffer() {
        std::vector<char> result;
        //result.reserve(aux_buffer_size);

        // First copy [header->aux_head, aux_buffer_size]
        // then [0, header->aux_head - 1]

        std::cout << header->aux_head << " " << header->aux_tail << std::endl;

        result.insert(result.end(), (char*) aux_buffer + header->aux_head, (char*) aux_buffer + aux_buffer_size);
        result.insert(result.end(), (char*) aux_buffer, (char*) aux_buffer + header->aux_head);

        return result;
    }

    void intel_pt_buffer_watchdog::run() {
        while (should_run) {
            auto current_raw_offset = perf_collector.header->aux_head;
            if (current_raw_offset < last_raw_offset) {
                current_wraparound += perf_collector.aux_buffer_size;
            }
            last_raw_offset = current_raw_offset;
            iters++;
            std::this_thread::sleep_for (std::chrono::milliseconds(10));
            if (iters % 1000 == 0) {
                std::cout << "Collection: " << iters / 100 << "s: " << (current_wraparound + last_raw_offset) * 1.0 / (iters / 100) / 1024 / 1024 << " MB/s (total " <<
                                      (current_wraparound + last_raw_offset) * 1.0 / 1024 / 1024 / 1024 << " GB)"<< std::endl;
            }
        }
    }

    uint64_t intel_pt_buffer_watchdog::get_current_offset() const {
        return current_wraparound + last_raw_offset;
    }

    void intel_pt_buffer_dumper::run() {
        std::vector<char> compressed;
        uint64_t total_saved = 0;

        while (should_run) {
            auto offset_start = new_offset_to_be_saved;
            auto offset_end = offset_start + save_size - 1;

            if (watchdog.get_current_offset() <= offset_end) {
                std::this_thread::sleep_for (std::chrono::milliseconds(10));
                continue;
            }

            auto offset_start_wraparound = offset_start + perf_collector.aux_buffer_size;
            if (watchdog.get_current_offset() >= offset_start_wraparound) {
                std::cerr << "INFORMATION LOSS! (Case 1)" << std::endl;
                break;
            }

            // Save it!
            //std::cout << "Saving from (1): " << offset_start << " " << offset_end << std::endl;

            offset_start %= perf_collector.aux_buffer_size;
            offset_end %= perf_collector.aux_buffer_size;

            if (offset_start >= offset_end) {
                std::cerr << "Unsupported case offset_start >= offset_end" << std::endl;
                break;
            }

            //std::cout << "Saving from (2): " << offset_start << " " << offset_end << std::endl;

            {
                const int src_size = offset_end - offset_start + 1;
                const int max_compressed_size = LZ4_compressBound(src_size);

                compressed.resize(max_compressed_size);

                const int compressed_size = LZ4_compress_fast((char*) perf_collector.aux_buffer, compressed.data(), src_size, max_compressed_size, 10);

                if (compressed_size <= 0) {
                    throw std::runtime_error("Compression error!");
                }

                compressed.resize(compressed_size);

                total_saved += compressed_size;
                std::cout << "Saving:     Saving block " << iters << " of size " << compressed_size / 1024.0 / 1024.0 << " MB (total " << total_saved / 1024.0 / 1024.0 / 1024.0 << " GB)" << std::endl;

                std::ofstream output("../blocks_new/block" + std::to_string(iters));
                output.write(compressed.data(), compressed_size);
                output.close();

            }

            iters++;

            if (watchdog.get_current_offset() >= offset_start_wraparound) {
                std::cerr << "INFORMATION LOSS! (Case 2)" << std::endl;
                break;
            }

            new_offset_to_be_saved += save_size;
        }
    }
}