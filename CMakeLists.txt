cmake_minimum_required(VERSION 3.19)
project(armajitto VERSION 1.0.0)

# CMake project structured adapted from Alex Reinking's SharedStaticStarter
# GitHub: https://github.com/alexreinking/SharedStaticStarter
# Blog: https://alexreinking.com/blog/building-a-dual-shared-and-static-library-with-cmake.html

## Determine if this is a top-level build or included as a subproject
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)

option(ARMAJITTO_USE_VTUNE "Use VTune JIT Profiling API if available" OFF)

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
    include/armajitto/version.hpp
    include/armajitto/core/context.hpp
    include/armajitto/core/memory_map.hpp
    include/armajitto/core/options.hpp
    include/armajitto/core/recompiler.hpp
    include/armajitto/core/specification.hpp
    include/armajitto/core/system_interface.hpp
    include/armajitto/defs/cpu_arch.hpp
    include/armajitto/defs/cpu_model.hpp
    include/armajitto/defs/memory_access.hpp
    include/armajitto/guest/arm/cop_register.hpp
    include/armajitto/guest/arm/coprocessor.hpp
    include/armajitto/guest/arm/exceptions.hpp
    include/armajitto/guest/arm/exec_state.hpp
    include/armajitto/guest/arm/gpr.hpp
    include/armajitto/guest/arm/mode.hpp
    include/armajitto/guest/arm/psr.hpp
    include/armajitto/guest/arm/state.hpp
    include/armajitto/guest/arm/coprocessors/coproc_14_debug_dummy.hpp
    include/armajitto/guest/arm/coprocessors/coproc_15_sys_control.hpp
    include/armajitto/guest/arm/coprocessors/coproc_null.hpp
    include/armajitto/guest/arm/coprocessors/cp15/cp15_cache.hpp
    include/armajitto/guest/arm/coprocessors/cp15/cp15_control.hpp
    include/armajitto/guest/arm/coprocessors/cp15/cp15_defs.hpp
    include/armajitto/guest/arm/coprocessors/cp15/cp15_id.hpp
    include/armajitto/guest/arm/coprocessors/cp15/cp15_pu.hpp
    include/armajitto/guest/arm/coprocessors/cp15/cp15_tcm.hpp

    src/core/allocator.cpp
    src/core/allocator.hpp
    src/core/context.cpp
    src/core/location_ref.hpp
    src/core/memory_map.cpp
    src/core/memory_map_host_access.hpp
    src/core/memory_map_impl.hpp
    src/core/recompiler.cpp
    src/guest/arm/arithmetic.hpp
    src/guest/arm/exception_vectors.hpp
    src/guest/arm/flags.hpp
    src/guest/arm/gpr.cpp
    src/guest/arm/instructions.hpp
    src/guest/arm/mode.cpp
    src/guest/arm/mode_utils.hpp
    src/guest/arm/state.cpp
    src/guest/arm/state_offsets.hpp
    src/guest/arm/coprocessors/coproc_15_sys_control.cpp
    src/guest/arm/coprocessors/cp15_host_access.hpp
    src/guest/arm/coprocessors/cp15/cp15_cache.cpp
    src/guest/arm/coprocessors/cp15/cp15_tcm.cpp
    src/host/block_cache.hpp
    src/host/host.hpp
    src/host/host_code.hpp
    src/host/x86_64/abi.hpp
    src/host/x86_64/cpuid.cpp
    src/host/x86_64/cpuid.hpp
    src/host/x86_64/reg_alloc.cpp
    src/host/x86_64/reg_alloc.hpp
    src/host/x86_64/vtune.hpp
    src/host/x86_64/x86_64_compiled_code.hpp
    src/host/x86_64/x86_64_compiler.cpp
    src/host/x86_64/x86_64_compiler.hpp
    src/host/x86_64/x86_64_flags.hpp
    src/host/x86_64/x86_64_host.cpp
    src/host/x86_64/x86_64_host.hpp
    src/host/x86_64/x86_64_type_traits.hpp
    src/ir/basic_block.cpp
    src/ir/basic_block.hpp
    src/ir/emitter.cpp
    src/ir/emitter.hpp
    src/ir/ir_ops.hpp
    src/ir/optimizer.cpp
    src/ir/optimizer.hpp
    src/ir/translator.cpp
    src/ir/translator.hpp
    src/ir/var_lifetime.cpp
    src/ir/var_lifetime.hpp
    src/ir/verifier.hpp
    src/ir/defs/arguments.hpp
    src/ir/defs/memory_access.hpp
    src/ir/defs/opcode_types.hpp
    src/ir/defs/variable.hpp
    src/ir/ops/ir_ops_base.hpp
    src/ir/ops/ir_ops_visitor.hpp
    src/ir/ops/impl/ir_alu_ops.hpp
    src/ir/ops/impl/ir_branch_ops.hpp
    src/ir/ops/impl/ir_cop_ops.hpp
    src/ir/ops/impl/ir_flag_ops.hpp
    src/ir/ops/impl/ir_mem_ops.hpp
    src/ir/ops/impl/ir_misc_ops.hpp
    src/ir/ops/impl/ir_reg_ops.hpp
    src/ir/optimizer/arithmetic_ops_coalescence.cpp
    src/ir/optimizer/arithmetic_ops_coalescence.hpp
    src/ir/optimizer/bitwise_ops_coalescence.cpp
    src/ir/optimizer/bitwise_ops_coalescence.hpp
    src/ir/optimizer/const_propagation.cpp
    src/ir/optimizer/const_propagation.hpp
    src/ir/optimizer/dead_flag_value_store_elimination.cpp
    src/ir/optimizer/dead_flag_value_store_elimination.hpp
    src/ir/optimizer/dead_gpr_store_elimination.cpp
    src/ir/optimizer/dead_gpr_store_elimination.hpp
    src/ir/optimizer/dead_host_flag_store_elimination.cpp
    src/ir/optimizer/dead_host_flag_store_elimination.hpp
    src/ir/optimizer/dead_reg_store_elimination.cpp
    src/ir/optimizer/dead_reg_store_elimination.hpp
    src/ir/optimizer/dead_store_elimination_base.cpp
    src/ir/optimizer/dead_store_elimination_base.hpp
    src/ir/optimizer/dead_var_store_elimination.cpp
    src/ir/optimizer/dead_var_store_elimination.hpp
    src/ir/optimizer/host_flags_ops_coalescence.cpp
    src/ir/optimizer/host_flags_ops_coalescence.hpp
    src/ir/optimizer/optimizer_pass_base.cpp
    src/ir/optimizer/optimizer_pass_base.hpp
    src/ir/optimizer/common/host_flags_tracking.cpp
    src/ir/optimizer/common/host_flags_tracking.hpp
    src/ir/optimizer/common/var_subst.cpp
    src/ir/optimizer/common/var_subst.hpp
    src/ir/translator/decode_arm.hpp
    src/ir/translator/decode_thumb.hpp
    src/util/bitmask_enum.hpp
    src/util/bit_ops.hpp
    src/util/layered_memory_map.hpp
    src/util/noitree.hpp
    src/util/pointer_cast.hpp
    src/util/scope_guard.hpp
    src/util/type_traits.hpp
    src/util/unreachable.hpp
    src/util/unsafe_circular_buffer.hpp
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

