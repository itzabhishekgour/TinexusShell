add_executable(tinexus-notifications
    main.cpp
    notification_item.cpp
    dnd_manager.cpp
    notification_manager.cpp
    notification_server.cpp
    NotificationBubble.cpp
    NotificationStackWidget.cpp
    dbus_server.cpp
)

target_include_directories(tinexus-notifications PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/protocols
)

target_link_libraries(tinexus-notifications PRIVATE
    tinexus-sdk
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
    systemd
)

install(TARGETS tinexus-notifications RUNTIME DESTINATION bin)

# Standalone Visual & Timing Test Target
add_executable(test-notifications-render
    test_notifications_render.cpp
    NotificationBubble.cpp
    NotificationStackWidget.cpp
    notification_item.cpp
    notification_manager.cpp
    notification_server.cpp
    dnd_manager.cpp
    dbus_server.cpp
)

target_include_directories(test-notifications-render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/protocols
)

target_link_libraries(test-notifications-render PRIVATE
    tinexus-sdk
    tinexus_common
    txui
    tinexus_warnings
    tinexus_sanitizers
    systemd
)
