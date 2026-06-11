# Make udunits2's top-level CMakeLists FetchContent-friendly. We consume only
# the library, but its top-level build also (a) runs INCLUDE(CPack) reading
# ${CMAKE_SOURCE_DIR}/README and /COPYRIGHT — which under FetchContent point at
# the PARENT project (no such files) and abort the configure — and (b) attaches
# Texinfo documentation targets to ALL, which fail when makeinfo(1) is absent.
# Neutralize both. Run as the FetchContent PATCH_COMMAND with working directory
# = the udunits2 source dir. Idempotent via the marker below.
file(READ "CMakeLists.txt" _contents)
if(NOT _contents MATCHES "MIEM-patched")
  # (a) Drop the packaging include.
  string(REPLACE
    "INCLUDE(CPack)"
    "# INCLUDE(CPack)  # MIEM-patched: FetchContent has no parent README"
    _contents "${_contents}")
  # (b) Detach the doc targets from ALL (bracket-args keep ${file} literal).
  string(REPLACE
    [[add_custom_target(${file}_doc ALL]]
    [[add_custom_target(${file}_doc]]
    _contents "${_contents}")
  # (c) We only consume the library; udunits2's prog/ CLI uses getopt (absent
  #     on MSVC). Drop it so the FetchContent fallback stays portable.
  string(REPLACE
    "ADD_SUBDIRECTORY (prog)"
    "# ADD_SUBDIRECTORY (prog)  # MIEM-patched: library only"
    _contents "${_contents}")
  file(WRITE "CMakeLists.txt" "${_contents}")
endif()
