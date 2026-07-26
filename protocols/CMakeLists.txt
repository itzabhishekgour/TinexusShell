add_library(tinexus_protocols STATIC)
target_include_directories(tinexus_protocols PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)

set(PROTOCOL_XMLS
    tinexus-global-shortcut-v1.xml
    tinexus-shell-helper-v1.xml
)

foreach(xml ${PROTOCOL_XMLS})
    get_filename_component(protocol_name ${xml} NAME_WE)
    
    set(header_file "${CMAKE_CURRENT_BINARY_DIR}/${protocol_name}-protocol.h")
    set(source_file "${CMAKE_CURRENT_BINARY_DIR}/${protocol_name}-protocol.c")
    
    add_custom_command(
        OUTPUT ${header_file}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} server-header ${CMAKE_CURRENT_SOURCE_DIR}/${xml} ${header_file}
        DEPENDS ${xml}
        VERBATIM
    )
    
    add_custom_command(
        OUTPUT ${source_file}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${CMAKE_CURRENT_SOURCE_DIR}/${xml} ${source_file}
        DEPENDS ${xml} ${header_file}
        VERBATIM
    )
    
    target_sources(tinexus_protocols PRIVATE ${source_file} ${header_file})
endforeach()

# Include wayland-server-core for wl_resource types
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server)
target_include_directories(tinexus_protocols PUBLIC ${WAYLAND_SERVER_INCLUDE_DIRS})
target_link_libraries(tinexus_protocols PUBLIC ${WAYLAND_SERVER_LIBRARIES})
