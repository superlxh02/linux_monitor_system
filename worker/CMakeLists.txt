# Worker - 工作者服务器

add_subdirectory(src/ebpf)
add_subdirectory(src/kmod)

# 查找 gRPC
find_package(gRPC CONFIG QUIET)
find_library(LIBBPF_LIBRARY bpf PATHS /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/local/lib)
find_library(LIBELF_LIBRARY elf)
find_library(LIBZ_LIBRARY z)
find_program(CLANG_EXECUTABLE NAMES clang)
find_program(BPFTOOL_EXECUTABLE NAMES bpftool)

if(EXISTS "/usr/include/bpf/libbpf.h")
    set(LIBBPF_INCLUDE_DIR "/usr/include")
else()
    find_path(LIBBPF_INCLUDE_DIR bpf/libbpf.h)
endif()

if(LIBBPF_LIBRARY AND LIBELF_LIBRARY AND LIBZ_LIBRARY AND LIBBPF_INCLUDE_DIR)
    set(EBPF_FOUND TRUE)
    message(STATUS "eBPF support enabled")
else()
    set(EBPF_FOUND FALSE)
    message(WARNING "eBPF dependencies not found, disabling eBPF support")
endif()


# 源文件列表
set(WORKER_SOURCES
    worker.cpp
    src/rpc/grpc_manager_impl.cpp
    src/rpc/monitor_pusher.cpp
    src/monitor/cpuload_monitor.cpp
    src/monitor/cpusoftirq_monitor.cpp
    src/monitor/cpustate_monitor.cpp
    src/monitor/disk_monitor.cpp
    src/monitor/memory_monitor.cpp
    src/monitor/hostinfo_monitor.cpp
    src/monitor/net_ebpf_monitor.cpp
    src/monitor/net_monitor.cpp
    src/monitor/metric_collector.cpp
    src/monitor/user_monitor.cpp
    
)

# 可执行文件: worker (工作者服务器)
add_executable(worker ${WORKER_SOURCES})

# 构建 worker 时，确保先生成内核模块
add_dependencies(worker kmod_modules)

target_include_directories(worker PUBLIC
    ${PROJECT_SOURCE_DIR}/worker/include
)

# 基础链接库
set(WORKER_LIBS monitor_proto gRPC::grpc++)

# eBPF 依赖（仅在工具链与依赖齐全时启用）
set(EBPF_SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf/net_stats.bpf.c)
set(EBPF_SKEL_FROM ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf/.output/net_stats.skel.h)
set(EBPF_SKEL_TO ${CMAKE_CURRENT_SOURCE_DIR}/include/monitor/net_stats.skel.h)

if(NOT EBPF_FOUND)
    message(FATAL_ERROR "eBPF dependencies not found: require libbpf/libelf/z")
endif()

if(NOT CLANG_EXECUTABLE)
    message(FATAL_ERROR "clang not found, required for eBPF build")
endif()

if(NOT BPFTOOL_EXECUTABLE)
    message(FATAL_ERROR "bpftool not found, required for eBPF skeleton generation")
endif()

if(NOT EXISTS ${EBPF_SRC_FILE})
    message(FATAL_ERROR "Missing eBPF source file: ${EBPF_SRC_FILE}")
endif()

add_custom_command(
    OUTPUT ${EBPF_SKEL_TO}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ebpf_skeletons
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EBPF_SKEL_FROM} ${EBPF_SKEL_TO}
    DEPENDS ebpf_skeletons
    COMMENT "Generating and copying eBPF skeleton header"
)
add_custom_target(worker_ebpf_skeleton DEPENDS ${EBPF_SKEL_TO})
add_dependencies(worker worker_ebpf_skeleton)

target_include_directories(worker PRIVATE ${LIBBPF_INCLUDE_DIR})
list(APPEND WORKER_LIBS ${LIBBPF_LIBRARY} ${LIBELF_LIBRARY} ${LIBZ_LIBRARY})

target_link_libraries(worker PUBLIC ${WORKER_LIBS})

# 内核模块构建（可选）
add_custom_target(kernel_modules
    DEPENDS kmod_modules
    COMMENT "Building kernel modules"
)

# eBPF 程序构建（可选）
add_custom_target(ebpf_programs
    DEPENDS ebpf_skeletons
    COMMENT "Building eBPF programs"
)
