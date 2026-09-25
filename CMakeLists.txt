cmake_minimum_required(VERSION 3.24)

project(
  modern_embedded_c_exercises
  VERSION 1.0.0
  DESCRIPTION "100 exercises to learn modern embedded C (C17), plus a NUCLEO track"
  LANGUAGES C)

# ---------------------------------------------------------------------------
# Language level.
#
# We ask for C17 and refuse to silently fall back to an older standard
# (C_STANDARD_REQUIRED) or to enable vendor extensions such as GNU's `typeof`
# (C_EXTENSIONS OFF -> -std=c17 instead of -std=gnu17). C17 is C11 with the
# defect reports applied -- no new features -- and it is what current embedded
# toolchains (GCC for ARM, IAR, recent Keil) actually support. Nothing here
# needs C23, and plenty of embedded toolchains do not have it yet.
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Emit compile_commands.json so clang-tidy, clangd and CLion all see the exact
# flags each translation unit is compiled with.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(Sanitizers)
include(AddExercise)

option(MEC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(MEC_BUILD_EXERCISES "Build the exercise targets (your work)" ON)
option(MEC_BUILD_SOLUTIONS "Build the reference solutions" OFF)

# ---------------------------------------------------------------------------
# Two interface targets carry every setting the exercises share.
#
# This is the modern CMake idiom: no global `add_compile_options`, no
# directory-scoped state leaking into subdirectories. A target says what it
# needs, and consumers inherit it through `target_link_libraries`.
# ---------------------------------------------------------------------------
add_library(mec_options INTERFACE)
add_library(mec::options ALIAS mec_options)
target_compile_features(mec_options INTERFACE c_std_17)

add_library(mec_warnings INTERFACE)
add_library(mec::warnings ALIAS mec_warnings)
mec_set_project_warnings(mec_warnings ${MEC_WARNINGS_AS_ERRORS})

if(NOT CMAKE_CROSSCOMPILING)
  mec_enable_sanitizers(mec_options)

  # A few host exercises (chapters 11 and 12) demonstrate ISR/mainline races
  # with a second thread standing in for the interrupt. POSIX threads exist on
  # macOS and on embedded Linux, which is exactly the point being taught.
  find_package(Threads REQUIRED)
  target_link_libraries(mec_options INTERFACE Threads::Threads)
endif()

add_subdirectory(third_party)

if(CMAKE_CROSSCOMPILING)
  # The NUCLEO-L476RG track (chapters 15-17). The board support code provides
  # the linker script, startup and the UART the harness prints through.
  add_subdirectory(bsp)
endif()

enable_testing()

if(MEC_BUILD_EXERCISES)
  add_subdirectory(exercises)
endif()

if(MEC_BUILD_SOLUTIONS)
  add_subdirectory(solutions)
endif()

message(STATUS "")
message(STATUS "  modern embedded C exercises")
message(STATUS "  compiler ............ ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  build type .......... ${CMAKE_BUILD_TYPE}")
message(STATUS "  warnings as errors .. ${MEC_WARNINGS_AS_ERRORS}")
message(STATUS "  exercises ........... ${MEC_BUILD_EXERCISES}")
message(STATUS "  solutions ........... ${MEC_BUILD_SOLUTIONS}")
if(CMAKE_CROSSCOMPILING)
  message(STATUS "  target .............. STM32L476RG (NUCLEO track, chapters 15-17)")
else()
  message(STATUS "  sanitizers .......... ${MEC_SANITIZER}")
endif()
message(STATUS "")
