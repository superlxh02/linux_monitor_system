cmake_minimum_required(VERSION 3.20)

option(KMOD_STRICT "Fail when kernel headers are missing" ON)

execute_process(
  COMMAND uname -r
  OUTPUT_VARIABLE KERNEL_RELEASE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(KDIR "/lib/modules/${KERNEL_RELEASE}/build" CACHE PATH "Kernel build directory")
set(KMOD_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(KMOD_ARTIFACT_DIR "${CMAKE_CURRENT_BINARY_DIR}/kmod_artifacts")

if(NOT EXISTS "${KDIR}")
  message(WARNING "Kernel build directory not found: ${KDIR}")
endif()

if(EXISTS "${KDIR}")
  add_custom_target(kmod_modules
    COMMAND ${CMAKE_COMMAND} -E make_directory "${KMOD_ARTIFACT_DIR}"
    COMMAND make -C "${KDIR}" M="${KMOD_DIR}" MO="${KMOD_DIR}" modules
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KMOD_DIR}/softirq_collector.ko" "${KMOD_ARTIFACT_DIR}/softirq_collector.ko"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KMOD_DIR}/cpu_stat_collector.ko" "${KMOD_ARTIFACT_DIR}/cpu_stat_collector.ko"
    COMMAND make -C "${KDIR}" M="${KMOD_DIR}" MO="${KMOD_DIR}" clean
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KMOD_ARTIFACT_DIR}/softirq_collector.ko" "${KMOD_DIR}/softirq_collector.ko"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KMOD_ARTIFACT_DIR}/cpu_stat_collector.ko" "${KMOD_DIR}/cpu_stat_collector.ko"
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Building kernel modules (softirq_collector.ko, cpu_stat_collector.ko)"
  )

  add_custom_target(kmod_clean
    COMMAND make -C "${KDIR}" M="${KMOD_DIR}" MO="${KMOD_DIR}" clean
    COMMAND ${CMAKE_COMMAND} -E rm -f "${KMOD_ARTIFACT_DIR}/softirq_collector.ko"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${KMOD_ARTIFACT_DIR}/cpu_stat_collector.ko"
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Cleaning kernel module build artifacts"
  )

  add_custom_target(kmod_install_softirq
    COMMAND sudo insmod "${KMOD_DIR}/softirq_collector.ko"
    COMMAND ${CMAKE_COMMAND} -E echo "Module loaded. Device: /dev/cpu_softirq_monitor"
    DEPENDS kmod_modules
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Installing softirq_collector kernel module"
  )

  add_custom_target(kmod_install_cpustat
    COMMAND sudo insmod "${KMOD_DIR}/cpu_stat_collector.ko"
    COMMAND ${CMAKE_COMMAND} -E echo "Module loaded. Device: /dev/cpu_stat_monitor"
    DEPENDS kmod_modules
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Installing cpu_stat_collector kernel module"
  )

  add_custom_target(kmod_install
    DEPENDS kmod_install_softirq kmod_install_cpustat
  )

  add_custom_target(kmod_uninstall_softirq
    COMMAND sudo rmmod softirq_collector || true
    COMMAND ${CMAKE_COMMAND} -E echo "softirq_collector module unloaded."
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Uninstalling softirq_collector kernel module"
  )

  add_custom_target(kmod_uninstall_cpustat
    COMMAND sudo rmmod cpu_stat_collector || true
    COMMAND ${CMAKE_COMMAND} -E echo "cpu_stat_collector module unloaded."
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Uninstalling cpu_stat_collector kernel module"
  )

  add_custom_target(kmod_uninstall
    DEPENDS kmod_uninstall_softirq kmod_uninstall_cpustat
  )

  add_custom_target(kmod_info
    COMMAND modinfo "${KMOD_DIR}/softirq_collector.ko"
    COMMAND modinfo "${KMOD_DIR}/cpu_stat_collector.ko"
    DEPENDS kmod_modules
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Showing kernel module information"
  )
else()
  if(KMOD_STRICT)
    add_custom_target(kmod_modules
      COMMAND ${CMAKE_COMMAND} -E echo "[ERROR] Kernel build directory not found: ${KDIR}"
      COMMAND ${CMAKE_COMMAND} -E echo "[HINT] Install matching kernel headers for: ${KERNEL_RELEASE}"
      COMMAND ${CMAKE_COMMAND} -E echo "[HINT] Expected path: /lib/modules/${KERNEL_RELEASE}/build"
      COMMAND ${CMAKE_COMMAND} -E false
      COMMENT "Failing kmod build because kernel headers are missing"
    )
  else()
    add_custom_target(kmod_modules
      COMMAND ${CMAKE_COMMAND} -E echo "[WARN] Skip kmod build: kernel build directory not found: ${KDIR}"
      COMMENT "Skipping kernel module build"
    )
  endif()

  add_custom_target(kmod_clean
    COMMAND ${CMAKE_COMMAND} -E echo "[WARN] Skip kmod clean: kernel build directory not found: ${KDIR}"
    COMMENT "Skipping kernel module clean"
  )

  add_custom_target(kmod_install_softirq
    COMMAND ${CMAKE_COMMAND} -E echo "[WARN] Skip kmod install softirq: kernel build directory not found: ${KDIR}"
    DEPENDS kmod_modules
    COMMENT "Skipping softirq_collector install"
  )

  add_custom_target(kmod_install_cpustat
    COMMAND ${CMAKE_COMMAND} -E echo "[WARN] Skip kmod install cpustat: kernel build directory not found: ${KDIR}"
    DEPENDS kmod_modules
    COMMENT "Skipping cpu_stat_collector install"
  )

  add_custom_target(kmod_install
    DEPENDS kmod_install_softirq kmod_install_cpustat
  )

  add_custom_target(kmod_uninstall_softirq
    COMMAND ${CMAKE_COMMAND} -E echo "[WARN] Skip kmod uninstall softirq: kernel build directory not found: ${KDIR}"
    COMMENT "Skipping softirq_collector uninstall"
  )

  add_custom_target(kmod_uninstall_cpustat
    COMMAND ${CMAKE_COMMAND} -E echo "[WARN] Skip kmod uninstall cpustat: kernel build directory not found: ${KDIR}"
    COMMENT "Skipping cpu_stat_collector uninstall"
  )

  add_custom_target(kmod_uninstall
    DEPENDS kmod_uninstall_softirq kmod_uninstall_cpustat
  )

  add_custom_target(kmod_info
    COMMAND ${CMAKE_COMMAND} -E echo "[WARN] Skip kmod info: kernel build directory not found: ${KDIR}"
    DEPENDS kmod_modules
    COMMENT "Skipping kernel module info"
  )
endif()

add_custom_target(kmod_log
  COMMAND sudo dmesg | tail -30
  COMMENT "Showing recent kernel logs"
)

find_program(GCC_EXECUTABLE NAMES gcc cc)
if(GCC_EXECUTABLE)
  add_custom_target(kmod_test_cpu_stat
    COMMAND "${GCC_EXECUTABLE}" -o "${KMOD_DIR}/test_cpu_stat" "${KMOD_DIR}/test_cpu_stat.c"
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Building test_cpu_stat userspace tester"
  )

  add_custom_target(kmod_test_softirq
    COMMAND "${GCC_EXECUTABLE}" -o "${KMOD_DIR}/test_softirq" "${KMOD_DIR}/test_softirq.c"
    WORKING_DIRECTORY "${KMOD_DIR}"
    COMMENT "Building test_softirq userspace tester"
  )
else()
  message(WARNING "gcc/cc not found, skip kmod test program build targets")
endif()
