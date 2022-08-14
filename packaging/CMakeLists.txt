include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (NOT DEFINED armajitto_INSTALL_CMAKEDIR)
   set(armajitto_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/armajitto"
       CACHE STRING "Path to armajitto CMake files")
endif ()

install(TARGETS armajitto EXPORT armajitto_Targets
        RUNTIME COMPONENT armajitto_Runtime
        LIBRARY COMPONENT armajitto_Runtime
        NAMELINK_COMPONENT armajitto_Development
        ARCHIVE COMPONENT armajitto_Development
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(DIRECTORY "${armajitto_SOURCE_DIR}/include/"
        TYPE INCLUDE
        COMPONENT armajitto_Development)

install(FILES "${armajitto_BINARY_DIR}/include/armajitto/export.h"
        DESTINATION include/armajitto/
        COMPONENT armajitto_Development
)

if (BUILD_SHARED_LIBS)
    set(type shared)
else ()
    set(type static)
endif ()

install(EXPORT armajitto_Targets
        DESTINATION "${armajitto_INSTALL_CMAKEDIR}"
        NAMESPACE armajitto::
        FILE armajitto-${type}-targets.cmake
        COMPONENT armajitto_Development)

write_basic_package_version_file(
    armajittoConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/armajittoConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/armajittoConfigVersion.cmake"
        DESTINATION "${armajitto_INSTALL_CMAKEDIR}"
        COMPONENT armajitto_Development)

# TODO: add additional CPack variables here
include(CPack)
