add_executable(tinexus-shell
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
)

target_include_directories(tinexus-shell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
    ${CMAKE_SOURCE_DIR}/src/ipcd/include
)

target_link_libraries(tinexus-shell PRIVATE
    tinexus_common
    txui
    tinexus_protocols_client
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-shell RUNTIME DESTINATION bin)