add_subdirectory(vendor)

## Add xbyak
# TODO: include on x86_64 systems only
target_link_libraries(armajitto PUBLIC xbyak)

## Add VTune
# TODO: include on x86_64 systems only
include(cmake/FindVTune.cmake)
if (VTune_FOUND AND ARMAJITTO_USE_VTUNE)
  message(STATUS "Adding VTune JIT Profiling API from ${VTune_LIBRARIES}")
  target_include_directories(armajitto PRIVATE ${VTune_INCLUDE_DIRS})
  target_link_libraries(armajitto PRIVATE ${VTune_LIBRARIES})
  target_compile_definitions(armajitto PRIVATE ARMAJITTO_USE_VTUNE=1)
endif()

## Build precompiled header
target_precompile_headers(armajitto
    PUBLIC
        <algorithm>
        <array>
        <bit>
        <bitset>
        <cassert>
        <concepts>
        <cstddef>
        <cstdint>
        <cstdlib>
        <deque>
        <iomanip>
        <limits>
        <memory>
        <memory_resource>
        <optional>
        <sstream>
        <string>
        <tuple>
        <type_traits>
        <unordered_map>
        <utility>
        <vector>
)

## Define version string and numbers as compiler macros
target_compile_definitions(armajitto PUBLIC ARMAJITTO_VERSION="${armajitto_VERSION}")
target_compile_definitions(armajitto PUBLIC ARMAJITTO_VERSION_MAJOR=${armajitto_VERSION_MAJOR}u)
target_compile_definitions(armajitto PUBLIC ARMAJITTO_VERSION_MINOR=${armajitto_VERSION_MINOR}u)
target_compile_definitions(armajitto PUBLIC ARMAJITTO_VERSION_PATCH=${armajitto_VERSION_PATCH}u)

