cmake_minimum_required(VERSION 3.15)
project(adc_processor C)

# Requirement: target the C99 standard.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Helpful warnings during development.
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

add_executable(adc_processor
    main.c
    adc.c
    io.c
    stats.c
)

# math library (sqrt) on Unix-like toolchains.
if (UNIX)
    target_link_libraries(adc_processor m)
endif()
