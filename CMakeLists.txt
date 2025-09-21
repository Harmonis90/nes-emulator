# CMakeLists.txt — NES Emulator (C)
cmake_minimum_required(VERSION 3.20)
project(nes-emulator C)

# ------------------------------------------------------------------------------
# Language / warnings
# ------------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------------------------
# Include paths
# ------------------------------------------------------------------------------
set(PROJ_INC_DIRS
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cpu
        ${CMAKE_SOURCE_DIR}/src/ppu
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/src/input
        ${CMAKE_SOURCE_DIR}/src/cartridge
        ${CMAKE_SOURCE_DIR}/src/rom
)

# ------------------------------------------------------------------------------
# Core library (NO BUS HERE!)
#   The core is everything that doesn’t define cpu_read/cpu_write.
#   This lets unit tests provide a mock bus without duplicate symbols.
# ------------------------------------------------------------------------------
add_library(nes-emulator-core
        # CPU
        src/cpu/cpu.c
        src/cpu/cpu_addr.c
        src/cpu/cpu_internal.c
        src/cpu/cpu_ops.c
        src/cpu/cpu_table.c
        src/cpu/cpu_timing.c

        # PPU
        src/ppu/ppu.c
        src/ppu/ppu_mem.c
        src/ppu/ppu_regs.c

        # Cartridge + mappers
        src/cartridge/cartridge.c
        src/cartridge/ines.c
        src/cartridge/mapper.c
        src/cartridge/mapper_nrom.c

        # Input
        src/input/controller.c

        # Utils / headers-only are picked up via include dirs
)

target_include_directories(nes-emulator-core PUBLIC ${PROJ_INC_DIRS})

# ------------------------------------------------------------------------------
# Emulator executable (real BUS implementation linked here)
# ------------------------------------------------------------------------------
add_executable(nes-emulator
        src/main.c
        src/memory/bus.c      # <- provides cpu_read/cpu_write for the real emulator
)

target_include_directories(nes-emulator PRIVATE ${PROJ_INC_DIRS})
target_link_libraries(nes-emulator PRIVATE nes-emulator-core)

# Put the emulator exe at the build root (nice for CLion Run Configs)
set_target_properties(nes-emulator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ------------------------------------------------------------------------------
# Tests
#   cpu-core-tests links nes-emulator-core and provides a MOCK bus,
#   so we do NOT link src/memory/bus.c into this test target.
# ------------------------------------------------------------------------------
enable_testing()

add_executable(cpu-core-tests
        tests/test_bus.c          # mock cpu_read/cpu_write
        tests/test_cpu_core.c
)
target_include_directories(cpu-core-tests PRIVATE ${PROJ_INC_DIRS} ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(cpu-core-tests PRIVATE nes-emulator-core)
add_test(NAME cpu-core-tests COMMAND cpu-core-tests)

# Example placeholder for PPU tests (add when ready):
# add_executable(ppu-tests tests/test_ppu.c tests/test_bus.c)
# target_include_directories(ppu-tests PRIVATE ${PROJ_INC_DIRS} ${CMAKE_SOURCE_DIR}/tests)
# target_link_libraries(ppu-tests PRIVATE nes-emulator-core)
# add_test(NAME ppu-tests COMMAND ppu-tests)

# ------------------------------------------------------------------------------
# Notes
# ------------------------------------------------------------------------------
# * The core library intentionally excludes src/memory/bus.c so unit tests can
#   provide a mock bus without duplicate symbol errors.
# * On Windows/MinGW, nothing special is required here; CLion’s toolchain will
#   pick up the right linker. If you add the APU later, just append its sources
#   to nes-emulator-core.
# * Run:  ./nes-emulator <path-to-rom>
