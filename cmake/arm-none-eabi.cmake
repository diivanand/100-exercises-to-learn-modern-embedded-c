# Cross toolchain for the NUCLEO-L476RG track (chapters 15-17).
#
# Passed to CMake with -DCMAKE_TOOLCHAIN_FILE=..., which the `target` presets
# do. Everything target-specific about the *compiler* is here; everything
# target-specific about the *board* (linker script, startup, UART) is in bsp/.
#
# Install the toolchain with:  brew install --cask gcc-arm-embedded
# (or `brew install arm-none-eabi-gcc`; on Linux, apt install gcc-arm-none-eabi)

set(CMAKE_SYSTEM_NAME Generic) # "Generic" = bare metal: no OS, no syscalls
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# A bare-metal compiler cannot link a runnable test program during CMake's
# compiler sanity check (there is no OS to run it on), so check by building a
# static library instead.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# The STM32L476RG is a Cortex-M4F: ARMv7E-M, Thumb-2 only, single-precision
# FPU. -mfloat-abi=hard passes floats in FPU registers; the whole image must
# agree on this, which is why it lives in the toolchain file and not on a
# target.
set(MEC_MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

# -ffunction-sections/-fdata-sections + --gc-sections: every function and
# object gets its own section so the linker can discard the unused ones. This
# is how a 1 MB-flash part fits a libc.
set(CMAKE_C_FLAGS_INIT "${MEC_MCU_FLAGS} -ffunction-sections -fdata-sections")

# -Og keeps the code debuggable while still being honest about optimisation --
# unlike -O0, it will happily delete a read you did not mark volatile.
set(CMAKE_C_FLAGS_DEBUG_INIT "-Og -g3")

# nano.specs: newlib-nano, the small-footprint libc build. nosys.specs: stub
# out the OS calls a bare-metal image does not have. The -u pulls in
# newlib-nano's optional float formatting, which mect's CHECK_NEAR output
# uses; it costs a few KB of the 1 MB flash.
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${MEC_MCU_FLAGS} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -u _printf_float")

# Never look for headers or libraries on the build host.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
