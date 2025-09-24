cmake_minimum_required(VERSION 3.20)
project(nes-emulator C)

# -------------------------------
# Language / warnings
# -------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# -------------------------------
# Toggleable DEBUG_TRACE
# -------------------------------
# Turn on with: -DDEBUG_TRACE=ON  (CLion: Settings -> CMake -> CMake options)
option(DEBUG_TRACE "Enable verbose CPU trace logs" OFF)

# Also auto-enable in Debug config (you can delete this if you prefer manual only)
# This makes DEBUG builds trace by default; Release/RelWithDebInfo won't unless you pass -DDEBUG_TRACE=ON
#set(_TRACE_GENEX $<$<OR:$<BOOL:${DEBUG_TRACE}>,$<CONFIG:Debug>>:DEBUG_TRACE=1>)

# -------------------------------
# Include paths
# -------------------------------
set(PROJ_INC_DIRS
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cpu
        ${CMAKE_SOURCE_DIR}/src/ppu
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/src/input
        ${CMAKE_SOURCE_DIR}/src/cartridge
        ${CMAKE_SOURCE_DIR}/src/rom
        ${CMAKE_SOURCE_DIR}/src/nes
)

# -------------------------------
# Core library (NO bus here)
# -------------------------------
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
        src/ppu/ppu_oam.c
        src/ppu/ppu_regs.c
        src/ppu/ppu_timing.c

        # Cartridge + mappers
        src/cartridge/cartridge.c
        src/cartridge/ines.c
        src/cartridge/mapper.c
        src/cartridge/mapper_nrom.c

        # Input
        src/input/controller.c

        # NES
        src/nes/nes.c
        src/nes/rom_loader.c
)

target_include_directories(nes-emulator-core PUBLIC ${PROJ_INC_DIRS})
target_compile_definitions(nes-emulator-core PRIVATE ${_TRACE_GENEX})

# -------------------------------
# Emulator executable (real BUS)
# -------------------------------
add_executable(nes-emulator
        src/main.c
        src/memory/bus.c        # provides cpu_read/cpu_write for the real emulator
)
target_include_directories(nes-emulator PRIVATE ${PROJ_INC_DIRS})
target_link_libraries(nes-emulator PRIVATE nes-emulator-core)
target_compile_definitions(nes-emulator PRIVATE ${_TRACE_GENEX})

# Put binary at build root (handy for CLion run configs)
set_target_properties(nes-emulator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# -------------------------------
# Tests (mock bus)
# -------------------------------
enable_testing()

add_executable(cpu-core-tests
        tests/test_bus.c        # mock cpu_read/cpu_write for unit tests
        tests/test_cpu_core.c
)
target_include_directories(cpu-core-tests PRIVATE ${PROJ_INC_DIRS} ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(cpu-core-tests PRIVATE nes-emulator-core)
target_compile_definitions(cpu-core-tests PRIVATE ${_TRACE_GENEX})
add_test(NAME cpu-core-tests COMMAND cpu-core-tests)

#----------------------------
# Test PPU/VBlank
#----------------------------
option(ENABLE_PPU_TRACE "Log one line per frame and VBL edges" OFF)

if (ENABLE_PPU_TRACE)
    target_compile_definitions(nes-emulator PRIVATE PPU_TRACE=1)
    target_compile_definitions(nes-emulator-core PRIVATE PPU_TRACE=1)
    # keep tests consistent if you like
    if (TARGET tests)
        target_compile_definitions(tests PRIVATE PPU_TRACE=1)
    endif()
endif()


# Notes:
# * The core library excludes the real bus so tests can link a mock bus without duplicate symbols.
# * To force trace ON for any config: pass -DDEBUG_TRACE=ON in CLion's CMake options.
# * To force trace OFF even in Debug: pass -DDEBUG_TRACE=OFF.
