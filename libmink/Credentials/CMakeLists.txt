project(minkcredentials
    VERSION 1.0.0
    LANGUAGES C
)

add_compile_options(
	-Wstrict-prototypes -Wmissing-prototypes -Wbad-function-cast
	-Wswitch-default
)

# -----------------------------------------------------------------------------
# cc_library_headers { name: "credentialsHeaders", ... }
# -----------------------------------------------------------------------------
add_library(credentialsHeaders INTERFACE)
target_include_directories(credentialsHeaders INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# -----------------------------------------------------------------------------
# cc_library_static { name: "libminkcredentials", ... }
# -----------------------------------------------------------------------------
add_library(libminkcredentials STATIC
    src/osIndCredentials.c
)

# Compiler flags
# cflags: ["-D_ANDROID_"] + ["-fexceptions"]
target_compile_definitions(libminkcredentials PRIVATE _ANDROID_)
target_compile_options(libminkcredentials PRIVATE -fexceptions)

# Include directories
# local_include_dirs: ["inc"]
target_include_directories(libminkcredentials PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# Dependencies
# header_libs: ["mink_headers", "qcbor_headers", "minksocket_headers"]
# shared_libs: ["liblog", "libutils", "libqcbor", "libssl", "libcrypto"]
target_link_libraries(libminkcredentials PRIVATE
    # Header libraries (assuming these targets exist)
    PRIVATE minkadaptor
    mink_headers
    qcbor_headers
    minksocket_headers

    # Shared libraries
    liblog
    libutils
    libqcbor
    libssl
    libcrypto
)

# Link headers interface
target_link_libraries(libminkcredentials PUBLIC credentialsHeaders)

# Installation (optional, mimicking vendor: true)
# install(TARGETS libminkcredentials ARCHIVE DESTINATION lib)
