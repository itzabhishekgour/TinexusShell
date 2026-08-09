content = '''add_executable(tinexus-shell
    main.cpp
    \/src/indexer/desktop_parser.cpp
)

target_include_directories(tinexus-shell PRIVATE
    \/include
    \/src/indexer/include
)

target_link_libraries(tinexus-shell PRIVATE
    tinexus_common
    txui
    tinexus_protocols_client
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-shell RUNTIME DESTINATION bin)
'''
with open('src/shell/CMakeLists.txt', 'w') as f:
    f.write(content)
