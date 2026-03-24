add_library(miem-fortran
  util.F90
  emis_state.F90
  miem.F90
)

target_link_libraries(miem-fortran PUBLIC miem)

# Place .mod files in the build directory
set_target_properties(miem-fortran PROPERTIES
  Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mod
)

target_include_directories(miem-fortran
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/mod>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS miem-fortran
  EXPORT MIEMTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mod/
  DESTINATION include
)
