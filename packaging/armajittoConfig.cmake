cmake_minimum_required(VERSION 3.19)

set(armajitto_known_comps static shared)
set(armajitto_comp_static NO)
set(armajitto_comp_shared NO)
foreach (armajitto_comp IN LISTS ${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS)
    if (armajitto_comp IN_LIST armajitto_known_comps)
        set(armajitto_comp_${armajitto_comp} YES)
    else ()
        set(${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE
            "armajitto does not recognize component `${armajitto_comp}`.")
        set(${CMAKE_FIND_PACKAGE_NAME}_FOUND FALSE)
        return()
    endif ()
endforeach ()

if (armajitto_comp_static AND armajitto_comp_shared)
    set(${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE
        "armajitto `static` and `shared` components are mutually exclusive.")
    set(${CMAKE_FIND_PACKAGE_NAME}_FOUND FALSE)
    return()
endif ()

set(armajitto_static_targets "${CMAKE_CURRENT_LIST_DIR}/armajitto-static-targets.cmake")
set(armajitto_shared_targets "${CMAKE_CURRENT_LIST_DIR}/armajitto-shared-targets.cmake")

macro(armajitto_load_targets type)
    if (NOT EXISTS "${armajitto_${type}_targets}")
        set(${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE
            "armajitto `${type}` libraries were requested but not found.")
        set(${CMAKE_FIND_PACKAGE_NAME}_FOUND FALSE)
        return()
    endif ()
    include("${armajitto_${type}_targets}")
endmacro()

if (armajitto_comp_static)
    armajitto_load_targets(static)
elseif (armajitto_comp_shared)
    armajitto_load_targets(shared)
elseif (DEFINED armajitto_SHARED_LIBS AND armajitto_SHARED_LIBS)
    armajitto_load_targets(shared)
elseif (DEFINED armajitto_SHARED_LIBS AND NOT armajitto_SHARED_LIBS)
    armajitto_load_targets(static)
elseif (BUILD_SHARED_LIBS)
    if (EXISTS "${armajitto_shared_targets}")
        armajitto_load_targets(shared)
    else ()
        armajitto_load_targets(static)
    endif ()
else ()
    if (EXISTS "${armajitto_static_targets}")
        armajitto_load_targets(static)
    else ()
        armajitto_load_targets(shared)
    endif ()
endif ()