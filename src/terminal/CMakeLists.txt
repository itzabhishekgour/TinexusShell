find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(VTERM QUIET IMPORTED_TARGET vterm)
endif()

if(VTERM_FOUND)
    message(STATUS "tinexus-terminal: Found system libvterm via pkg-config")
else()
    message(STATUS "tinexus-terminal: libvterm not found on system, falling back to FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        vterm
        GIT_REPOSITORY https://github.com/neovim/libvterm.git
        GIT_TAG nvim
    )
    FetchContent_Populate(vterm)
    
    file(GLOB VTERM_SOURCES "${vterm_SOURCE_DIR}/src/*.c")
    add_library(vterm STATIC ${VTERM_SOURCES})
    target_include_directories(vterm PUBLIC "${vterm_SOURCE_DIR}/include")
    
    # Needs -std=c99
    set_target_properties(vterm PROPERTIES C_STANDARD 99)
endif()

add_executable(tinexus-terminal
    main.cpp
    pty_process.cpp
    TerminalEmulator.cpp
    TerminalBridge.cpp
    TerminalBridge.hpp
    TerminalCanvas.cpp
    TerminalCanvas.hpp
)

target_include_directories(tinexus-terminal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-terminal PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
)

if(VTERM_FOUND)
    target_link_libraries(tinexus-terminal PRIVATE PkgConfig::VTERM)
else()
    target_link_libraries(tinexus-terminal PRIVATE vterm)
endif()

install(TARGETS tinexus-terminal RUNTIME DESTINATION bin)

add_executable(test-terminal-pty-e2e
    ${CMAKE_SOURCE_DIR}/tools/test_terminal_pty_e2e.cpp
    pty_process.cpp
    TerminalEmulator.cpp
)
target_include_directories(test-terminal-pty-e2e PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test-terminal-pty-e2e PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)
if(VTERM_FOUND)
    target_link_libraries(test-terminal-pty-e2e PRIVATE PkgConfig::VTERM)
else()
    target_link_libraries(test-terminal-pty-e2e PRIVATE vterm)
endif()

# Legacy tests that depend on txui
if(ENABLE_LEGACY_TXUI)
    add_executable(test-terminal-render
        ../../tests/unit/test_terminal_render.cpp
        pty_process.cpp
        TerminalEmulator.cpp
        TerminalWidget.cpp
    )
    target_include_directories(test-terminal-render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(test-terminal-render PRIVATE txui tinexus-sdk tinexus_common tinexus_warnings tinexus_sanitizers)
    if(VTERM_FOUND)
        target_link_libraries(test-terminal-render PRIVATE PkgConfig::VTERM)
    else()
        target_link_libraries(test-terminal-render PRIVATE vterm)
    endif()

    add_executable(test-lru-cache ../../tests/unit/test_lru_cache.cpp TerminalEmulator.cpp TerminalWidget.cpp pty_process.cpp)
    target_include_directories(test-lru-cache PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(test-lru-cache PRIVATE txui tinexus-sdk tinexus_common vterm)

    add_executable(test-scrollback
        ../../tests/unit/test_scrollback.cpp
        pty_process.cpp
        TerminalEmulator.cpp
        TerminalWidget.cpp
    )
    target_include_directories(test-scrollback PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(test-scrollback PRIVATE txui tinexus-sdk tinexus_common tinexus_warnings tinexus_sanitizers)
    if(VTERM_FOUND)
        target_link_libraries(test-scrollback PRIVATE PkgConfig::VTERM)
    else()
        target_link_libraries(test-scrollback PRIVATE vterm)
    endif()

    add_executable(test-sb-cap
        ../../tests/unit/test_sb_cap.cpp
        pty_process.cpp
        TerminalEmulator.cpp
        TerminalWidget.cpp
    )
    target_include_directories(test-sb-cap PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(test-sb-cap PRIVATE txui tinexus-sdk tinexus_common tinexus_warnings tinexus_sanitizers)
    if(VTERM_FOUND)
        target_link_libraries(test-sb-cap PRIVATE PkgConfig::VTERM)
    else()
        target_link_libraries(test-sb-cap PRIVATE vterm)
    endif()

    add_executable(test-visual-polish
        ../../tests/unit/test_visual_polish.cpp
        pty_process.cpp
        TerminalEmulator.cpp
        TerminalWidget.cpp
    )
    target_include_directories(test-visual-polish PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(test-visual-polish PRIVATE txui tinexus-sdk tinexus_common tinexus_warnings tinexus_sanitizers)
    if(VTERM_FOUND)
        target_link_libraries(test-visual-polish PRIVATE PkgConfig::VTERM)
    else()
        target_link_libraries(test-visual-polish PRIVATE vterm)
    endif()
endif()
