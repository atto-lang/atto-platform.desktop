# Validate that requesting real guest execution never silently falls back
# to the host-only scalar stub. Does not invoke an Oreslang compiler.
if(NOT DEFINED ORES_DESKTOP_SOURCE_DIR)
  message(FATAL_ERROR "ORES_DESKTOP_SOURCE_DIR required")
endif()
if(NOT DEFINED ORES_DESKTOP_TEST_ROOT)
  message(FATAL_ERROR "ORES_DESKTOP_TEST_ROOT required")
endif()
function(require_rejection case_name compiler_path)
  set(build_dir "${ORES_DESKTOP_TEST_ROOT}/${case_name}")
  file(REMOVE_RECURSE "${build_dir}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${ORES_DESKTOP_SOURCE_DIR}"
      -B "${build_dir}" -DBUILD_TESTING=ON
      -DORES_DESKTOP_WITH_SDL3=OFF
      -DORES_DESKTOP_REQUIRE_LLVM_GUEST=ON
      "-DORES_DESKTOP_LLVMC=${compiler_path}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE stdout_text
    ERROR_VARIABLE stderr_text
  )
  if("${exit_code}" STREQUAL "0")
    message(FATAL_ERROR "${case_name}: expected fail-closed rejection, but CMake succeeded")
  endif()
  if(NOT "${stdout_text}\n${stderr_text}" MATCHES "ORES_DESKTOP_LLVMC")
    message(FATAL_ERROR "${case_name}: rejected without the expected compiler admission diagnostic")
  endif()
endfunction()
require_rejection(missing "")
require_rejection(relative "relative/oreslang-llvmc")
message(STATUS "Real LLVM guest compiler admission: negative tests passed")
