cmake_minimum_required(VERSION 3.20)
project(oreslang_desktop VERSION 0.1.0 LANGUAGES C)
include(CTest)
option(ORES_DESKTOP_WITH_SDL3 "Build actual SDL3 desktop window host" OFF)
option(ORES_DESKTOP_REQUIRE_LLVM_GUEST
  "Require an independently supplied LLVM compiler for source-to-native guest smoke" OFF)
set(ORES_DESKTOP_LLVMC "" CACHE FILEPATH
  "Absolute path to an independently built and provenance-checked oreslang-llvmc")
set(ORES_DESKTOP_LLVMC_SHA256 "" CACHE STRING
  "Expected SHA-256 digest of the independently built oreslang-llvmc")
if(ORES_DESKTOP_REQUIRE_LLVM_GUEST AND (NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING))
  message(FATAL_ERROR
    "Required LLVM guest integration must be a native executable CTest build")
endif()
if(ORES_DESKTOP_REQUIRE_LLVM_GUEST AND NOT ORES_DESKTOP_LLVMC)
  message(FATAL_ERROR
    "ORES_DESKTOP_REQUIRE_LLVM_GUEST=ON needs ORES_DESKTOP_LLVMC")
endif()
if(ORES_DESKTOP_REQUIRE_LLVM_GUEST AND NOT ORES_DESKTOP_LLVMC_SHA256)
  message(FATAL_ERROR "Real guest execution requires ORES_DESKTOP_LLVMC_SHA256")
endif()
if(ORES_DESKTOP_LLVMC_SHA256 AND NOT ORES_DESKTOP_LLVMC)
  message(FATAL_ERROR "ORES_DESKTOP_LLVMC_SHA256 requires ORES_DESKTOP_LLVMC")
endif()
if(ORES_DESKTOP_LLVMC)
  if(NOT IS_ABSOLUTE "${ORES_DESKTOP_LLVMC}" OR
     NOT EXISTS "${ORES_DESKTOP_LLVMC}" OR
     IS_DIRECTORY "${ORES_DESKTOP_LLVMC}" OR
     IS_SYMLINK "${ORES_DESKTOP_LLVMC}")
    message(FATAL_ERROR
      "ORES_DESKTOP_LLVMC must name an existing absolute file that is not a symlink")
  endif()
  if(ORES_DESKTOP_LLVMC_SHA256)
    string(TOLOWER "${ORES_DESKTOP_LLVMC_SHA256}" expected_compiler_sha256)
    string(LENGTH "${expected_compiler_sha256}" expected_sha256_length)
    if(NOT expected_sha256_length EQUAL 64 OR
       NOT expected_compiler_sha256 MATCHES "^[0-9a-f]+$")
      message(FATAL_ERROR "ORES_DESKTOP_LLVMC_SHA256 must be 64 hexadecimal digits")
    endif()
    file(SHA256 "${ORES_DESKTOP_LLVMC}" actual_compiler_sha256)
    if(NOT actual_compiler_sha256 STREQUAL expected_compiler_sha256)
      message(FATAL_ERROR "ORES_DESKTOP_LLVMC_SHA256 disagrees with the binary at the configured path")
    endif()
  endif()
  if(WIN32 OR CMAKE_CROSSCOMPILING OR NOT BUILD_TESTING)
    message(FATAL_ERROR "LLVM guest smoke currently needs a native Unix CTest runner")
  endif()
  find_program(ORES_DESKTOP_BASH bash REQUIRED)
  find_program(ORES_DESKTOP_CLANG clang REQUIRED)
endif()
add_library(ores_desktop STATIC src/ores_desktop.c)
target_include_directories(ores_desktop PUBLIC include)
target_compile_features(ores_desktop PUBLIC c_std_11)
if(MSVC)
  target_compile_options(ores_desktop PRIVATE /W4 /WX)
