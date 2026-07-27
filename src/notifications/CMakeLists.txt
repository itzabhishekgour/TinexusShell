add_executable(tinexus-notifications
    main.cpp
    notification_item.cpp
    dnd_manager.cpp
    notification_manager.cpp
    notification_server.cpp
)

target_include_directories(tinexus-notifications PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-notifications PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-notifications RUNTIME DESTINATION bin)
