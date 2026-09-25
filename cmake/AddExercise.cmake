# Exercise discovery.
#
# Every exercise lives in `exercises/<NN_chapter>/<NN_name>/exercise.c` and is
# a self-contained mect translation unit. Rather than maintaining 115
# hand-written `add_executable` calls, we glob the tree and derive everything
# from the directory layout: add a directory, get a target and a test.
#
# Chapters 00-14 are host exercises: they build and run on your Mac (or any
# Linux box). Chapters 15-17 are the NUCLEO-L476RG track: they only build in
# the cross-compiling configuration (`cmake --preset target`), produce .elf
# and .bin files instead of tests, and are run on the board with `./mec flash`.
#
# `CONFIGURE_DEPENDS` makes the build system re-run the glob when the set of
# matching files changes. Globbing is often discouraged (CMake cannot know
# about a new file without re-running), but for a fixed, regular layout like
# this one it removes far more boilerplate than it costs.

# The first chapter of the on-target track. Everything below it runs on the
# host; everything from it on requires the cross toolchain and the board.
set(MEC_FIRST_TARGET_CHAPTER 15)

function(mec_discover_exercises kind)
  if(kind STREQUAL "exercise")
    set(prefix "ex")
  elseif(kind STREQUAL "solution")
    set(prefix "sol")
  else()
    message(FATAL_ERROR "mec_discover_exercises: kind must be exercise|solution")
  endif()
  set(root "${CMAKE_CURRENT_SOURCE_DIR}")

  file(
    GLOB_RECURSE sources
    CONFIGURE_DEPENDS
    "${root}/*/*/exercise.c")
  list(SORT sources)

  if(NOT sources)
    message(WARNING "No ${kind} sources found under ${root}")
    return()
  endif()

  set(all_targets "")
  set(skipped 0)
  foreach(source IN LISTS sources)
    # exercises/11_volatile_and_mmio/03_read_modify_write/exercise.c
    #   -> chapter "11_volatile_and_mmio", name "03_read_modify_write"
    file(RELATIVE_PATH relative "${root}" "${source}")
    get_filename_component(dir "${relative}" DIRECTORY)

    string(REPLACE "/" ";" parts "${dir}")
    list(GET parts 0 chapter)
    list(GET parts 1 name)

    string(REGEX REPLACE "^([0-9]+)_.*$" "\\1" chapter_number "${chapter}")

    set(test_name "${chapter_number}_${name}") # 11_03_read_modify_write
    set(target "${prefix}_${test_name}") # ex_11_03_read_modify_write

    # Host build skips the NUCLEO chapters; the cross build builds ONLY them.
    if(chapter_number GREATER_EQUAL MEC_FIRST_TARGET_CHAPTER)
      set(is_target ON)
    else()
      set(is_target OFF)
    endif()
    if(is_target AND NOT CMAKE_CROSSCOMPILING)
      math(EXPR skipped "${skipped} + 1")
      continue()
    endif()
    if(NOT is_target AND CMAKE_CROSSCOMPILING)
      math(EXPR skipped "${skipped} + 1")
      continue()
    endif()

    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE mec::options mec::warnings)

    if(is_target)
      target_link_libraries(${target} PRIVATE mec::bsp)
      # An exercise that ships its own linker script (15.01 does) overrides
      # the board support one; everything else links with bsp/stm32l476rg.ld.
      if(EXISTS "${root}/${dir}/link.ld")
        set(linker_script "${root}/${dir}/link.ld")
      else()
        set(linker_script "${CMAKE_SOURCE_DIR}/bsp/stm32l476rg.ld")
      endif()
      target_link_options(${target} PRIVATE "-T${linker_script}"
                          "-Wl,-Map=$<TARGET_FILE:${target}>.map")
      set_target_properties(${target} PROPERTIES LINK_DEPENDS
                                                 "${linker_script}")
      # st-flash wants a raw binary; keep it next to the .elf.
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:${target}>"
                "$<TARGET_FILE:${target}>.bin"
        COMMENT "objcopy ${target}.bin")
    else()
      target_link_libraries(${target} PRIVATE mec::mect_main)
    endif()

    # A handful of exercises only mean anything with the optimiser on --
    # 11.01's missing `volatile` is invisible at -O0. A marker file in the
    # exercise directory opts in, and the header comment says it is there.
    if(EXISTS "${root}/${dir}/optimize")
      target_compile_options(${target} PRIVATE -O2)
    endif()

    # Keep the binaries grouped so `ls` in the build tree is readable, and so
    # IDEs show the same folder structure as the source tree.
    set_target_properties(
      ${target}
      PROPERTIES FOLDER "${kind}s/${chapter}"
                 RUNTIME_OUTPUT_DIRECTORY
                 "${CMAKE_BINARY_DIR}/bin/${kind}s/${chapter}")

    if(NOT is_target)
      if(kind STREQUAL "exercise")
        set(ctest_name "${test_name}")
      else()
        set(ctest_name "solution/${test_name}")
      endif()
      add_test(NAME "${ctest_name}" COMMAND ${target})
    endif()

    list(APPEND all_targets ${target})
  endforeach()

  list(LENGTH all_targets count)
  if(CMAKE_CROSSCOMPILING)
    message(STATUS "Discovered ${count} NUCLEO ${kind}s (${skipped} host ${kind}s left to the host build)")
  elseif(skipped GREATER 0)
    message(STATUS "Discovered ${count} ${kind}s (${skipped} NUCLEO ${kind}s need the target preset)")
  else()
    message(STATUS "Discovered ${count} ${kind}s")
  endif()

  # A convenience target so `cmake --build build --target exercises` builds
  # everything in one go without also building the solutions.
  add_custom_target(${kind}s DEPENDS ${all_targets})
endfunction()
