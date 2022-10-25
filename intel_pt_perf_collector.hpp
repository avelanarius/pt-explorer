#pragma once
#include <linux/perf_event.h>
#include <vector>
#include <thread>
#include <filesystem>

namespace intel_pt_wrapper {

    class intel_pt_perf_collector {
        int fd = 0;

        size_t header_size;
    public:
        volatile struct perf_event_mmap_page* header = nullptr;
        size_t aux_buffer_size;

        void* aux_buffer = nullptr;

        intel_pt_perf_collector(pid_t pid);

        ~intel_pt_perf_collector();

        void setup_collection(size_t aux_size, size_t header_size_pages);

        void set_filter(std::string& filer_string);

        void enable();

        void disable();

        std::vector<char> get_aux_buffer();

        size_t tmp() {
            return header->aux_head;
        }
    };

    class intel_pt_buffer_watchdog {
        const intel_pt_perf_collector& perf_collector;
        volatile uint64_t last_raw_offset = 0;
        volatile uint64_t current_wraparound = 0;
        volatile uint64_t iters = 0;

        std::thread watchdog_thread;
        std::atomic_bool should_run = true;

        void run();
    public:
        intel_pt_buffer_watchdog(const intel_pt_perf_collector& perf_collector) : perf_collector(perf_collector),
            watchdog_thread(&intel_pt_buffer_watchdog::run, this) {}

        ~intel_pt_buffer_watchdog() {
            should_run = false;
            watchdog_thread.join();
        }

        uint64_t get_current_offset() const;
    };

    class intel_pt_buffer_dumper {
        const intel_pt_perf_collector& perf_collector;
        const intel_pt_buffer_watchdog& watchdog;

        std::thread dumper_thread;
        std::atomic_bool should_run = true;

        volatile uint64_t new_offset_to_be_saved = 0;
        uint64_t save_size;
        volatile uint64_t iters = 0;

        void run();
    public:
        intel_pt_buffer_dumper(const intel_pt_perf_collector& perf_collector, const intel_pt_buffer_watchdog& watchdog)
            : perf_collector(perf_collector), watchdog(watchdog), dumper_thread(&intel_pt_buffer_dumper::run, this), save_size(perf_collector.aux_buffer_size / 2 / 4) {
            std::filesystem::create_directories("../blocks_new/");
        }

        ~intel_pt_buffer_dumper() {
            should_run = false;
            dumper_thread.join();
        }
    };
}