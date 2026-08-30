cmake_minimum_required(VERSION 3.16)

project(CompiladorCasero LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

file(GLOB_RECURSE COMPILADOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

add_executable(compilador ${COMPILADOR_SOURCES})

target_include_directories(compilador PRIVATE
    "${CMAKE_SOURCE_DIR}/src/include"
)

if(MSVC)
    target_compile_options(compilador PRIVATE /W4 /O2)
else()
    target_compile_options(compilador PRIVATE -Wall -Wextra -Wpedantic -O3 -ffunction-sections -fdata-sections)
endif()

set_target_properties(compilador PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

message(STATUS "Configurando proyecto para ${CMAKE_SYSTEM_NAME}")
message(STATUS "Salida de ejecutable: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
