cmake_minimum_required(VERSION 3.21)
project(pt_explorer)

set(CMAKE_CXX_STANDARD 20)

add_executable(pt_explorer main.cpp bfd_wrapper.hpp bfd_wrapper.cpp intel_pt_perf_collector.cpp intel_pt_perf_collector.hpp proc_maps_wrapper.cpp proc_maps_wrapper.hpp address_resolver.cpp address_resolver.hpp lz4.h lz4.c)
target_link_libraries(pt_explorer -lbfd -lipt -lpthread)