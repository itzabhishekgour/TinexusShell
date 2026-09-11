add_executable(tinexus-drm-info
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/comp/backend/drm_backend.cpp
)

target_include_directories(tinexus-drm-info PRIVATE
    ${CMAKE_SOURCE_DIR}/src/comp/backend/include
    ${CMAKE_SOURCE_DIR}/src/comp/window/include
)

target_link_libraries(tinexus-drm-info PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-drm-info RUNTIME DESTINATION bin)
