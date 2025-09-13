set(TEST_SOURCES
        test_cpu.c
        test_bus.c
        test_mapper.c
)

add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests PRIVATE nes-emulator-core)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

# Keep sanitizer parity when supported
if (NOT MSVC AND SANITIZERS_SUPPORTED)
    target_compile_options(tests PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(tests PRIVATE -fsanitize=address -fsanitize=undefined)
endif()

add_test(NAME nes_all COMMAND tests)
