# Warning set for C, adapted from cpp-best-practices chapter 2 and the flags
# Effective C (2nd ed., chapter 11) recommends for GCC and Clang.
#
# The compiler is the cheapest static analyser you will ever run, and in C --
# a language that will happily let you return the address of a local -- it is
# also the first line of defence. Turning these on, and turning them into
# errors, is the single highest-value thing you can do to a C build. Every
# reference solution in this course is clean under this set.

function(mec_set_project_warnings target as_errors)
  set(clang_warnings
      -Wall
      -Wextra # reasonable and standard
      -Wshadow # a declaration shadows one from an enclosing scope
      -Wcast-align # casts that could produce a misaligned pointer (02.08)
      -Wcast-qual # casting away const -- usually a design smell (03.04)
      -Wunused # anything unused
      -Wpedantic # non-standard C is used
      -Wconversion # type conversions that may lose data (02.01)
      -Wsign-conversion # sign conversions that may lose data (02.02)
      -Wnull-dereference
      -Wdouble-promotion # float implicitly promoted to double
      -Wformat=2 # security issues around printf-family formatting
      -Wimplicit-fallthrough # a switch case falls through silently (13.01)
      -Wstrict-prototypes # f() declares NO parameters in C17; say f(void)
      -Wold-style-definition # K&R-style definitions have been dead since 1989
      -Wvla # variable-length arrays: banned on MCU stacks (08.07)
      -Wwrite-strings # string literals are const in spirit; make it checked
  )

  set(gcc_warnings
      ${clang_warnings}
      -Wmisleading-indentation
      -Wduplicated-cond
      -Wduplicated-branches
      -Wlogical-op
      -Wjump-misses-init # a goto/switch jumps over an initialisation (09.03)
  )

  if(CMAKE_C_COMPILER_ID MATCHES ".*Clang")
    set(warnings ${clang_warnings})
  elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(warnings ${gcc_warnings})
  endif()

  if(as_errors)
    list(APPEND warnings -Werror)
  endif()

  target_compile_options(${target} INTERFACE ${warnings})
endfunction()
