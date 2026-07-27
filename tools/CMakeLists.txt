add_executable(tinexus-search-bench
    tinexus-search-bench.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/ram_snapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/normalizer.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/intent_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/cache.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/ranking_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/provider_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/providers/app_provider.cpp
    ${CMAKE_SOURCE_DIR}/src/searchd/session.cpp
)

target_include_directories(tinexus-search-bench PRIVATE
    ${CMAKE_SOURCE_DIR}/src/indexer/include
    ${CMAKE_SOURCE_DIR}/src/searchd/include
)

target_link_libraries(tinexus-search-bench PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

add_subdirectory(tinexusctl)
add_subdirectory(tinexus-comp-inspector)
add_subdirectory(tinexus-drm-info)
