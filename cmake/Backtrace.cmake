add_library(dingosdk_backtrace_client STATIC Engine/Core/Debug/client.cpp)
add_library(dingosdk_backtrace_upload STATIC Engine/Core/Debug/upload.cpp
    Engine/Core/Debug/native_dump.cpp Engine/Core/Debug/multipart.cpp)
target_link_libraries(dingosdk_backtrace_upload PRIVATE dingosdk_json winhttp bcrypt)
target_sources(dingosdk_launcher PRIVATE Launcher/crash_reporter.cpp)
target_link_libraries(dingosdk_launcher PRIVATE dingosdk_backtrace_upload dingosdk_json dbghelp)
target_link_libraries(dingosdk_logging PRIVATE dingosdk_backtrace_client)
# The launcher hosts crash capture for both executables; no third binary is shipped.
add_dependencies(dingosdk_runtime dingosdk_launcher)

# Release dumps need matching symbols; keep optimization and omit incremental linking.
# PDBALTPATH embeds only the PDB's file name, not the build machine's folder.
foreach(target dingosdk_runtime dingosdk_launcher dingosdk_server)
    target_compile_options(${target} PRIVATE /Zi)
    target_link_options(${target} PRIVATE /DEBUG:FULL /INCREMENTAL:NO /OPT:REF /OPT:ICF
        "/PDBALTPATH:$<TARGET_PDB_FILE_NAME:${target}>")
endforeach()
get_property(backtrace_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS backtrace_targets)
    get_target_property(kind ${target} TYPE)
    if(kind STREQUAL "STATIC_LIBRARY")
        target_compile_options(${target} PRIVATE /Zi)
    endif()
endforeach()

option(DINGOSDK_BUILD_BACKTRACE_TESTS "Build crash capture and upload regression tests" OFF)
if(DINGOSDK_BUILD_BACKTRACE_TESTS)
    enable_testing()
    add_executable(dingosdk_backtrace_tests Engine/Core/Debug/Test/backtrace_tests.cpp)
    target_link_libraries(dingosdk_backtrace_tests PRIVATE dingosdk_logging dingosdk_backtrace_upload dingosdk_json dbghelp)
    add_dependencies(dingosdk_backtrace_tests dingosdk_launcher)
    add_test(NAME backtrace COMMAND dingosdk_backtrace_tests "${CMAKE_CURRENT_BINARY_DIR}/backtrace-fixtures")
    set_tests_properties(backtrace PROPERTIES TIMEOUT 90)
endif()
