include(CMakePackageConfigHelpers)

set(MIEM_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/miem")

set(MIEM_EXPORT_TARGETS miem)

install(TARGETS ${MIEM_EXPORT_TARGETS}
  EXPORT miemTargets
  LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/miem"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
)

install(EXPORT miemTargets
  NAMESPACE musica::
  DESTINATION ${MIEM_INSTALL_CMAKEDIR}
  FILE miemTargets.cmake
)

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/miemConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/miemConfig.cmake"
  INSTALL_DESTINATION "${MIEM_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/miemConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${PROJECT_BINARY_DIR}/miemConfig.cmake"
  "${PROJECT_BINARY_DIR}/miemConfigVersion.cmake"
  "${PROJECT_SOURCE_DIR}/cmake/FindnetCDF.cmake"
  DESTINATION "${MIEM_INSTALL_CMAKEDIR}"
)
