add_executable(tinexus-files
    main.cpp
    file_model.cpp
    column_view_model.cpp
    trash_manager.cpp
    file_operations.cpp
    ui/ColumnWidget.cpp
    ui/ColumnBrowserWidget.cpp
    ui/FilesWindow.cpp
)

target_include_directories(tinexus-files PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-files PRIVATE
    tinexus-sdk
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-files RUNTIME DESTINATION bin)
