cmake_minimum_required(VERSION 3.19)
project(armajitto VERSION 1.0.0)

# CMake project structured adapted from Alex Reinking's SharedStaticStarter
# GitHub: https://github.com/alexreinking/SharedStaticStarter
# Blog: https://alexreinking.com/blog/building-a-dual-shared-and-static-library-with-cmake.html

## C++ language configuration boilerplate
if (NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET AND
    NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif ()

## Let armajitto_SHARED_LIBS override BUILD_SHARED_LIBS
if (DEFINED armajitto_SHARED_LIBS)
    set(BUILD_SHARED_LIBS "${armajitto_SHARED_LIBS}")
endif ()

## Create the main armajitto library target
add_library(armajitto 
    include/armajitto/armajitto.hpp
    include/armajitto/arm/mode.hpp
    include/armajitto/arm/psr.hpp
    include/armajitto/arm/state.hpp
    include/armajitto/core/recompiler.hpp
    include/armajitto/core/specification.hpp
    include/armajitto/core/system_interface.hpp
    include/armajitto/defs/cpu_model.hpp
    include/armajitto/defs/memory_access.hpp
    src/abi/x86_64/cpuid.cpp
    src/abi/x86_64/cpuid.hpp
    src/allocator/arena.hpp
    src/core/recompiler.cpp
)
add_library(armajitto::armajitto ALIAS armajitto)
set_target_properties(armajitto PROPERTIES
                      VERSION ${armajitto_VERSION}
                      SOVERSION ${armajitto_VERSION_MAJOR})
target_include_directories(armajitto
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_compile_features(armajitto PUBLIC cxx_std_20)

## Build precompiled header
target_precompile_headers(armajitto
    PUBLIC
        <array>
        <cassert>
        <cstdint>
)

## Define version string and numbers as compiler macros
target_compile_definitions(armajitto PUBLIC ARMAJITTO_VERSION="${armajitto_VERSION}")
target_compile_definitions(armajitto PUBLIC ARMAJITTO_VERSION_MAJOR=${armajitto_VERSION_MAJOR}u)
target_compile_definitions(armajitto PUBLIC ARMAJITTO_VERSION_MINOR=${armajitto_VERSION_MINOR}u)
target_compile_definitions(armajitto PUBLIC ARMAJITTO_VERSION_PATCH=${armajitto_VERSION_PATCH}u)

######### TEMPORARY #########
## Add a test executable to aid development
add_executable(armajitto-test
    src/test.cpp
)
add_executable(armajitto::armajitto-test ALIAS armajitto-test)
set_target_properties(armajitto-test PROPERTIES
                      VERSION ${armajitto_VERSION}
                      SOVERSION ${armajitto_VERSION_MAJOR})
target_link_libraries(armajitto-test PRIVATE armajitto)
target_compile_features(armajitto-test PUBLIC cxx_std_20)
######### TEMPORARY #########

## Configure Visual Studio solution
if (MSVC)
    include(cmake/VSHelpers.cmake)

    ######### TEMPORARY #########
    ## Configure startup project
    set_property(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" PROPERTY VS_STARTUP_PROJECT armajitto-test)

    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    ## Configure Visual Studio filters and debugging
    vs_set_filters(TARGET armajitto)
    
    set_target_properties(armajitto PROPERTIES FOLDER "armajitto")
endif ()

## Generate the export header for armajitto and attach it to the target
include(GenerateExportHeader)
generate_export_header(armajitto EXPORT_FILE_NAME include/armajitto/export.h)
target_compile_definitions(armajitto PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:armajitto_STATIC_DEFINE>")
target_include_directories(armajitto PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

## Determine if this is a top-level build or included as a subproject
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)

## Include the install rules if the user wanted them (included by default when top-level)
option(armajitto_INCLUDE_PACKAGING "Include packaging rules for armajitto" "${is_top_level}")
if (armajitto_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
