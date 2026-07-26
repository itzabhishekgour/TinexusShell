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

target_link_libraries(tinexus-indexerd PRIVATE
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    SQLite::SQLite3
)