######### TEMPORARY #########
if(is_top_level)
    ## Add a test executable to aid development
    add_executable(armajitto-test
        test/test.cpp
    )
    add_executable(armajitto::armajitto-test ALIAS armajitto-test)
    set_target_properties(armajitto-test PROPERTIES
                          VERSION ${armajitto_VERSION}
                          SOVERSION ${armajitto_VERSION_MAJOR})
    if(UNIX OR MINGW)
        include(CMakeModules/FindSDL2.cmake)
        find_package(SDL2 REQUIRED)
        target_link_libraries(armajitto-test PRIVATE ${SDL2_LIBRARY})
        target_include_directories(armajitto-test PRIVATE ${SDL2_INCLUDE_DIR})
    else()
        find_package(SDL2 CONFIG REQUIRED)
        target_link_libraries(armajitto-test PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static)
    endif()
    target_link_libraries(armajitto-test PRIVATE armajitto)
    target_compile_features(armajitto-test PUBLIC cxx_std_20)

    ## Add the fuzzer project
    add_executable(armajitto-fuzzer
        fuzzer/main.cpp
        fuzzer/interp.cpp
        fuzzer/interp.hpp
        fuzzer/system.hpp
        
        fuzzer/interp/arm7tdmi.hpp
        fuzzer/interp/arm946es.hpp
        fuzzer/interp/arm/arithmetic.hpp
        fuzzer/interp/arm/conditions.hpp
        fuzzer/interp/arm/exceptions.hpp
        fuzzer/interp/arm/registers.hpp
    )
    add_executable(armajitto::armajitto-fuzzer ALIAS armajitto-fuzzer)
    target_link_libraries(armajitto-fuzzer PRIVATE armajitto)
    set_target_properties(armajitto-fuzzer PROPERTIES
                          VERSION ${armajitto_VERSION}
                          SOVERSION ${armajitto_VERSION_MAJOR})
    target_compile_features(armajitto-fuzzer PUBLIC cxx_std_20)
endif()
######### TEMPORARY #########

## Configure Visual Studio solution
if (MSVC)
    include(cmake/VSHelpers.cmake)

    ######### TEMPORARY #########
    ## Configure startup project
    if(is_top_level)
        set_property(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" PROPERTY VS_STARTUP_PROJECT armajitto-test)
    endif()

    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    ## Configure Visual Studio filters and debugging
    vs_set_filters(TARGET armajitto)
    set_target_properties(armajitto PROPERTIES FOLDER "armajitto")
    
    if(is_top_level)
        vs_set_filters(TARGET armajitto-test)
        set_target_properties(armajitto-test PROPERTIES FOLDER "armajitto")

        vs_set_filters(TARGET armajitto-fuzzer)
        set_target_properties(armajitto-fuzzer PROPERTIES FOLDER "armajitto")
    endif()
endif ()

## Generate the export header for armajitto and attach it to the target
include(GenerateExportHeader)
generate_export_header(armajitto EXPORT_FILE_NAME include/armajitto/export.h)
target_compile_definitions(armajitto PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:armajitto_STATIC_DEFINE>")
target_include_directories(armajitto PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

## Include the install rules if the user wanted them (included by default when top-level)
option(armajitto_INCLUDE_PACKAGING "Include packaging rules for armajitto" "${is_top_level}")
if (armajitto_INCLUDE_PACKAGING)
    add_subdirectory(packaging)
endif ()
