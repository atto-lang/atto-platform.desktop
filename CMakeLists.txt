cmake_minimum_required(VERSION 3.20)
project(oreslang_desktop VERSION 0.1.0 LANGUAGES C)
include(CTest)
option(ORES_DESKTOP_WITH_SDL3 "Build actual SDL3 desktop window host" OFF)
option(ORES_DESKTOP_REQUIRE_LLVM_GUEST
  "Require an independently supplied LLVM compiler for source-to-native guest smoke" OFF)
set(ORES_DESKTOP_LLVMC "" CACHE FILEPATH
  "Absolute path to an independently built and provenance-checked oreslang-llvmc")
if(ORES_DESKTOP_REQUIRE_LLVM_GUEST AND (NOT BUILD_TESTING OR CMAKE_CROSSCOMPILING))
  message(FATAL_ERROR
    "Required LLVM guest integration must be a native executable CTest build")
endif()
if(ORES_DESKTOP_REQUIRE_LLVM_GUEST AND NOT ORES_DESKTOP_LLVMC)
  message(FATAL_ERROR
    "ORES_DESKTOP_REQUIRE_LLVM_GUEST=ON needs ORES_DESKTOP_LLVMC")
endif()
if(ORES_DESKTOP_LLVMC)
  if(NOT IS_ABSOLUTE "${ORES_DESKTOP_LLVMC}" OR
     NOT EXISTS "${ORES_DESKTOP_LLVMC}" OR
     IS_DIRECTORY "${ORES_DESKTOP_LLVMC}")
    message(FATAL_ERROR "ORES_DESKTOP_LLVMC must name an existing absolute file")
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
  if(ORES_DESKTOP_LLVMC)
    add_test(NAME desktop_real_llvm_guest_link
      COMMAND "${ORES_DESKTOP_BASH}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/llvm-link-smoke.sh"
        "${ORES_DESKTOP_LLVMC}")
    set_tests_properties(desktop_real_llvm_guest_link PROPERTIES
      TIMEOUT 120 LABELS "real-guest;compiler-required")
  endif()
  add_test(NAME desktop_guest_compiler_admission_negative
    COMMAND "${CMAKE_COMMAND}"
      "-DORES_DESKTOP_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DORES_DESKTOP_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/guest-admission-negative"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/guest_compiler_admission.cmake")
  set_tests_properties(desktop_guest_compiler_admission_negative PROPERTIES TIMEOUT 30)
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

