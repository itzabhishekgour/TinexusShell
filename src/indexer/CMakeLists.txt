find_package(SQLite3 REQUIRED)

add_executable(tinexus-indexerd
    desktop_parser.cpp
    ram_snapshot.cpp
    index_db.cpp
    fs_watcher.cpp
    main.cpp
)

target_include_directories(tinexus-indexerd PRIVATE
    include
)
set_target_properties(tinexus-indexerd PROPERTIES NO_SYSTEM_FROM_IMPORTED ON)

target_link_libraries(tinexus-indexerd PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    SQLite::SQLite3
)
