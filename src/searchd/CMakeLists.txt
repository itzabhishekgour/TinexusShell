add_executable(tinexus-searchd
    session.cpp
    normalizer.cpp
    intent_detector.cpp
    cache.cpp
    ranking_pipeline.cpp
    provider_manager.cpp
    providers/app_provider.cpp
    providers/calculator_provider.cpp
    providers/system_provider.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/ram_snapshot.cpp
    main.cpp
)

target_include_directories(tinexus-searchd PRIVATE
    include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
)

target_link_libraries(tinexus-searchd PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)
