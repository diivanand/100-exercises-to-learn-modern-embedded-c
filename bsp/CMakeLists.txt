# Board support for the NUCLEO-L476RG track. Only entered when
# cross-compiling (see the top-level CMakeLists.txt).

add_library(mec_bsp STATIC
            startup_stm32l476.c
            bsp.c
            mect_port_nucleo.c)
add_library(mec::bsp ALIAS mec_bsp)

target_include_directories(mec_bsp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mec_bsp PUBLIC mec::mect_core mec::options
                      PRIVATE mec::warnings)
