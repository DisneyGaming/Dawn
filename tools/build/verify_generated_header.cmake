if(NOT DEFINED DAWN_IDENTITY_HEADER OR DAWN_IDENTITY_HEADER STREQUAL "")
    message(FATAL_ERROR "DAWN_GENERATED_INCLUDE_DIR was not supplied")
endif()
if(NOT EXISTS "${DAWN_IDENTITY_HEADER}")
    message(FATAL_ERROR
        "Missing generated provenance header: ${DAWN_IDENTITY_HEADER}. "
        "Build through tools/build/build_release_candidate.ps1.")
endif()
