add_executable(tinexus-login
    main.cpp
    pam_authenticator.cpp
    session_launcher.cpp
    login_model.cpp
    login_view.cpp
    login_controller.cpp
)

target_include_directories(tinexus-login PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-login PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-login RUNTIME DESTINATION bin)