else()
  target_compile_options(ores_desktop PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(ORES_DESKTOP_WITH_SDL3)
  find_package(SDL3 CONFIG REQUIRED)
  add_library(ores_desktop_sdl3 STATIC src/ores_desktop_sdl3.c)
  target_include_directories(ores_desktop_sdl3 PUBLIC include)
  target_link_libraries(ores_desktop_sdl3 PUBLIC ores_desktop SDL3::SDL3)
  add_executable(ores_desktop_demo examples/sdl3_demo.c)
  target_link_libraries(ores_desktop_demo PRIVATE ores_desktop_sdl3)
endif()
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  if(ORES_DESKTOP_REQUIRE_LLVM_GUEST)
    add_test(NAME desktop_real_llvm_guest_link
      COMMAND "${ORES_DESKTOP_BASH}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/llvm-link-smoke.sh"
        "${ORES_DESKTOP_LLVMC}" "${expected_compiler_sha256}")
    set_tests_properties(desktop_real_llvm_guest_link PROPERTIES
      TIMEOUT 120 LABELS "real-guest;compiler-required")
  endif()
  if(UNIX)
    find_program(ORES_DESKTOP_TEST_BASH bash)
    find_program(ORES_DESKTOP_TEST_CLANG clang)
    if(ORES_DESKTOP_TEST_BASH AND ORES_DESKTOP_TEST_CLANG)
      add_test(NAME desktop_llvm_link_smoke_guard
        COMMAND "${ORES_DESKTOP_TEST_BASH}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/llvm_link_smoke_guard.sh")
      set_tests_properties(desktop_llvm_link_smoke_guard PROPERTIES
        TIMEOUT 60 LABELS "host-only;llvm-gate-regression")
    endif()
  endif()
  add_test(NAME desktop_guest_compiler_admission_negative
    COMMAND "${CMAKE_COMMAND}"
      "-DORES_DESKTOP_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DORES_DESKTOP_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/guest-admission-negative"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/guest_compiler_admission.cmake")
  set_tests_properties(desktop_guest_compiler_admission_negative PROPERTIES TIMEOUT 120)
  add_executable(test_desktop tests/test_desktop.c)
  target_link_libraries(test_desktop PRIVATE ores_desktop)
  add_test(NAME desktop_host_abi_lifecycle COMMAND test_desktop)
  set_tests_properties(desktop_host_abi_lifecycle PROPERTIES TIMEOUT 10)
  add_executable(test_desktop_burst tests/test_desktop_burst.c)
  target_link_libraries(test_desktop_burst PRIVATE ores_desktop)
  add_executable(test_desktop_guest_scalar_host
    examples/llvm_host_bridge.c tests/guest_scalar_host_stub.c)
  target_link_libraries(test_desktop_guest_scalar_host PRIVATE ores_desktop)
  add_test(NAME desktop_guest_scalar_host_stub COMMAND test_desktop_guest_scalar_host)
  set_tests_properties(desktop_guest_scalar_host_stub PROPERTIES TIMEOUT 10)
  add_executable(test_desktop_guest_scalar_reject
    examples/llvm_host_bridge.c tests/guest_scalar_host_stub.c)
  target_link_libraries(test_desktop_guest_scalar_reject PRIVATE ores_desktop)
  target_compile_definitions(test_desktop_guest_scalar_reject
    PRIVATE ORES_DESKTOP_TEST_MALFORMED_SCALAR)
  add_test(NAME desktop_guest_scalar_reject_invalid
    COMMAND test_desktop_guest_scalar_reject)
  set_tests_properties(desktop_guest_scalar_reject_invalid
    PROPERTIES TIMEOUT 10)
  foreach(contract_test test_desktop test_desktop_burst test_desktop_guest_scalar_host test_desktop_guest_scalar_reject)
    if(MSVC)
      target_compile_options(${contract_test} PRIVATE /W4 /WX)
    else()
      target_compile_options(${contract_test} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
  endforeach()
  add_test(NAME desktop_event_budget_fairness COMMAND test_desktop_burst)
  set_tests_properties(desktop_event_budget_fairness PROPERTIES TIMEOUT 10)
endif()

