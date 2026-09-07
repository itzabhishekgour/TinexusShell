add_executable(tinexus-files
    main.cpp
    file_model.cpp
    column_view_model.cpp
    trash_manager.cpp
    file_operations.cpp
    ThumbnailCache.cpp
    TagManager.cpp
    ui/FileToolbarWidget.cpp
    ui/FileSidebarWidget.cpp
    ui/FileIconGridView.cpp
    ui/FileListView.cpp
    ui/FileGalleryView.cpp
    ui/FileInspectorWidget.cpp
    ui/FileQuickLookModal.cpp
    ui/FileContextMenu.cpp
    ui/ColumnWidget.cpp
    ui/ColumnBrowserWidget.cpp
)

target_include_directories(tinexus-files PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipcd/include
)

target_link_libraries(tinexus-files PRIVATE
    tinexus-sdk
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-files RUNTIME DESTINATION bin)

add_executable(test-files-render
    test_files_render.cpp
    file_model.cpp
    column_view_model.cpp
    trash_manager.cpp
    file_operations.cpp
    ThumbnailCache.cpp
    TagManager.cpp
    ui/FileToolbarWidget.cpp
    ui/FileSidebarWidget.cpp
    ui/FileIconGridView.cpp
    ui/FileListView.cpp
    ui/FileGalleryView.cpp
    ui/FileInspectorWidget.cpp
    ui/FileQuickLookModal.cpp
    ui/FileContextMenu.cpp
    ui/ColumnWidget.cpp
    ui/ColumnBrowserWidget.cpp
)

target_include_directories(test-files-render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipcd/include
)

target_link_libraries(test-files-render PRIVATE
    tinexus-sdk
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
)

set_target_properties(test-files-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

