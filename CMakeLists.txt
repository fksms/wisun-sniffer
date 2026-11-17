cmake_minimum_required(VERSION 3.13)
project(wisun-sniffer LANGUAGES C)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UHD REQUIRED IMPORTED_TARGET uhd)
pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)

add_executable(${PROJECT_NAME}
    main.c
    usrp.c
    lfrb.c
    channelizer.c
    fir_kaiser.c
    brb.c
    demodulator.c
    fsk.c
    channelizer_test.c
    packet.c
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    PkgConfig::UHD
    PkgConfig::FFTW
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:MinSizeRel>>:-fsanitize=address>
    -fno-common
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Werror=implicit-function-declaration
    -Wno-deprecated-declarations
)

target_link_options(${PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:MinSizeRel>>:-fsanitize=address>
)
