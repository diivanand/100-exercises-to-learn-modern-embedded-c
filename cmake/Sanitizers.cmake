# Runtime sanitizers catch the bugs the type system cannot: use-after-free,
# buffer overruns, signed overflow, data races. They cost ~2x runtime and are
# worth running over the whole test suite before you trust any code.
#
#   cmake --preset asan   && ctest --preset asan     # address + undefined
#   cmake --preset tsan   && ctest --preset tsan     # threads (chapter 12)
#
# On a desktop-class embedded Linux box you can run sanitizers on the device;
# on an MCU you cannot, which is precisely why this course makes you run your
# hardware-independent logic on the host first, where the sanitizers live.
# (Effective C ch. 11, "Dynamic Analysis"; Grenning calls it dual-targeting.)
#
# ASan and TSan are mutually exclusive: they both instrument memory and their
# runtimes conflict. That is why this is a single-choice variable, not a set
# of independent booleans.

set(MEC_SANITIZER
    "none"
    CACHE STRING "Runtime sanitizer: none | address | thread")
set_property(CACHE MEC_SANITIZER PROPERTY STRINGS none address thread)

function(mec_enable_sanitizers target)
  if(MEC_SANITIZER STREQUAL "none")
    return()
  endif()

  if(NOT (CMAKE_C_COMPILER_ID MATCHES ".*Clang" OR CMAKE_C_COMPILER_ID
                                                   STREQUAL "GNU"))
    message(WARNING "MEC_SANITIZER is only wired up for Clang and GCC")
    return()
  endif()

  if(MEC_SANITIZER STREQUAL "address")
    # UBSan composes with ASan, and the pair is the standard debug build.
    set(flags -fsanitize=address,undefined -fno-omit-frame-pointer
              -fno-sanitize-recover=undefined)
  elseif(MEC_SANITIZER STREQUAL "thread")
    set(flags -fsanitize=thread)
  else()
    message(FATAL_ERROR "Unknown MEC_SANITIZER value: ${MEC_SANITIZER}")
  endif()

  target_compile_options(${target} INTERFACE ${flags} -g)
  target_link_options(${target} INTERFACE ${flags})
endfunction()
