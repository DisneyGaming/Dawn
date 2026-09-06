if(NOT DEFINED SUNRISE_IDENTITY_HEADER OR SUNRISE_IDENTITY_HEADER STREQUAL "")
    message(FATAL_ERROR "SUNRISE_GENERATED_INCLUDE_DIR was not supplied")
endif()
if(NOT EXISTS "${SUNRISE_IDENTITY_HEADER}")
    message(FATAL_ERROR
        "Missing generated provenance header: ${SUNRISE_IDENTITY_HEADER}. "
        "Build through tools/build/build_release_candidate.ps1.")
endif()
