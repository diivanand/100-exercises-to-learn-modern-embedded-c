# mect is this course's test harness, vendored so the repository builds with
# no network access. It is ~400 lines of plain C17; reading it is part of the
# course (07.07 explains its CHECK_EQ macro).

add_library(mec_mect_headers INTERFACE)
add_library(mec::mect_headers ALIAS mec_mect_headers)

# SYSTEM keeps our aggressive warning set from firing inside the harness's
# headers -- you are responsible for warnings in *your* code, not in a
# dependency's. (mect is kept clean under the full set anyway.)
target_include_directories(mec_mect_headers SYSTEM
                           INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

if(CMAKE_CROSSCOMPILING)
  # On the target, the harness core is compiled with the cross-compiler and
  # the port + main live in bsp/. See bsp/CMakeLists.txt.
  add_library(mec_mect_core STATIC mect/mect.c)
  add_library(mec::mect_core ALIAS mec_mect_core)
  target_link_libraries(mec_mect_core PUBLIC mec::mect_headers)
else()
  # On the host, harness + port + main() are compiled exactly once, here,
  # instead of once per exercise. With ~100 executables that matters.
  add_library(mec_mect_main STATIC mect/mect.c mect/mect_main_host.c)
  add_library(mec::mect_main ALIAS mec_mect_main)
  target_link_libraries(mec_mect_main PUBLIC mec::mect_headers)
endif()
