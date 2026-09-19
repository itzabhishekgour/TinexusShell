cmake_minimum_required(VERSION 3.28)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
    WaylandClient
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

add_executable(tinexus-files
    main.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)

target_include_directories(tinexus-files PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)

target_link_libraries(tinexus-files PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::WaylandClient
)

set_target_properties(tinexus-files PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS tinexus-files RUNTIME DESTINATION bin)

add_executable(test-files-live
    test_files_live.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-files-live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-files-live PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-files-live PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Phase 1 Gate Test: Sort-by-column + Tag apply/persist ──────────────────
add_executable(test-phase1-sort-tag
    test_phase1_sort_tag.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-phase1-sort-tag PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-phase1-sort-tag PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-phase1-sort-tag PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Phase 2A Gate Test: Async Streaming File Engine & Non-Blocking GUI ─────
add_executable(test-phase2-async
    test_phase2_async.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-phase2-async PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-phase2-async PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-phase2-async PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Phase 2B Gate Test: Interactive Conflict Resolution & Modal Dialog ─────
add_executable(test-phase2b-conflict
    test_phase2b_conflict.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-phase2b-conflict PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-phase2b-conflict PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-phase2b-conflict PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Phase 3 Gate Test: Instant Search & Interactive Address Bar ─────────────
add_executable(test-phase3-search
    test_phase3_search.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-phase3-search PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-phase3-search PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-phase3-search PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Phase 4 Gate Test: Multi-Tab Navigation ─────────────────────────────────
add_executable(test-phase4-tabs
    test_phase4_tabs.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-phase4-tabs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-phase4-tabs PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-phase4-tabs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Phase 5 Gate Test: Multi-Select & Batch Operations ──────────────────────
add_executable(test-phase5-multiselect
    test_phase5_multiselect.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-phase5-multiselect PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-phase5-multiselect PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-phase5-multiselect PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Phase 6 Gate Test: inotify Live File Watcher ────────────────────────────
add_executable(test-phase6-watcher
    test_phase6_watcher.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-phase6-watcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-phase6-watcher PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-phase6-watcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Phase 7 Gate Test: Keyboard Shortcuts Verification ──────────────────────
add_executable(test-phase7-shortcuts
    test_phase7_shortcuts.cpp
    FilesBridge.cpp
    FilesBridge.hpp
    TagManager.cpp
    trash_manager.cpp
    file_model.cpp
    column_view_model.cpp
    file_operations.cpp
)
target_include_directories(test-phase7-shortcuts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(test-phase7-shortcuts PRIVATE
    tinexus_common
    tinexus_warnings
    Qt6::Core
    Qt6::Gui
)
set_target_properties(test-phase7-shortcuts PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")



