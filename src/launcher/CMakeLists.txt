add_executable(tinexus-launcher
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/indexer/desktop_parser.cpp
)

target_include_directories(tinexus-launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/indexer/include
)

target_link_libraries(tinexus-launcher PRIVATE
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-launcher RUNTIME DESTINATION bin)
