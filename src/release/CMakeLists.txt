add_executable(tinexus-release
    main.cpp
    checksum_generator.cpp
    signature_engine.cpp
    release_notes_gen.cpp
    release_manifest.cpp
    artifact_packager.cpp
    git_release.cpp
    release_pipeline.cpp
)

target_include_directories(tinexus-release PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-release PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-release RUNTIME DESTINATION bin)
