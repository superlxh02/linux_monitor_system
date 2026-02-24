cmake_minimum_required(VERSION 3.20)

find_program(CLANG_EXECUTABLE NAMES clang)
find_program(BPFTOOL_EXECUTABLE NAMES bpftool)

if(NOT CLANG_EXECUTABLE)
  message(WARNING "clang not found, skip eBPF build in worker/src/ebpf")
  add_custom_target(ebpf_skeletons)
  return()
endif()

if(NOT BPFTOOL_EXECUTABLE)
  message(WARNING "bpftool not found, skip eBPF skeleton generation in worker/src/ebpf")
  add_custom_target(ebpf_skeletons)
  return()
endif()

set(BPF_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
if(BPF_ARCH STREQUAL "x86_64")
  set(BPF_ARCH "x86")
elseif(BPF_ARCH STREQUAL "aarch64")
  set(BPF_ARCH "arm64")
endif()

set(VMLINUX_H "/usr/include/vmlinux.h" CACHE FILEPATH "Path to vmlinux.h")
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.output")
set(SKEL_DIR "${OUTPUT_DIR}")

set(BPF_SRCS
  net_stats.bpf.c
)

set(BPF_CFLAGS
  -g
  -O2
  -target
  bpf
  -D__TARGET_ARCH_${BPF_ARCH}
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -I/usr/include
)

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

if(EXISTS "${VMLINUX_H}")
  set(VMLINUX_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/vmlinux.h")
  add_custom_command(
    OUTPUT "${VMLINUX_LOCAL}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VMLINUX_H}" "${VMLINUX_LOCAL}"
    DEPENDS "${VMLINUX_H}"
    COMMENT "Copying vmlinux.h to worker/src/ebpf"
    VERBATIM
  )
else()
  set(VMLINUX_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/vmlinux.h")
  add_custom_command(
    OUTPUT "${VMLINUX_LOCAL}"
    COMMAND /bin/sh -c "${BPFTOOL_EXECUTABLE} btf dump file /sys/kernel/btf/vmlinux format c > ${VMLINUX_LOCAL}"
    COMMENT "Generating vmlinux.h from /sys/kernel/btf/vmlinux"
    VERBATIM
  )
endif()

set(BPF_SKELS)
foreach(BPF_SRC ${BPF_SRCS})
  set(BPF_SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${BPF_SRC}")
  if(NOT EXISTS "${BPF_SRC_PATH}")
    message(WARNING "eBPF source not found: ${BPF_SRC_PATH}, skip")
    continue()
  endif()

  get_filename_component(BPF_NAME "${BPF_SRC}" NAME_WE)
  string(REPLACE ".bpf" "" BPF_NAME "${BPF_NAME}")

  set(BPF_OBJ "${OUTPUT_DIR}/${BPF_NAME}.bpf.o")
  set(BPF_SKEL "${SKEL_DIR}/${BPF_NAME}.skel.h")

  add_custom_command(
    OUTPUT "${BPF_OBJ}"
    COMMAND ${CLANG_EXECUTABLE} ${BPF_CFLAGS} -c "${BPF_SRC_PATH}" -o "${BPF_OBJ}"
    DEPENDS "${BPF_SRC_PATH}" "${VMLINUX_LOCAL}"
    COMMENT "Compiling eBPF program: ${BPF_SRC}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${BPF_SKEL}"
    COMMAND /bin/sh -c "${BPFTOOL_EXECUTABLE} gen skeleton ${BPF_OBJ} > ${BPF_SKEL}"
    DEPENDS "${BPF_OBJ}"
    COMMENT "Generating BPF skeleton: ${BPF_SKEL}"
    VERBATIM
  )

  list(APPEND BPF_SKELS "${BPF_SKEL}")
endforeach()

add_custom_target(ebpf_skeletons DEPENDS ${BPF_SKELS})

add_custom_target(ebpf_info
  COMMAND ${CMAKE_COMMAND} -E echo "CLANG: ${CLANG_EXECUTABLE}"
  COMMAND ${CMAKE_COMMAND} -E echo "BPFTOOL: ${BPFTOOL_EXECUTABLE}"
  COMMAND ${CMAKE_COMMAND} -E echo "ARCH: ${BPF_ARCH}"
  COMMAND ${CMAKE_COMMAND} -E echo "VMLINUX_H: ${VMLINUX_H}"
  COMMAND ${CMAKE_COMMAND} -E echo "BPF_SKELS: ${BPF_SKELS}"
  VERBATIM
)
