# Exercise fail-closed compiler admission even without an Oreslang compiler.
# A genuine guest smoke is a separate CTest, never substituted by this test.
if(NOT DEFINED ORES_DESKTOP_SOURCE_DIR)
  message(FATAL_ERROR "ORES_DESKTOP_SOURCE_DIR required")
endif()
if(NOT DEFINED ORES_DESKTOP_TEST_ROOT)
  message(FATAL_ERROR "ORES_DESKTOP_TEST_ROOT required")
endif()
function(require_rejection case_name compiler_path digest diagnostic)
  set(build_dir "${ORES_DESKTOP_TEST_ROOT}/${case_name}")
  file(REMOVE_RECURSE "${build_dir}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${ORES_DESKTOP_SOURCE_DIR}"
      -B "${build_dir}" -DBUILD_TESTING=ON
      -DORES_DESKTOP_WITH_SDL3=OFF
      -DORES_DESKTOP_REQUIRE_LLVM_GUEST=ON
      "-DORES_DESKTOP_LLVMC=${compiler_path}"
      "-DORES_DESKTOP_LLVMC_SHA256=${digest}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE stdout_text
    ERROR_VARIABLE stderr_text
  )
  if("${exit_code}" STREQUAL "0")
    message(FATAL_ERROR "${case_name}: expected rejection; configure succeeded")
  endif()
  if(NOT "${stdout_text}\n${stderr_text}" MATCHES "${diagnostic}")
    message(FATAL_ERROR "${case_name}: failure was not the expected compiler admission diagnostic")
  endif()
endfunction()
set(bad_digest "0000000000000000000000000000000000000000000000000000000000000000")
require_rejection(missing "" "${bad_digest}" "needs ORES_DESKTOP_LLVMC")
require_rejection(relative "relative/oreslang-llvmc" "${bad_digest}" "must name an existing absolute file")
require_rejection(nonexistent_absolute "${ORES_DESKTOP_TEST_ROOT}/compiler-not-installed"
  "${bad_digest}" "must name an existing absolute file")
require_rejection(missing_digest "${CMAKE_COMMAND}" "" "requires ORES_DESKTOP_LLVMC_SHA256")
require_rejection(wrong_digest "${CMAKE_COMMAND}" "${bad_digest}"
  "ORE[S]_DESKTOP_LLVMC_SHA256 disagrees")
message(STATUS "Real LLVM guest compiler admission: negative tests passed")
